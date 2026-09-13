#!/usr/bin/env bash
set -euo pipefail

expected_revision=${1:-${EXPECTED_REVISION:-}}
if [[ -n "$expected_revision" && ! "$expected_revision" =~ ^[0-9a-f]{40}$ ]]; then
  echo "expected revision must be a lowercase 40-character commit SHA" >&2
  exit 64
fi

actual_revision=$(git rev-parse HEAD)
if [[ -n "$expected_revision" && "$actual_revision" != "$expected_revision" ]]; then
  echo "source revision mismatch: expected $expected_revision, got $actual_revision" >&2
  exit 1
fi

dirty_paths=$(git status --porcelain=v1 --untracked-files=normal)
if [[ -n "$dirty_paths" ]]; then
  echo "tracked or untracked source paths changed:" >&2
  printf '%s\n' "$dirty_paths" >&2
  exit 1
fi

submodule_status=$(git submodule status --recursive)
if grep -Eq '^[+-U]' <<<"$submodule_status"; then
  echo "submodule checkout does not match the selected revision:" >&2
  printf '%s\n' "$submodule_status" >&2
  exit 1
fi

git submodule foreach --quiet --recursive '
  if ! git diff --quiet || ! git diff --cached --quiet || [[ -n "$(git ls-files --others --exclude-standard)" ]]; then
    echo "dirty submodule: $displaypath" >&2
    git status --short >&2
    exit 1
  fi
'

echo "Source tree verified clean at $actual_revision"
