#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || ! -x "$1" ]]; then
  echo "usage: $0 <qwertycoind>" >&2
  exit 64
fi

daemon="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
source_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
state_dir="$(mktemp -d "${TMPDIR:-/tmp}/qwc-epose-public-options.XXXXXX")"
trap 'rm -rf -- "$state_dir"' EXIT

canonical_options=(
  epose-service
  epose-keystore
  epose-reward-address
  epose-host
  epose-port
  epose-discovery-endpoint
)
legacy_options=(
  epose-v2-service
  epose-v2-keystore
  epose-v2-reward-address
  epose-v2-endpoint-host
  epose-v2-endpoint-port
  epose-v2-discovery-endpoint
)

"$daemon" --help >"$state_dir/help.out" 2>"$state_dir/help.err"
for option in "${canonical_options[@]}"; do
  grep -q -- "--$option" "$state_dir/help.out"
done
for option in "${legacy_options[@]}"; do
  if grep -q -- "--$option" "$state_dir/help.out"; then
    echo "deprecated option is visible in help: --$option" >&2
    exit 1
  fi
done

legacy_keystore="$state_dir/legacy-keystore"
"$daemon" --help \
  --epose-v2-service \
  "--epose-v2-keystore=$legacy_keystore" \
  --epose-v2-reward-address=QWC_TEST_ADDRESS \
  --epose-v2-endpoint-host=service.example.org \
  --epose-v2-endpoint-port=8198 \
  --epose-v2-discovery-endpoint=http://one.example.org:8198 \
  --epose-v2-discovery-endpoint=http://two.example.org:8198 \
  >"$state_dir/legacy.out" 2>"$state_dir/legacy.err"
if [[ "$(grep -c '^Deprecated EPoSe option names were used;' "$state_dir/legacy.err")" -ne 1 ]]; then
  echo "legacy CLI warning was not emitted exactly once" >&2
  exit 1
fi
if grep -Fq "$legacy_keystore" "$state_dir/legacy.err"; then
  echo "legacy CLI warning disclosed an option value" >&2
  exit 1
fi

"$daemon" --help \
  --epose-service \
  "--epose-keystore=$state_dir/canonical-keystore" \
  --epose-reward-address=QWC_TEST_ADDRESS \
  --epose-host=service.example.org \
  --epose-port=8198 \
  --epose-discovery-endpoint=http://one.example.org:8198 \
  >"$state_dir/canonical.out" 2>"$state_dir/canonical.err"
if grep -q '^Deprecated EPoSe option names were used;' "$state_dir/canonical.err"; then
  echo "canonical CLI emitted a migration warning" >&2
  exit 1
fi

"$daemon" --help \
  --epose-host=service.example.org \
  --epose-v2-endpoint-host=service.example.org \
  >"$state_dir/equal.out" 2>"$state_dir/equal.err"

if "$daemon" --help \
    --epose-host=one.example.org \
    --epose-v2-endpoint-host=two.example.org \
    >"$state_dir/conflict.out" 2>"$state_dir/conflict.err"; then
  echo "conflicting CLI aliases were accepted" >&2
  exit 1
fi
grep -q "Conflicting EPoSe options '--epose-host'" "$state_dir/conflict.err"
if grep -Eq 'one\.example\.org|two\.example\.org' "$state_dir/conflict.err"; then
  echo "conflict error disclosed option values" >&2
  exit 1
fi

cat >"$state_dir/config-legacy.conf" <<'EOF'
epose-v2-service=true
epose-v2-keystore=/tmp/qwc-epose-config-keystore
epose-v2-reward-address=QWC_TEST_ADDRESS
epose-v2-endpoint-host=service.example.org
epose-v2-endpoint-port=8198
epose-v2-discovery-endpoint=http://one.example.org:8198
testnet=true
stagenet=true
EOF
if "$daemon" --config-file "$state_dir/config-legacy.conf" \
    >"$state_dir/config-legacy.out" 2>"$state_dir/config-legacy.err"; then
  echo "legacy config did not reach the deliberate network conflict" >&2
  exit 1
fi
if [[ "$(grep -c '^Deprecated EPoSe option names were used;' "$state_dir/config-legacy.err")" -ne 1 ]]; then
  echo "legacy config warning was not emitted exactly once" >&2
  exit 1
fi
grep -q "Can't specify more than one" "$state_dir/config-legacy.err"

