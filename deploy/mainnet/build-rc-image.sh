#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

image_tag="${QWC_IMAGE_TAG:-qwertycoin-v2-node:v2.0.0-rc.1}"
nproc_value="${NPROC:-2}"

cd "$repo_root"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 69
fi

git_status="$(git status --short)"
if [[ -n "$git_status" && "${QWC_ALLOW_DIRTY_BUILD:-0}" != "1" ]]; then
  echo "refusing to build release-candidate image from a dirty worktree" >&2
  echo "$git_status" >&2
  echo "set QWC_ALLOW_DIRTY_BUILD=1 only for disposable local validation builds" >&2
  exit 70
fi

docker build \
  --build-arg "NPROC=$nproc_value" \
  -t "$image_tag" \
  -f Dockerfile \
  .

docker run --rm --entrypoint qwertycoind "$image_tag" --version
docker run --rm --entrypoint qwertycoin-wallet-cli "$image_tag" --version
docker run --rm --entrypoint qwertycoin-wallet-rpc "$image_tag" --version

cat <<EOF
Built Qwertycoin mainnet release-candidate image:

  $image_tag

Record the image id or registry digest before deploying it to seed nodes.
EOF
