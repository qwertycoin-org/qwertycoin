#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
state_dir="${root}/network4"
status_file="${state_dir}/final-rehearsal.status"
log_file="${state_dir}/final-rehearsal.log"
lock_file="${state_dir}/final-rehearsal.lock"
guard_pid=""

mkdir -p "${state_dir}"
exec 9>"${lock_file}"
flock -n 9 || exit 0

set -a
# shellcheck disable=SC1091
source "${state_dir}/gate.env"
set +a

compose_stop() {
  docker compose --project-name "${EPOSE_COMPOSE_PROJECT_NAME}" \
    -f "${state_dir}/docker-compose.yml" stop >/dev/null 2>&1 || true
}

finish() {
  rc=$?
  if [ -n "${guard_pid}" ]; then
    kill "${guard_pid}" >/dev/null 2>&1 || true
  fi
  if [ "${rc}" -ne 0 ]; then
    compose_stop
    printf 'failed rc=%s at=%s\n' "${rc}" "$(date -u +%FT%TZ)" >"${status_file}"
  fi
  exit "${rc}"
}
trap finish EXIT INT TERM

printf 'running at=%s\n' "$(date -u +%FT%TZ)" >"${status_file}"
"${root}/local_epose_network.sh" up

EPOSE_GUARD_STATE_DIR="${state_dir}" \
EPOSE_GUARD_CONTAINER_PREFIX="${EPOSE_COMPOSE_PROJECT_NAME}" \
EPOSE_GUARD_NODE_COUNT="${EPOSE_NODE_COUNT}" \
EPOSE_GUARD_MINIMUM_AVAILABLE_MIB=1024 \
  "${root}/oom-guard.sh" >/dev/null 2>&1 &
guard_pid=$!

admission_deadline="$(( $(date +%s) + EPOSE_MINE_TIMEOUT ))"
while true; do
  admission_ready=0
  for node in $(seq 0 "$((EPOSE_SERVICE_NODE_COUNT - 1))"); do
    container="${EPOSE_COMPOSE_PROJECT_NAME}-qwc-epose-node-${node}-1"
    if docker logs "${container}" 2>&1 \
        | grep -q 'Submitted EPoSE-v2 lifecycle and admission for epoch 1'; then
      admission_ready=$((admission_ready + 1))
    fi
  done
  printf 'admission-ready=%s/%s at=%s\n' "${admission_ready}" \
    "${EPOSE_SERVICE_NODE_COUNT}" "$(date -u +%FT%TZ)" >"${status_file}"
  [ "${admission_ready}" -eq "${EPOSE_SERVICE_NODE_COUNT}" ] && break
  if [ "$(date +%s)" -gt "${admission_deadline}" ]; then
    echo "Timed out waiting for all genuine final-candidate admission proofs" >&2
    exit 1
  fi
  sleep 10
done

"${root}/local_epose_network.sh" assert-mined-network

snapshot_parts=()
for node in $(seq 0 "$((EPOSE_NODE_COUNT - 1))"); do
  container="${EPOSE_COMPOSE_PROJECT_NAME}-qwc-epose-node-${node}-1"
  rpc_ip="$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "${container}")"
  info="$(curl -fsS -X POST "http://${rpc_ip}:8197/json_rpc" \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"get_info","params":{}}')"
  epose="$(curl -fsS -X POST "http://${rpc_ip}:8197/json_rpc" \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"get_epose_info","params":{}}')"
  genesis="$(curl -fsS -X POST "http://${rpc_ip}:8197/json_rpc" \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"get_block_header_by_height","params":{"height":0}}')"
  rewards="$(curl -fsS -X POST "http://${rpc_ip}:8197/json_rpc" \
    -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":"0","method":"get_service_rewards","params":{"height":1440}}')"
  part="${state_dir}/snapshot-node-${node}.json"
  jq -n --argjson node "${node}" --argjson info "${info}" --argjson epose "${epose}" \
    --argjson genesis "${genesis}" --argjson rewards "${rewards}" \
    '{node:$node,height:$info.result.height,top_hash:$info.result.top_block_hash,
      version:$info.result.version,genesis_hash:$genesis.result.block_header.hash,
      state_hash:$epose.result.state_hash,reward_epoch:$rewards.result.epoch,
      reward_qualified_count:$rewards.result.qualified_count}' >"${part}"
  snapshot_parts+=("${part}")
done
jq -s '.' "${snapshot_parts[@]}" >"${state_dir}/final-rehearsal-snapshot.json"
jq -e --arg genesis "${EPOSE_EXPECTED_GENESIS}" --arg version "${EPOSE_EXPECTED_VERSION}" '
  length == 4 and
  ([.[].height] | unique | length) == 1 and
  ([.[].top_hash] | unique | length) == 1 and
  ([.[].state_hash] | unique | length) == 1 and
  ([.[].genesis_hash] | unique) == [$genesis] and
  ([.[].version] | unique) == [$version] and
  all(.[]; .reward_epoch == 1 and .reward_qualified_count == 4)
' "${state_dir}/final-rehearsal-snapshot.json" >/dev/null

"${root}/local_epose_network.sh" assert-restart-persists
"${root}/local_epose_network.sh" assert-fresh-sync-matches
"${root}/local_epose_network.sh" assert-same-tip
"${root}/local_epose_network.sh" status

for node in $(seq 0 "$((EPOSE_NODE_COUNT - 1))"); do
  container="${EPOSE_COMPOSE_PROJECT_NAME}-qwc-epose-node-${node}-1"
  docker exec "${container}" qwertycoind --version
done

compose_stop
printf 'passed at=%s\n' "$(date -u +%FT%TZ)" >"${status_file}"
trap - EXIT INT TERM
if [ -n "${guard_pid}" ]; then
  kill "${guard_pid}" >/dev/null 2>&1 || true
fi
