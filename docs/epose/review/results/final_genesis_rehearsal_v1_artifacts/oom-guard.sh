#!/usr/bin/env bash
set -euo pipefail

state_dir="${EPOSE_GUARD_STATE_DIR:?EPOSE_GUARD_STATE_DIR is required}"
container_prefix="${EPOSE_GUARD_CONTAINER_PREFIX:?EPOSE_GUARD_CONTAINER_PREFIX is required}"
node_count="${EPOSE_GUARD_NODE_COUNT:?EPOSE_GUARD_NODE_COUNT is required}"
minimum_available_mib="${EPOSE_GUARD_MINIMUM_AVAILABLE_MIB:-1024}"
log="${state_dir}/oom-guard.log"
containers=()

for node in $(seq 0 "$((node_count - 1))"); do
  containers+=("${container_prefix}-qwc-epose-node-${node}-1")
done

while true; do
  available_mib="$(free -m | awk 'NR == 2 { print $7 }')"
  printf '%s available_mib=%s\n' "$(date -u +%FT%TZ)" "${available_mib}" >>"${log}"
  if [ "${available_mib}" -lt "${minimum_available_mib}" ]; then
    printf '%s threshold breached; stopping final-genesis disposable containers\n' \
      "$(date -u +%FT%TZ)" >>"${log}"
    docker stop "${containers[@]}" >>"${log}" 2>&1 || true
    exit 75
  fi

  running=0
  for container in "${containers[@]}"; do
    if [ "$(docker inspect -f '{{.State.Running}}' "${container}" 2>/dev/null || true)" = true ]; then
      running=$((running + 1))
    fi
  done
  [ "${running}" -eq 0 ] && exit 0
  sleep 5
done
