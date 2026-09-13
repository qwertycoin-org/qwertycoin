#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <release-tag> <expected-revision>" >&2
  exit 64
fi
release_tag=$1
expected_revision=$2
[[ "$release_tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc[1-9][0-9]*)?$ ]] || { echo "invalid release tag" >&2; exit 64; }
[[ "$expected_revision" =~ ^[0-9a-f]{40}$ ]] || { echo "invalid expected revision" >&2; exit 64; }
[[ "$(git rev-parse HEAD)" == "$expected_revision" ]] || { echo "checkout revision mismatch" >&2; exit 1; }

remote_lines=$(git ls-remote --tags origin "refs/tags/$release_tag" "refs/tags/$release_tag^{}")
if [[ -n "$remote_lines" ]]; then
  git fetch --no-tags origin "refs/tags/$release_tag:refs/tags/$release_tag"
  resolved=$(git rev-parse "$release_tag^{}")
  [[ "$resolved" == "$expected_revision" ]] || {
    echo "existing tag $release_tag resolves to $resolved, not $expected_revision" >&2
    exit 1
  }
else
  git tag -a "$release_tag" -m "Local release build identity for $release_tag" "$expected_revision"
fi
[[ "$(git rev-parse "$release_tag^{}")" == "$expected_revision" ]] || exit 1
echo "Release tag state prepared locally: $release_tag -> $expected_revision"
