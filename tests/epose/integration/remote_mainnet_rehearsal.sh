#!/usr/bin/env bash
set -euo pipefail

SSH_KEY="${QWC_REHEARSAL_SSH_KEY:-/home/node/.ssh/id_ed25519_quinn}"
KNOWN_HOSTS="${QWC_REHEARSAL_KNOWN_HOSTS:-/home/node/.ssh/known_hosts}"
EXPECTED_VERSION="${QWC_REHEARSAL_VERSION:-}"
EXPECTED_GENESIS="${QWC_FINAL_GENESIS:-906629482787e94cb00463696a0e95ec75a480da09257c6270c65ba1a74a76b0}"
HOST_NAMES=(seed-00 seed-01 seed-02 seed-03)
SSH_TARGETS=(root@95.216.221.239 root@seed-01.qwertycoin.org codiki@159.195.216.239 fabian@159.195.194.92)
PUBLIC_ENDPOINTS=(95.216.221.239 202.61.202.161 159.195.216.239 159.195.194.92)

usage() {
  echo "usage: $0 <inventory|status|assert-converged|assert-rpc-split|start-mining|stop-mining|mining-status|restart-smoke|sigkill-smoke>" >&2
  exit 64
}

admin_http() {
  local index="$1" path="$2" payload="$3"
  ssh_host "$index" "curl -fsS --max-time 15 -X POST http://127.0.0.1:8197/$path -H 'Content-Type: application/json' -d '$payload'"
}

ssh_host() {
  local index="$1"
  shift
  ssh -i "$SSH_KEY" \
    -o UserKnownHostsFile="$KNOWN_HOSTS" \
    -o StrictHostKeyChecking=yes \
    -o BatchMode=yes \
    "${SSH_TARGETS[$index]}" "$@"
}

admin_rpc() {
  local index="$1" method="$2"
  ssh_host "$index" "curl -fsS --max-time 15 -X POST http://127.0.0.1:8197/json_rpc -H 'Content-Type: application/json' -d '{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"$method\",\"params\":{}}'"
}

admin_rpc_params() {
  local index="$1" method="$2" params="$3"
  ssh_host "$index" "curl -fsS --max-time 15 -X POST http://127.0.0.1:8197/json_rpc -H 'Content-Type: application/json' -d '{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"$method\",\"params\":$params}'"
}

snapshot() {
  local index="$1" info epose genesis
  info="$(admin_rpc "$index" get_info)"
  epose="$(admin_rpc "$index" get_epose_info)"
  genesis="$(admin_rpc_params "$index" get_block_header_by_height '{"height":0}')"
  jq -n \
    --arg node "${HOST_NAMES[$index]}" \
    --argjson info "$info" \
    --argjson epose "$epose" \
    --argjson genesis "$genesis" \
    '{node:$node,version:$info.result.version,height:$info.result.height,tip_height:($info.result.height-1),top_hash:$info.result.top_block_hash,genesis_hash:$genesis.result.block_header.hash,state_hash:$epose.result.state_hash,epoch:$epose.result.current_epoch,member_count:$epose.result.service_node_count,qualified_count:$epose.result.qualified_count,registered:$epose.result.local_service_node_registered,qualified:$epose.result.local_service_node_qualified}'
}

wait_ready() {
  local index="$1" deadline=$((SECONDS + 180))
  until admin_rpc "$index" get_info >/dev/null 2>&1; do
    if (( SECONDS > deadline )); then
      echo "${HOST_NAMES[$index]} RPC did not recover" >&2
      return 1
    fi
    sleep 2
  done
}

inventory() {
  local i
  for i in "${!SSH_TARGETS[@]}"; do
    ssh_host "$i" \
      'docker inspect qwertycoin-mainnet --format "node={{.Name}} image={{.Image}} started={{.State.StartedAt}} status={{.State.Status}}"; docker exec qwertycoin-mainnet qwertycoind --version | head -n1; docker exec qwertycoin-mainnet sha256sum /usr/local/bin/qwertycoind; docker inspect qwertycoin-mainnet --format "mounts={{json .Mounts}} ports={{json .HostConfig.PortBindings}}"'
  done
}

status() {
  local i
  for i in "${!SSH_TARGETS[@]}"; do snapshot "$i"; done | jq -s .
}

assert_converged() {
  local snapshots
  snapshots="$(status)"
  jq -e --arg genesis "$EXPECTED_GENESIS" '
    length == 4 and
    ([.[].genesis_hash] | unique) == [$genesis] and
    ([.[] | [.height,.top_hash,.state_hash]] | unique | length) == 1
  ' <<<"$snapshots" >/dev/null
  if [[ -n "$EXPECTED_VERSION" ]]; then
    jq -e --arg version "$EXPECTED_VERSION" \
      'all(.[]; .version == $version)' <<<"$snapshots" >/dev/null
  fi
  printf '%s\n' "$snapshots"
}

