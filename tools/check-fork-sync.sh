#!/usr/bin/env bash
set -euo pipefail
: "${AGENT_NOTE_ARCHIVE_BASE_REF:?Set the pre-merge base commit}"
output_dir="$(mktemp -d)"
trap 'rm -rf "$output_dir"' EXIT
npm ci --ignore-scripts
npm run verify-notes
for source in .scratch/channel-top-posts/test_*_seams.cpp; do
    binary="$output_dir/$(basename "${source%.cpp}")"
    g++ -std=c++20 -O0 -UNDEBUG -include cstdint -include cctype -include ctime "$source" -o "$binary"
    "$binary"
done
python3 -m unittest discover -s tools -p 'test_fork_*.py'
actionlint -config-file .github/actionlint.yaml
