"""Trusted orchestration for a single upstream-sync PR; candidate code runs elsewhere."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = 'jinmo100/AyuGramDesktop'
UPSTREAM = 'https://github.com/AyuGram/AyuGramDesktop.git'
SHA = re.compile(r'[0-9a-f]{40}')


def run(*args, cwd=None, env=None, input=None):
    result = subprocess.run(args, cwd=cwd, env=env, input=input, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError(f'{args[0]} failed: {result.stderr.strip()}')
    return result.stdout.strip()


def git(repo, *args, **kwargs):
    return run('git', '-C', str(repo), *args, **kwargs)


def require_sha(value):
    if not SHA.fullmatch(value):
        raise ValueError('Expected a full lowercase commit SHA')
    return value


def ancestor(repo, older, newer):
    result = subprocess.run(['git', '-C', str(repo), 'merge-base', '--is-ancestor', older, newer],
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    if result.returncode not in (0, 1):
        raise RuntimeError('Cannot inspect commit ancestry')
    return result.returncode == 0


def candidate(repo, base, head):
    require_sha(base)
    require_sha(head)
    tree = git(repo, '-c', 'merge.renames=true', 'merge-tree', '--write-tree', base, head)
    if not SHA.fullmatch(tree):
        raise RuntimeError('Candidate merge did not produce exactly one tree')
    timestamp = max(int(git(repo, 'show', '-s', '--format=%ct', value)) for value in (base, head))
    env = dict(os.environ, GIT_AUTHOR_NAME='AyuGram upstream sync',
               GIT_AUTHOR_EMAIL='upstream-sync@users.noreply.github.com',
               GIT_COMMITTER_NAME='AyuGram upstream sync',
               GIT_COMMITTER_EMAIL='upstream-sync@users.noreply.github.com',
               GIT_AUTHOR_DATE=f'{timestamp} +0000', GIT_COMMITTER_DATE=f'{timestamp} +0000')
    return git(repo, '-c', 'commit.gpgsign=false', 'commit-tree', tree, '-p', base, '-p', head,
               input='Merge AyuGram upstream dev\n', env=env)


def verify_policy(repo, base, merged):
    changed = git(repo, 'diff', '--name-only', base, merged).splitlines()
    exact = {'package.json', 'package-lock.json', '.github/actionlint.yaml',
             '.github/workflows/sync-upstream.yml', 'tools/check-fork-sync.sh',
             'tools/setup-fork-sync.sh'}
    prefixes = ('tools/fork_', 'tools/test_fork_', '.agents/scripts/', '.github/workflows/',
                '.scratch/channel-top-posts/test_', '.scratch/channel-top-posts/layout_')
    protected = [path for path in changed if path in exact or path.startswith(prefixes)]
    if protected:
        raise RuntimeError('Validation policy changed; human review required: ' + ', '.join(protected))


def api(endpoint, method='GET', payload=None):
    args = ['gh', 'api', '--method', method, f'repos/{REPOSITORY}/{endpoint}']
    if payload is not None:
        args += ['--input', '-']
    output = run(*args, input=json.dumps(payload) if payload is not None else None)
    return json.loads(output) if output else None


def refs(repo):
    lines = git(repo, 'ls-remote', '--heads', 'origin', 'dev', 'upstream-sync').splitlines()
    return {line.split()[1].removeprefix('refs/heads/'): line.split()[0] for line in lines}


def output(values):
    print(json.dumps(values))
    if path := os.environ.get('GITHUB_OUTPUT'):
        with open(path, 'a', encoding='utf-8') as stream:
            for key, value in values.items():
                stream.write(f'{key}={str(value).lower() if isinstance(value, bool) else value}\n')


def prepare(repo):
    current = refs(repo)
    base = require_sha(current['dev'])
    git(repo, 'fetch', '--no-tags', 'origin', base)
    git(repo, 'fetch', '--no-tags', UPSTREAM, 'dev')
    head = require_sha(git(repo, 'rev-parse', 'FETCH_HEAD'))
    if 'upstream-sync' in current:
        old = current['upstream-sync']
        git(repo, 'fetch', '--no-tags', 'origin', old)
        if not ancestor(repo, old, head):
            raise RuntimeError('Upstream history was rewritten; human review required')
    if ancestor(repo, head, base):
        output({'changed': False})
        return
    git(repo, 'push', 'origin', f'{head}:refs/heads/upstream-sync')
    pulls = api('pulls?state=open&base=dev&head=jinmo100:upstream-sync')
    if len(pulls) > 1:
        raise RuntimeError('Multiple sync PRs found; human review required')
    body = ('Automatically maintained upstream sync. Checks cover merge conflicts, notes, '
            'workflow syntax and standalone logic tests; Windows builds run only before release. '
            'This PR and its automation are AI generated.')
    if pulls:
        pull = pulls[0]
    else:
        pull = api('pulls', 'POST', {'title': 'Sync AyuGram upstream dev', 'head': 'upstream-sync',
                                    'base': 'dev', 'body': body})
    output({'changed': True, 'base': base, 'head': head, 'pr': pull['number']})


def finalize(repo, base, head, expected, pr):
    current = refs(repo)
    if current.get('dev') != base or current.get('upstream-sync') != head:
        raise RuntimeError('Branches moved after validation; run sync again')
    pull = api(f'pulls/{pr}')
    if (pull['state'] != 'open' or pull['base']['sha'] != base or pull['head']['sha'] != head
            or pull['base']['ref'] != 'dev' or pull['head']['ref'] != 'upstream-sync'
            or pull['head']['repo']['full_name'] != REPOSITORY):
        raise RuntimeError('PR identity changed after validation')
    git(repo, 'fetch', '--no-tags', 'origin', base, head)
    merged = candidate(repo, base, head)
    verify_policy(repo, base, merged)
    if merged != require_sha(expected):
        raise RuntimeError('Recreated merge differs from checked candidate')
    # The lease makes a concurrently changed destination reject this fast-forward.
    git(repo, 'push', '--atomic', f'--force-with-lease=refs/heads/dev:{base}',
        f'--force-with-lease=refs/heads/upstream-sync:{head}', 'origin',
        f'{merged}:refs/heads/dev', f'{head}:refs/heads/upstream-sync')
    output({'merged': merged})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['prepare', 'candidate', 'finalize'])
    parser.add_argument('--repo', type=Path, default=Path('.'))
    parser.add_argument('--base', type=require_sha)
    parser.add_argument('--head', type=require_sha)
    parser.add_argument('--expected', type=require_sha)
    parser.add_argument('--pr', type=int)
    args = parser.parse_args()
    if os.environ.get('GITHUB_REPOSITORY', REPOSITORY) != REPOSITORY:
        raise ValueError('This automation is scoped to ' + REPOSITORY)
    if args.operation == 'prepare':
        prepare(args.repo)
    elif args.operation == 'candidate':
        if not args.base or not args.head:
            parser.error('--base and --head are required')
        merged = candidate(args.repo, args.base, args.head)
        verify_policy(args.repo, args.base, merged)
        output({'candidate': merged})
    else:
        if not all((args.base, args.head, args.expected, args.pr)):
            parser.error('--base, --head, --expected and --pr are required')
        finalize(args.repo, args.base, args.head, args.expected, args.pr)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