assert_rpc_split() {
  local i public_code admin_code
  for i in "${!SSH_TARGETS[@]}"; do
    public_code="$(curl -sS -o /dev/null -w '%{http_code}' --max-time 10 -X POST \
      "http://${PUBLIC_ENDPOINTS[$i]}:8198/submit_epose_envelope" \
      -H 'Content-Type: application/json' -d '{}')"
    if [[ "$public_code" != 404 ]]; then
      echo "${HOST_NAMES[$i]} restricted RPC exposed submit endpoint (HTTP $public_code)" >&2
      return 1
    fi
    admin_code="$(ssh_host "$i" "curl -sS -o /dev/null -w '%{http_code}' --max-time 10 -X POST http://127.0.0.1:8197/submit_epose_envelope -H 'Content-Type: application/json' -d '{}'")"
    if [[ "$admin_code" == 404 ]]; then
      echo "${HOST_NAMES[$i]} admin RPC is missing submit endpoint" >&2
      return 1
    fi
    curl -fsS --max-time 10 -X POST \
      "http://${PUBLIC_ENDPOINTS[$i]}:8198/get_epose_service_endpoint_v2" \
      -H 'Content-Type: application/json' -d '{}' >/dev/null
    echo "${HOST_NAMES[$i]} restricted=$public_code admin=$admin_code challenge=available"
  done
}

start_mining() {
  local i command_json reward_address payload
  for i in "${!SSH_TARGETS[@]}"; do
    command_json="$(ssh_host "$i" 'docker inspect qwertycoin-mainnet --format "{{json .Config.Cmd}}"')"
    reward_address="$(jq -r '.[] | select(startswith("--epose-reward-address=")) | sub("^--epose-reward-address="; "")' <<<"$command_json")"
    if [[ -z "$reward_address" || "$reward_address" == null ]]; then
      echo "${HOST_NAMES[$i]} has no configured rehearsal reward address" >&2
      return 1
    fi
    payload="$(jq -cn --arg address "$reward_address" '{miner_address:$address,threads_count:1,do_background_mining:false,ignore_battery:true}')"
    admin_http "$i" start_mining "$payload" >/dev/null
    echo "${HOST_NAMES[$i]} mining started with one RandomX thread"
  done
}

stop_mining() {
  local i
  for i in "${!SSH_TARGETS[@]}"; do
    admin_http "$i" stop_mining '{}' >/dev/null
    echo "${HOST_NAMES[$i]} mining stopped"
  done
}

mining_status() {
  local i response
  for i in "${!SSH_TARGETS[@]}"; do
    response="$(admin_http "$i" mining_status '{}')"
    jq -n --arg node "${HOST_NAMES[$i]}" --argjson response "$response" \
      '{node:$node,active:$response.active,threads:$response.threads_count,speed:$response.speed,difficulty:$response.difficulty}'
  done | jq -s .
}

restart_smoke() {
  local before after i
  before="$(status)"
  for i in "${!SSH_TARGETS[@]}"; do ssh_host "$i" docker restart qwertycoin-mainnet >/dev/null; done
  for i in "${!SSH_TARGETS[@]}"; do wait_ready "$i"; done
  after="$(status)"
  jq -e --argjson after "$after" '
    length == 4 and
    all(.[]; .node as $node | .height as $height |
      any($after[]; .node == $node and .height >= $height))
  ' <<<"$before" >/dev/null
  assert_converged
}

sigkill_smoke() {
  local before after
  before="$(snapshot 1)"
  ssh_host 1 'docker kill --signal KILL qwertycoin-mainnet >/dev/null; docker start qwertycoin-mainnet >/dev/null'
  wait_ready 1
  after="$(snapshot 1)"
  jq -e --argjson after "$after" '.genesis_hash == $after.genesis_hash and .state_hash == $after.state_hash' <<<"$before" >/dev/null
  assert_converged
}

case "${1:-}" in
  inventory) inventory ;;
  status) status ;;
  assert-converged) assert_converged ;;
  assert-rpc-split) assert_rpc_split ;;
  start-mining) start_mining ;;
  stop-mining) stop_mining ;;
  mining-status) mining_status ;;
  restart-smoke) restart_smoke ;;
  sigkill-smoke) sigkill_smoke ;;
  *) usage ;;
esac
