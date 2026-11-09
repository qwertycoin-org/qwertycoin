#!/usr/bin/env bash
set -u

build_dir="${QWC_BUILD_DIR:-build/release}"
unit="$build_dir/tests/unit_tests/unit_tests"
epose="$build_dir/tests/unit_tests/epose_unit_tests"
core="$build_dir/tests/core_tests/core_tests"

for binary in "$unit" "$epose" "$core"; do
  if [[ ! -x "$binary" ]]; then
    printf 'Missing test binary: %s\n' "$binary" >&2
    exit 2
  fi
done

passed=0
failed=0
run_gate() {
  local name="$1"
  shift
  printf '\n=== %s ===\n' "$name"
  if "$@"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
  fi
}

run_gate epose_unit_tests "$epose"
run_gate unit_tests "$unit" \
  --gtest_filter='-boosted_tcp_server.*:http_server.*:AddressFromURL.*:node_server.bind_same_p2p_port:DNSResolver.DNSSECSuccess'
run_gate gen_chainlock "$core" --generate_and_play_test_data --filter 'gen_chainlock*'
run_gate gen_contracts "$core" --generate_and_play_test_data --filter 'gen_contracts*'
run_gate gen_height_window "$core" --generate_and_play_test_data --filter 'gen_height_window*'

printf '\nRelease gate: %d passed, %d failed\n' "$passed" "$failed"
[[ "$failed" -eq 0 ]]