cat >"$state_dir/config-conflict.conf" <<'EOF'
epose-host=one.example.org
epose-v2-endpoint-host=two.example.org
EOF
if "$daemon" --config-file "$state_dir/config-conflict.conf" \
    >"$state_dir/config-conflict.out" 2>"$state_dir/config-conflict.err"; then
  echo "conflicting config aliases were accepted" >&2
  exit 1
fi
grep -q "Conflicting EPoSe options '--epose-host'" "$state_dir/config-conflict.err"
if grep -Eq 'one\.example\.org|two\.example\.org' "$state_dir/config-conflict.err"; then
  echo "config conflict error disclosed an option value" >&2
  exit 1
fi

env -i PATH="$PATH" \
  QWC_EPOSE_HOST=service.example.org \
  QWC_EPOSE_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  sh -eu -c ". '$source_root/deploy/mainnet/epose-env-compat.sh';
    test \"\$QWC_EPOSE_KEYSTORE_PATH\" = /service-node/epose-v2-keystore;
    test \"\$QWC_EPOSE_PORT\" = 8198" \
  >"$state_dir/env-canonical.out" 2>"$state_dir/env-canonical.err"

env -i PATH="$PATH" \
  QWC_EPOSE_V2_ENDPOINT_HOST=service.example.org \
  QWC_EPOSE_V2_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  sh -eu -c ". '$source_root/deploy/mainnet/epose-env-compat.sh';
    test \"\$QWC_EPOSE_HOST\" = service.example.org;
    test \"\$QWC_EPOSE_PORT\" = 8198" \
  >"$state_dir/env-legacy.out" 2>"$state_dir/env-legacy.err"
if [[ "$(grep -c '^Deprecated EPoSe environment variable names were used;' "$state_dir/env-legacy.err")" -ne 1 ]]; then
  echo "legacy environment warning was not emitted exactly once" >&2
  exit 1
fi

env -i PATH="$PATH" \
  QWC_EPOSE_HOST=service.example.org \
  QWC_EPOSE_V2_ENDPOINT_HOST=service.example.org \
  QWC_EPOSE_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  QWC_EPOSE_V2_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  sh -eu -c ". '$source_root/deploy/mainnet/epose-env-compat.sh'" \
  >"$state_dir/env-equal.out" 2>"$state_dir/env-equal.err"
if [[ "$(grep -c '^Deprecated EPoSe environment variable names were used;' "$state_dir/env-equal.err")" -ne 1 ]]; then
  echo "equal legacy environment values did not emit exactly one warning" >&2
  exit 1
fi

set +e
env -i PATH="$PATH" \
  QWC_EPOSE_HOST=one.example.org \
  QWC_EPOSE_V2_ENDPOINT_HOST=two.example.org \
  QWC_EPOSE_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  sh -eu -c ". '$source_root/deploy/mainnet/epose-env-compat.sh'" \
  >"$state_dir/env-conflict.out" 2>"$state_dir/env-conflict.err"
env_conflict_status=$?
set -e
if [[ "$env_conflict_status" -ne 64 ]]; then
  echo "conflicting environment aliases did not fail with status 64" >&2
  exit 1
fi
grep -q '^conflicting EPoSe environment variables:' "$state_dir/env-conflict.err"

set +e
env -i PATH="$PATH" \
  QWC_EPOSE_HOST= \
  QWC_EPOSE_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  sh -eu -c ". '$source_root/deploy/mainnet/epose-env-compat.sh'" \
  >"$state_dir/env-empty.out" 2>"$state_dir/env-empty.err"
env_empty_status=$?
set -e
if [[ "$env_empty_status" -ne 64 ]]; then
  echo "an explicitly empty canonical environment value was accepted" >&2
  exit 1
fi
grep -q '^QWC_EPOSE_HOST must not be empty$' "$state_dir/env-empty.err"

set +e
env -i PATH="$PATH" \
  QWC_EPOSE_HOST=service.example.org \
  QWC_EPOSE_PORT=65536 \
  QWC_EPOSE_DISCOVERY_ENDPOINTS=http://one.example.org:8198 \
  sh -eu -c ". '$source_root/deploy/mainnet/epose-env-compat.sh'" \
  >"$state_dir/env-port.out" 2>"$state_dir/env-port.err"
env_port_status=$?
set -e
if [[ "$env_port_status" -ne 64 ]]; then
  echo "an out-of-range EPoSe port was accepted" >&2
  exit 1
fi
grep -q '^QWC_EPOSE_PORT must be an integer between 1 and 65535$' "$state_dir/env-port.err"

echo "EPoSe public option compatibility: PASS"
