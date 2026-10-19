#!/usr/bin/env python3
"""Offline release behavior tests; real Git and a stateful external gh stand-in."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fork_release as release

MOCK_GH = r'''#!/usr/bin/env python3
import hashlib, json, os, pathlib, sys
p = pathlib.Path(os.environ['MOCK_GH_STATE'])
s = json.loads(p.read_text())
a = sys.argv[1:]
s.setdefault('calls', []).append(a)
def done(value=None, status=0):
    p.write_text(json.dumps(s))
    if status:
        print('gh: request failed (HTTP ' + str(status) + ')', file=sys.stderr)
        sys.exit(1)
    print(json.dumps(value))
    sys.exit(0)
if a[:2] == ['release', 'upload']:
    s['uploads'] = s.get('uploads', 0) + 1
    if s.get('fail_upload') == s['uploads']: done(status=503)
    f = pathlib.Path(a[3])
    s['release']['assets'].append({'name': f.name, 'digest': 'sha256:' + hashlib.sha256(f.read_bytes()).hexdigest()})
    done({})
assert a[0] == 'api', a
endpoint = a[1].split('/AyuGramDesktop/', 1)[1]
method = a[a.index('--method') + 1] if '--method' in a else 'GET'
payload = json.load(sys.stdin) if '--input' in a else None
if endpoint.startswith('releases/tags/'):
    done(status=404)
if endpoint.startswith('git/ref/tags/'):
    tag = endpoint.split('tags/', 1)[1]
    target = s.get('tags', {}).get(tag)
    if target:
        if s.get('dirty_final') and s.get('release'):
            pathlib.Path(s['dirty_final']).write_text('changed before publication')
        done({'object': target if isinstance(target, dict) else {'type': 'commit', 'sha': target}})
    done(status=404)
if endpoint.startswith('git/tags/'):
    done({'object': {'type': 'commit', 'sha': s['peeled']}})
if endpoint.startswith('releases?'):
    assert '--paginate' in a and '--slurp' in a
    if s.get('lookup_error'): done(status=s['lookup_error'])
    done([[{'tag_name': 'v7.2.9-jinmo.2'}], [{'tag_name': 'v7.2.9-jinmo.5', 'draft': True}],
          [s['release']] if s.get('release') else []])
if endpoint.startswith('tags?'):
    assert '--paginate' in a and '--slurp' in a
    done([[], [{'name': 'v7.2.9-jinmo.8'}]])
if endpoint == 'releases' and method == 'POST':
    s['release'] = dict(payload, id=10, assets=[])
    if s.get('create_tag_on_draft'):
        s.setdefault('tags', {})[payload['tag_name']] = payload['target_commitish']
    done(s['release'])
if endpoint == 'git/refs' and method == 'POST':
    s.setdefault('tags', {})[payload['ref'].removeprefix('refs/tags/')] = payload['sha']
    done({'object': {'type': 'commit', 'sha': payload['sha']}})
if endpoint == 'releases/10' and method == 'PATCH':
    s['release'].update(payload)
    done(s['release'])
raise AssertionError(a)
'''


def git(cwd, *args):
    return subprocess.run(['git', *args], cwd=cwd, check=True, text=True,
                          capture_output=True).stdout.strip()


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        self.repo = self.tmp / 'repo'
        self.repo.mkdir()
        git(self.repo, 'init', '-b', 'dev')
        git(self.repo, 'config', 'user.email', 'test@example.invalid')
        git(self.repo, 'config', 'user.name', 'Release Test')
        (self.repo / 'Telegram/build').mkdir(parents=True)
        (self.repo / 'Telegram/build/version').write_text('AppVersionStrSmall 7.2.9\n')
        (self.repo / 'LICENSE').write_text('license\n')
        (self.repo / '.gitignore').write_text('out/\n')
        (self.repo / 'file').write_text('source\n')
        git(self.repo, 'add', '.')
        git(self.repo, 'commit', '-m', 'source')
        self.sha = git(self.repo, 'rev-parse', 'HEAD')
        self.bare = self.tmp / 'remote.git'
        git(self.tmp, 'init', '--bare', str(self.bare))
        self.url = 'https://github.com/jinmo100/AyuGramDesktop.git'
        git(self.repo, 'config', f'url.{self.bare}.insteadOf', self.url)
        git(self.repo, 'remote', 'add', 'origin', self.url)
        git(self.repo, 'push', 'origin', 'dev')
        self.exe = self.repo / 'out/Debug/AyuGram.exe'
        self.exe.parent.mkdir(parents=True)
        self.exe.write_bytes(b'unchanged incremental executable')
        os.utime(self.exe, (100, 100))
        self.cache = {'DESKTOP_APP_DISABLE_AUTOUPDATE': 'ON',
            'TDESKTOP_FORK_RELEASES_URL': release.RELEASES_URL,
            'CMAKE_HOME_DIRECTORY': str(self.repo), 'CMAKE_GENERATOR': 'Visual Studio 17 2022',
            'CMAKE_GENERATOR_PLATFORM': 'x64', 'CMAKE_GENERATOR_TOOLSET': 'version=14.44',
            'CMAKE_CONFIGURATION_TYPES': 'Debug;Release',
            'CMAKE_MSVC_RUNTIME_LIBRARY': 'MultiThreaded$<$<CONFIG:Debug>:Debug>'}
        compiler = self.repo / 'out/CMakeFiles/3.31/CMakeCXXCompiler.cmake'
        compiler.parent.mkdir(parents=True)
        compiler.write_text('set(CMAKE_CXX_COMPILER "E:/VS/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe")\n')
        self.write_cache()
        self.proof = self.tmp / 'proof.json'
        now = int(time.time())
        self.data = {'source_sha': self.sha, 'artifact': str(self.exe),
            'artifact_sha256': release.sha256(self.exe), 'build_succeeded': True,
            'layout_succeeded': True, 'process_checked': True, 'started_at': now - 5,
            'build_started_at': now - 4, 'build_finished_at': now - 2,
            'finished_at': now - 1, 'cache': dict(self.cache),
            'validation_config': {'msvc_version': '14.44.35207', 'qt_version': '5.15.19',
                                  'tbuild_root': 'E:/TBuild'}}
        self.write_proof()
        self.bin = self.tmp / 'bin'
        self.bin.mkdir()
        gh = self.bin / 'gh'
        gh.write_text(MOCK_GH)
        gh.chmod(0o755)
        self.state_path = self.tmp / 'gh-state.json'
        self.state_path.write_text('{}')
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ['PATH'],
                        MOCK_GH_STATE=str(self.state_path))
        self.tag = 'v7.2.9-jinmo.1'

    def write_cache(self):
        (self.repo / 'out/CMakeCache.txt').write_text(''.join(f'{k}:STRING={v}\n' for k, v in self.cache.items()))

    def write_proof(self):
        self.proof.write_text(json.dumps(self.data))

    def state(self):
        return json.loads(self.state_path.read_text())

    def set_state(self, **updates):
        state = self.state()
        state.update(updates)
        self.state_path.write_text(json.dumps(state))

    def cli(self, *extra, success=True):
        p = subprocess.run([sys.executable, str(Path(release.__file__).resolve()),
            '--repo', str(self.repo), '--source-sha', self.sha,
            '--build-manifest', str(self.proof), '--tag', self.tag, *extra],
            env=self.env, text=True, capture_output=True)
        self.assertEqual(p.returncode, 0 if success else 2, p.stderr)
        return p

    def test_successful_main_publication(self):
        self.cli()
        state = self.state()
        self.assertFalse(state['release']['draft'])
        self.assertTrue(state['release']['prerelease'])
        self.assertEqual(state['tags'][self.tag], self.sha)
        self.assertEqual(len(state['release']['assets']), 3)
        self.assertEqual(state['uploads'], 3)

    def test_server_created_draft_tag_is_not_created_twice(self):
        self.set_state(create_tag_on_draft=True)
        self.cli()
        state = self.state()
        self.assertFalse(state['release']['draft'])
        self.assertFalse(any('git/refs' in arg for call in state['calls'] for arg in call))

    def test_failure_stays_draft_and_resume_uses_identical_assets(self):
        self.set_state(fail_upload=2)
        self.cli(success=False)
        first = self.state()['release']['assets'][0]
        self.assertTrue(self.state()['release']['draft'])
        self.cli('--resume')
        state = self.state()
        self.assertFalse(state['release']['draft'])
        self.assertEqual(state['release']['assets'][0], first)
        self.assertEqual(state['uploads'], 4)

    def test_published_duplicate_refused(self):
        self.cli()
        before = self.state()['uploads']
        self.cli('--resume', success=False)
        self.assertEqual(self.state()['uploads'], before)

    def test_unknown_draft_null_target_and_extra_assets_refused(self):
        self.set_state(fail_upload=1)
        self.cli(success=False)
        original = self.state()['release']
        for change in ({'body': 'unknown owner'}, {'target_commitish': None},
                       {'assets': [{'name': 'unexpected', 'digest': 'sha256:abc'}]}):
            self.set_state(release=dict(original, **change))
            self.cli('--resume', success=False)
            self.assertTrue(self.state()['release']['draft'])

    def test_existing_tag_without_owned_draft_refused(self):
        self.set_state(tags={self.tag: self.sha})
        self.cli(success=False)
        self.assertNotIn('release', self.state())

    def test_annotated_tag_is_peeled_and_wrong_commit_refused(self):
        self.set_state(fail_upload=1)
        self.cli(success=False)
        annotated = {self.tag: {'type': 'tag', 'sha': 'a' * 40}}
        self.set_state(tags=annotated, peeled='b' * 40)
        self.cli('--resume', success=False)
        self.assertTrue(self.state()['release']['draft'])
        self.set_state(peeled=self.sha)
        self.cli('--resume')
        self.assertFalse(self.state()['release']['draft'])

    def test_submodule_dirty_wrong_and_uninitialized_refused(self):
        child = self.tmp / 'child'
        child.mkdir()
        git(child, 'init')
        git(child, 'config', 'user.email', 'test@example.invalid')
        git(child, 'config', 'user.name', 'Release Test')
        (child / 'file').write_text('child')
        git(child, 'add', '.')
        git(child, 'commit', '-m', 'child')
        git(self.repo, '-c', 'protocol.file.allow=always', 'submodule', 'add', str(child), 'sub')
        git(self.repo, 'commit', '-am', 'submodule')
        git(self.repo, 'push', 'origin', 'dev')
        source = git(self.repo, 'rev-parse', 'HEAD')
        self.assertEqual(release.preflight(self.repo, source), source)
        (self.repo / 'sub/file').write_text('dirty')
        with self.assertRaises(release.ReleaseError):
            release.preflight(self.repo, source)
        git(self.repo / 'sub', 'config', 'user.email', 'test@example.invalid')
        git(self.repo / 'sub', 'config', 'user.name', 'Release Test')
        git(self.repo / 'sub', 'commit', '-am', 'wrong commit')
        with self.assertRaises(release.ReleaseError):
            release.preflight(self.repo, source)
        git(self.repo, 'submodule', 'deinit', '-f', 'sub')
        with self.assertRaises(release.ReleaseError):
            release.preflight(self.repo, source)

    def test_api_errors_fail_closed(self):
        for status in (401, 403, 429, 500):
            self.set_state(lookup_error=status)
            self.cli(success=False)
            self.assertNotIn('release', self.state())

    def test_final_preflight_catches_source_change(self):
        self.set_state(dirty_final=str(self.repo / 'file'))
        self.cli(success=False)
        self.assertTrue(self.state()['release']['draft'])
        self.assertFalse(any('--method' in c and 'PATCH' in c for c in self.state()['calls']))

    def test_dirty_wrong_unpushed_and_spoof_source_refused_before_api(self):
        (self.repo / 'file').write_text('dirty')
        self.cli(success=False)
        (self.repo / 'file').write_text('source\n')
        self.sha = '0' * 40
        self.cli(success=False)
        self.sha = git(self.repo, 'rev-parse', 'HEAD')
        (self.repo / 'file').write_text('new source')
        git(self.repo, 'commit', '-am', 'unpushed')
        self.sha = git(self.repo, 'rev-parse', 'HEAD')
        self.cli(success=False)
        git(self.repo, 'push', 'origin', 'dev')
        git(self.repo, 'remote', 'set-url', 'origin', self.url + '.evil')
        self.cli(success=False)
        self.assertEqual(self.state(), {})

    def test_previously_pushed_source_remains_releasable_after_dev_advances(self):
        old_sha = self.sha
        (self.repo / 'file').write_text('later upstream sync')
        git(self.repo, 'commit', '-am', 'later sync')
        git(self.repo, 'push', 'origin', 'dev')
        git(self.repo, 'checkout', '--detach', old_sha)
        self.cli()
        self.assertEqual(self.state()['release']['target_commitish'], old_sha)

    def test_dirty_error_lists_status(self):
        (self.repo / 'file').write_text('dirty')
        result = self.cli(success=False)
        self.assertIn(' M file', result.stderr)
        self.assertEqual(self.state(), {})

    def test_supported_https_and_ssh_remote_forms(self):
        for url in sorted(release.ORIGINS):
            git(self.repo, 'config', '--add', f'url.{self.bare}.insteadOf', url)
            git(self.repo, 'remote', 'set-url', 'origin', url)
            self.assertEqual(release.preflight(self.repo, self.sha), self.sha)
        for url in ('https://evil/github.com/jinmo100/AyuGramDesktop',
                    'https://github.com/jinmo100/AyuGramDesktop-fake.git',
                    'https://github.com/jinmo100/AyuGramDesktop.git/extra'):
            git(self.repo, 'remote', 'set-url', 'origin', url)
            with self.assertRaises(release.ReleaseError):
                release.preflight(self.repo, self.sha)

    def test_bad_proof_and_actual_cache_refused(self):
        original = dict(self.data)
        for change in ({'finished_at': time.time() - 90000, 'started_at': time.time() - 90005},
                       {'started_at': time.time() + 100}, {'build_finished_at': time.time() + 100},
                       {'artifact_sha256': '0' * 64}, {'build_succeeded': False},
                       {'artifact': str(self.tmp / 'AyuGram.exe')}):
            self.data = dict(original, **change)
            self.write_proof()
            self.cli(success=False)
        self.data = original
        self.write_proof()
        self.cache['DESKTOP_APP_DISABLE_AUTOUPDATE'] = 'OFF'
        self.write_cache()
        self.cli(success=False)
        self.assertEqual(self.state(), {})

    def test_proof_cannot_claim_a_different_msvc_toolset(self):
        self.data['validation_config'] = {'msvc_version': '14.43', 'qt_version': '5.15.19',
                                          'tbuild_root': 'E:/TBuild'}
        self.write_proof()
        self.cli(success=False)
        self.assertEqual(self.state(), {})

    def test_unchanged_incremental_artifact_is_valid(self):
        data, artifact = release.load_manifest(self.proof, self.sha, self.repo)
        self.assertEqual(artifact, self.exe)
        self.assertEqual(data['artifact_sha256'], release.sha256(self.exe))

    def test_zip_is_reproducible_staged_and_allowlisted(self):
        roots = []
        self.addCleanup(lambda: [shutil.rmtree(p) for p in roots])
        digests = []
        for _ in range(2):
            root, assets = release.make_assets(self.repo, self.tag, self.data, self.exe, self.proof.parent)
            roots.append(root)
            self.assertEqual(root.parent, self.proof.parent)
            digests.append([release.sha256(a) for a in assets])
            with zipfile.ZipFile(assets[0]) as z:
                prefix = f'AyuGram-Jinmo-{self.tag}/'
                self.assertEqual(set(z.namelist()), {prefix + n for n in
                    ('AyuGram.exe', 'LICENSE', 'README.txt', 'TelegramForcePortable/')})
                self.assertEqual(z.read(prefix + 'AyuGram.exe'), self.exe.read_bytes())
                self.assertTrue(all(i.date_time == (1980, 1, 1, 0, 0, 0) for i in z.infolist()))
            os.utime(self.exe, None)
        self.assertEqual(*digests)
        (self.repo / 'LICENSE').unlink()
        with self.assertRaises(release.ReleaseError):
            release.make_assets(self.repo, self.tag, self.data, self.exe, self.proof.parent)

    def test_paginated_sequence_counts_drafts_and_tags(self):
        from unittest.mock import patch
        with patch.dict(os.environ, self.env):
            self.assertEqual(release.release_tag(self.repo, None), 'v7.2.9-jinmo.9')
        for tag in ('v7.2.9-jinmo.0', 'v7.2.9-jinmo.01', 'v7.2.8-jinmo.1'):
            with self.assertRaises(release.ReleaseError):
                release.release_tag(self.repo, tag)

    def test_git_worktree_and_publisher_lock(self):
        worktree = self.tmp / 'worktree'
        git(self.repo, 'worktree', 'add', '--detach', str(worktree), self.sha)
        self.assertTrue((worktree / '.git').is_file())
        with release.release_lock(worktree):
            with self.assertRaises(release.ReleaseError):
                with release.release_lock(worktree):
                    self.fail('lock allowed a second publisher')
        self.assertEqual(release.preflight(worktree, self.sha), self.sha)


if __name__ == '__main__':
    unittest.main()
