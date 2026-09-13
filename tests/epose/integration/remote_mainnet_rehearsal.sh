#!/usr/bin/env bash
set -euo pipefail

EXPECTED_VERSION="${QWC_REHEARSAL_VERSION:-}"
EXPECTED_GENESIS="${QWC_FINAL_GENESIS:-4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39}"

fail_config() {
  echo "invalid rehearsal access configuration: $1" >&2
  exit 78
}

usage() {
  echo "usage: $0 <validate-config|inventory|status|assert-converged|assert-rpc-split|start-mining|stop-mining|mining-status|restart-smoke|sigkill-smoke>" >&2
  exit 64
}

load_access_configuration() {
  SSH_KEY="${QWC_REHEARSAL_SSH_KEY:-}"
  KNOWN_HOSTS="${QWC_REHEARSAL_KNOWN_HOSTS:-}"
  INVENTORY_FILE="${QWC_REHEARSAL_INVENTORY_FILE:-}"

  [[ "$SSH_KEY" == /* && -f "$SSH_KEY" && ! -L "$SSH_KEY" && -r "$SSH_KEY" ]] \
    || fail_config "QWC_REHEARSAL_SSH_KEY must name a readable absolute regular file"
  [[ "$KNOWN_HOSTS" == /* && -f "$KNOWN_HOSTS" && ! -L "$KNOWN_HOSTS" && -r "$KNOWN_HOSTS" ]] \
    || fail_config "QWC_REHEARSAL_KNOWN_HOSTS must name a readable absolute regular file"
  [[ "$INVENTORY_FILE" == /* && -f "$INVENTORY_FILE" && ! -L "$INVENTORY_FILE" && -r "$INVENTORY_FILE" ]] \
    || fail_config "QWC_REHEARSAL_INVENTORY_FILE must name a readable absolute regular file"

  local inventory_mode
  inventory_mode="$(stat -c '%a' "$INVENTORY_FILE" 2>/dev/null || stat -f '%Lp' "$INVENTORY_FILE" 2>/dev/null)" \
    || fail_config "cannot inspect inventory file permissions"
  (( (8#$inventory_mode & 077) == 0 )) \
    || fail_config "inventory file must not be readable or writable by group/other"

  jq -e '
    .schema_version == 1 and
    (.nodes | type == "array" and length == 4) and
    all(.nodes[];
      (keys | sort) == ["name", "public_endpoint", "ssh_target"] and
      (.name | type == "string" and test("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")) and
      (.ssh_target | type == "string" and test("^[A-Za-z_][A-Za-z0-9._-]{0,63}@([A-Za-z0-9][A-Za-z0-9.-]{0,252}|\\[[0-9A-Fa-f:]+\\])$")) and
      (.public_endpoint | type == "string" and test("^([A-Za-z0-9][A-Za-z0-9.-]{0,252}|\\[[0-9A-Fa-f:]+\\])$"))) and
    ([.nodes[].name] | unique | length == 4) and
    ([.nodes[].ssh_target] | unique | length == 4) and
    ([.nodes[].public_endpoint] | unique | length == 4)
  ' "$INVENTORY_FILE" >/dev/null \
    || fail_config "inventory JSON must contain four unique, strictly validated nodes"

  HOST_NAMES=()
  while IFS= read -r value; do HOST_NAMES+=("$value"); done \
    < <(jq -r '.nodes[].name' "$INVENTORY_FILE")
  SSH_TARGETS=()
  while IFS= read -r value; do SSH_TARGETS+=("$value"); done \
    < <(jq -r '.nodes[].ssh_target' "$INVENTORY_FILE")
  PUBLIC_ENDPOINTS=()
  while IFS= read -r value; do PUBLIC_ENDPOINTS+=("$value"); done \
    < <(jq -r '.nodes[].public_endpoint' "$INVENTORY_FILE")
}

validate_config() {
  echo "rehearsal access configuration valid for ${#HOST_NAMES[@]} nodes"
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
    reward_address="$(jq -r '.[] | select(startswith("--epose-v2-reward-address=")) | sub("^--epose-v2-reward-address="; "")' <<<"$command_json")"
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

ACTION="${1:-}"
case "$ACTION" in
  validate-config|inventory|status|assert-converged|assert-rpc-split|start-mining|stop-mining|mining-status|restart-smoke|sigkill-smoke) ;;
  *) usage ;;
esac

load_access_configuration

case "$ACTION" in
  validate-config) validate_config ;;
  inventory) inventory ;;
  status) status ;;
  assert-converged) assert_converged ;;
  assert-rpc-split) assert_rpc_split ;;
  start-mining) start_mining ;;
  stop-mining) stop_mining ;;
  mining-status) mining_status ;;
  restart-smoke) restart_smoke ;;
  sigkill-smoke) sigkill_smoke ;;
esac
