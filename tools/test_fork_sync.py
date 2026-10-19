import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).with_name('fork_sync.py')


class CandidateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.git('init', '-b', 'dev')
        self.git('config', 'user.email', 'test@example.invalid')
        self.git('config', 'user.name', 'Test')
        (self.repo / 'base').write_text('base')
        self.git('add', '.')
        self.git('commit', '-m', 'base')
        self.common = self.git('rev-parse', 'HEAD')
        self.git('checkout', '-b', 'upstream-sync')
        (self.repo / 'upstream').write_text('new')
        self.git('add', '.')
        self.git('commit', '-m', 'upstream')
        self.head = self.git('rev-parse', 'HEAD')
        self.git('checkout', 'dev')
        (self.repo / 'custom').write_text('custom')
        self.git('add', '.')
        self.git('commit', '-m', 'custom')
        self.base = self.git('rev-parse', 'HEAD')

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args], text=True, stderr=subprocess.DEVNULL).strip()

    def test_candidate_preserves_both_histories_and_is_reproducible(self):
        spec = importlib.util.spec_from_file_location('fork_sync', SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        first = module.candidate(self.repo, self.base, self.head)
        second = module.candidate(self.repo, self.base, self.head)
        self.assertEqual(first, second)
        self.assertEqual(self.git('show', f'{first}:custom'), 'custom')
        self.assertEqual(self.git('show', f'{first}:upstream'), 'new')
        self.assertEqual(self.git('show', '-s', '--format=%P', first), f'{self.base} {self.head}')
        self.assertEqual(self.git('rev-parse', 'HEAD'), self.base)
        self.assertEqual(self.git('status', '--porcelain'), '')

    def load(self):
        spec = importlib.util.spec_from_file_location('fork_sync', SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def origin(self):
        remote = self.repo / 'origin.git'
        subprocess.run(['git', 'init', '--bare', str(remote)], check=True, capture_output=True)
        self.git('remote', 'add', 'origin', str(remote))
        self.git('push', 'origin', 'dev', 'upstream-sync')
        return remote

    def pull(self):
        return {'state': 'open', 'base': {'sha': self.base, 'ref': 'dev'},
                'head': {'sha': self.head, 'ref': 'upstream-sync',
                         'repo': {'full_name': 'jinmo100/AyuGramDesktop'}}}

    def test_conflict_does_not_modify_worktree(self):
        module = self.load()
        self.git('checkout', 'upstream-sync')
        (self.repo / 'base').write_text('upstream conflict')
        self.git('commit', '-am', 'upstream conflict')
        head = self.git('rev-parse', 'HEAD')
        self.git('checkout', 'dev')
        (self.repo / 'base').write_text('custom conflict')
        self.git('commit', '-am', 'custom conflict')
        base = self.git('rev-parse', 'HEAD')
        with self.assertRaises(RuntimeError):
            module.candidate(self.repo, base, head)
        self.assertEqual(self.git('status', '--porcelain'), '')
        self.assertEqual(self.git('rev-parse', 'HEAD'), base)

    def test_finalization_pushes_only_the_checked_merge(self):
        module = self.load()
        self.origin()
        merged = module.candidate(self.repo, self.base, self.head)
        with patch.object(module, 'api', return_value=self.pull()):
            module.finalize(self.repo, self.base, self.head, merged, 1)
        self.assertEqual(module.refs(self.repo)['dev'], merged)

    def test_branch_movement_rejects_stale_result(self):
        module = self.load()
        self.origin()
        merged = module.candidate(self.repo, self.base, self.head)
        (self.repo / 'later').write_text('later')
        self.git('add', 'later')
        self.git('commit', '-m', 'later')
        self.git('push', 'origin', 'dev')
        with self.assertRaisesRegex(RuntimeError, 'moved'):
            module.finalize(self.repo, self.base, self.head, merged, 1)
        self.assertEqual(module.refs(self.repo)['dev'], self.git('rev-parse', 'HEAD'))

    def test_tampered_check_sha_is_rejected(self):
        module = self.load()
        self.origin()
        with patch.object(module, 'api', return_value=self.pull()):
            with self.assertRaisesRegex(RuntimeError, 'differs'):
                module.finalize(self.repo, self.base, self.head, self.common, 1)
        self.assertEqual(module.refs(self.repo)['dev'], self.base)

    def test_remote_race_cannot_overwrite_new_dev(self):
        module = self.load()
        self.origin()
        merged = module.candidate(self.repo, self.base, self.head)
        (self.repo / 'race').write_text('concurrent')
        self.git('add', 'race')
        self.git('commit', '-m', 'concurrent')
        concurrent = self.git('rev-parse', 'HEAD')
        real_candidate = module.candidate

        def racing_candidate(*args):
            self.git('push', 'origin', f'{concurrent}:refs/heads/dev')
            return real_candidate(*args)

        with patch.object(module, 'api', return_value=self.pull()), patch.object(module, 'candidate', side_effect=racing_candidate):
            with self.assertRaisesRegex(RuntimeError, 'failed'):
                module.finalize(self.repo, self.base, self.head, merged, 1)
        self.assertEqual(module.refs(self.repo)['dev'], concurrent)

    def test_existing_sync_pr_is_reused(self):
        module = self.load()
        remote = self.origin()
        upstream = self.repo / 'upstream.git'
        subprocess.run(['git', 'clone', '--bare', str(remote), str(upstream)], check=True, capture_output=True)
        self.git('--git-dir=' + str(upstream), 'update-ref', 'refs/heads/dev', self.head)
        with patch.object(module, 'UPSTREAM', str(upstream)), patch.object(module, 'api', return_value=[{'number': 42}]) as api:
            module.prepare(self.repo)
        self.assertEqual(api.call_count, 1)
        self.assertEqual(module.refs(self.repo)['upstream-sync'], self.head)

    def test_source_race_rejects_both_ref_updates(self):
        module = self.load()
        self.origin()
        merged = module.candidate(self.repo, self.base, self.head)
        self.git('checkout', 'upstream-sync')
        (self.repo / 'next-upstream').write_text('new head')
        self.git('add', 'next-upstream')
        self.git('commit', '-m', 'next upstream')
        newer = self.git('rev-parse', 'HEAD')
        real_candidate = module.candidate

        def racing_candidate(*args):
            self.git('push', 'origin', f'{newer}:refs/heads/upstream-sync')
            return real_candidate(*args)

        with patch.object(module, 'api', return_value=self.pull()), patch.object(module, 'candidate', side_effect=racing_candidate):
            with self.assertRaisesRegex(RuntimeError, 'failed'):
                module.finalize(self.repo, self.base, self.head, merged, 1)
        self.assertEqual(module.refs(self.repo)['dev'], self.base)
        self.assertEqual(module.refs(self.repo)['upstream-sync'], newer)

    def test_candidate_cannot_replace_its_own_gate(self):
        module = self.load()
        self.git('checkout', 'upstream-sync')
        (self.repo / 'tools').mkdir()
        (self.repo / 'tools/check-fork-sync.sh').write_text('exit 0\n')
        self.git('add', '.')
        self.git('commit', '-m', 'weaken validation')
        head = self.git('rev-parse', 'HEAD')
        merged = module.candidate(self.repo, self.base, head)
        with self.assertRaisesRegex(RuntimeError, 'policy changed'):
            module.verify_policy(self.repo, self.base, merged)

    def test_no_update_does_not_create_pr(self):
        module = self.load()
        remote = self.origin()
        self.git('push', 'origin', f'{module.candidate(self.repo, self.base, self.head)}:refs/heads/dev')
        with patch.object(module, 'UPSTREAM', str(remote)), patch.object(module, 'api') as api:
            module.prepare(self.repo)
        api.assert_not_called()

    def test_rewritten_upstream_stops_without_changing_fork(self):
        module = self.load()
        remote = self.origin()
        self.git('push', 'origin', f'{self.common}:refs/heads/dev', '--force')
        with patch.object(module, 'UPSTREAM', str(remote)), patch.object(module, 'api') as api:
            with self.assertRaisesRegex(RuntimeError, 'rewritten'):
                module.prepare(self.repo)
        api.assert_not_called()
        self.assertEqual(module.refs(self.repo)['upstream-sync'], self.head)


if __name__ == '__main__':
    unittest.main()
