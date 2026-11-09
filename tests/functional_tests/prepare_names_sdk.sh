#!/usr/bin/env bash
set -euo pipefail

source="${QWC_CONTRACTS_SDK_FLAKE:-github:exploshot/qwertycoin-contracts-sdk/c6a5317cbd5c1df64db4dd0ec6f6f5b8748fb56f}"
node="${QWC_NODE_FLAKE:-path:$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
result="$(nix build --no-link --print-out-paths --override-input node "$node" "$source#names-wasm" "$source#sdk")"
mapfile -t outputs <<< "$result"
test "${#outputs[@]}" -eq 2
printf '%s\n%s\n' "${outputs[0]}" "${outputs[1]}/qwc-names"
