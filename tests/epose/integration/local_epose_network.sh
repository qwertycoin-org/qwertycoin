#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

NODE_COUNT="${EPOSE_NODE_COUNT:-5}"
SERVICE_NODE_COUNT="${EPOSE_SERVICE_NODE_COUNT:-${NODE_COUNT}}"
STATE_DIR="${EPOSE_STATE_DIR:-${REPO_ROOT}/.epose-local-net}"
IMAGE="${EPOSE_IMAGE:-qwertycoin-v2-node:epose-dev-tests}"
RPC_BASE_PORT="${EPOSE_RPC_BASE_PORT:-18197}"
P2P_BASE_PORT="${EPOSE_P2P_BASE_PORT:-18196}"
PUBLISH_P2P="${EPOSE_PUBLISH_P2P:-0}"
RPC_READY_TIMEOUT="${EPOSE_RPC_READY_TIMEOUT:-120}"
CONVERGENCE_TIMEOUT="${EPOSE_CONVERGENCE_TIMEOUT:-300}"
COMPOSE_PARALLEL_LIMIT="${EPOSE_COMPOSE_PARALLEL_LIMIT:-8}"
START_BATCH_SIZE="${EPOSE_START_BATCH_SIZE:-0}"
FIXED_DIFFICULTY="${EPOSE_FIXED_DIFFICULTY:-1}"
EPOCH_LENGTH="${EPOSE_EPOCH_LENGTH:-720}"
MINE_TIMEOUT="${EPOSE_MINE_TIMEOUT:-900}"
MINING_THREADS="${EPOSE_MINING_THREADS:-1}"
RELAY_MINE_BURST_BLOCKS="${EPOSE_RELAY_MINE_BURST_BLOCKS:-24}"
RELAY_IDLE_SECONDS="${EPOSE_RELAY_IDLE_SECONDS:-5}"
RANDOMX_UMASK="${EPOSE_RANDOMX_UMASK:-4}"
OFFLINE="${EPOSE_OFFLINE:-0}"
NETWORK_PREFIX="${EPOSE_NETWORK_PREFIX:-$(basename "${STATE_DIR}" | tr -c '[:alnum:]' '-')}"
NETWORK_NAME="${NETWORK_PREFIX}-qwc-epose-local-net"
PARTITION_A_NETWORK="${NETWORK_PREFIX}-qwc-epose-partition-a"
PARTITION_B_NETWORK="${NETWORK_PREFIX}-qwc-epose-partition-b"
COMPOSE_FILE="${STATE_DIR}/docker-compose.yml"
REWARD_ADDRESS="${EPOSE_SERVICE_REWARD_ADDRESS:-TBQmgSvK5rAJCLvPMLoJxJ7gVbKaSHqFDc3xva1Ggspn45TagaA3XCdTPw2cjU8szsVoXZbnwtBBcXPbSUJhrsZU9No7XHsNep}"
REWARD_VIEW_KEY="${EPOSE_SERVICE_REWARD_VIEW_KEY:-}"

usage() {
  cat <<USAGE
Usage: $0 <command>

Commands:
  generate        Write the Docker Compose file for EPOSE_NODE_COUNT nodes
  up              Generate and start the local EPoSE network
  down            Stop the local EPoSE network
  clean           Stop the local EPoSE network and remove its named volumes
  status          Print node heights, top hashes, and EPoSE state hashes
  assert-same-tip Fail unless every reachable node reports the same tip and EPoSE state
  assert-restart-persists
                  Recreate containers with existing volumes and verify state is unchanged
  assert-mined-registration
                  Mine until node 0 auto-registers on the active EPoSE protocol
  assert-mined-registration-restart-persists
                  Mine node 0 registration, recreate containers, and verify state persists
  assert-mined-network
                  Mine from every node until all local service nodes are registered,
                  at least one service node is qualified, and reward RPC views match
  assert-relay-non-service-miner
                  Verify service nodes register/qualify while only the last normal
                  non-service node mines
  assert-service-reward-finalized-epoch
                  Mine past the first finalized reward epoch and verify reward RPC
                  views are still based on the finalized source epoch
  assert-fresh-sync-matches
                  Start a fresh non-service validator datadir, sync from node 0,
                  and verify the same height, top hash, and EPoSE state hash
  assert-partition-heal
                  Split, heal, and verify the network returns to one tip and EPoSE state
  assert-partition-heal-reorg
                  Split the network, mine competing branches, heal, and verify reorged EPoSE state
  assert-epoch-boundary-reorg
                  Mine to an EPoSE epoch boundary, split into competing branches,
                  heal, restart, and verify reward/state convergence
  assert-sigkill-recovery
                  Kill one daemon with SIGKILL, restart it with the same volume,
                  mine new blocks, and verify EPoSE state convergence
  partition       Split the network into A/B bridge networks
  heal            Reconnect every node to the shared bridge network

Environment:
  EPOSE_NODE_COUNT              Node count, expected values include 5, 10, 25
  EPOSE_SERVICE_NODE_COUNT      Number of leading nodes started in service-node mode,
                                default matches EPOSE_NODE_COUNT
  EPOSE_STATE_DIR               Working directory for generated compose/data
  EPOSE_IMAGE                   Docker image, default ${IMAGE}
  EPOSE_RPC_BASE_PORT           First host RPC port, default ${RPC_BASE_PORT}
  EPOSE_P2P_BASE_PORT           First host P2P port, default ${P2P_BASE_PORT}
  EPOSE_PUBLISH_P2P             Set to 1 to publish P2P ports on 127.0.0.1
  EPOSE_RPC_READY_TIMEOUT       Seconds to wait for every node RPC, default ${RPC_READY_TIMEOUT}
  EPOSE_CONVERGENCE_TIMEOUT     Seconds to wait for post-mining convergence, default ${CONVERGENCE_TIMEOUT}
  EPOSE_COMPOSE_PARALLEL_LIMIT  Docker Compose operation parallelism, default ${COMPOSE_PARALLEL_LIMIT}
  EPOSE_START_BATCH_SIZE        Start nodes in batches, default 0 starts all at once
  EPOSE_FIXED_DIFFICULTY        Fixed testnet mining difficulty, default ${FIXED_DIFFICULTY}
  EPOSE_EPOCH_LENGTH            Blocks per EPoSE epoch, default ${EPOCH_LENGTH}
  EPOSE_MINE_TIMEOUT            Seconds to wait for mined registration, default ${MINE_TIMEOUT}
  EPOSE_MINING_THREADS          Miner threads per active node, default ${MINING_THREADS}
  EPOSE_RELAY_MINE_BURST_BLOCKS Blocks per non-service miner burst in relay tests,
                                default ${RELAY_MINE_BURST_BLOCKS}
  EPOSE_RELAY_IDLE_SECONDS      Idle seconds between relay test mining bursts,
                                default ${RELAY_IDLE_SECONDS}
  EPOSE_RANDOMX_UMASK           RandomX flag mask for test containers, default ${RANDOMX_UMASK}
  EPOSE_OFFLINE                 Set to 1 for isolated single-node mining smoke tests
  EPOSE_NETWORK_PREFIX          Prefix for explicit Docker network names
  EPOSE_SERVICE_REWARD_ADDRESS  Testnet reward address for service-node mode
  EPOSE_SERVICE_REWARD_VIEW_KEY Private view key matching EPOSE_SERVICE_REWARD_ADDRESS
USAGE
}

require_node_count() {
  case "${NODE_COUNT}" in
    ''|*[!0-9]*) echo "EPOSE_NODE_COUNT must be numeric" >&2; exit 2 ;;
  esac
  case "${SERVICE_NODE_COUNT}" in
    ''|*[!0-9]*) echo "EPOSE_SERVICE_NODE_COUNT must be numeric" >&2; exit 2 ;;
  esac
  if [ "${NODE_COUNT}" -lt 1 ]; then
    echo "EPOSE_NODE_COUNT must be at least 1" >&2
    exit 2
  fi
  if [ "${SERVICE_NODE_COUNT}" -gt "${NODE_COUNT}" ]; then
    echo "EPOSE_SERVICE_NODE_COUNT must not exceed EPOSE_NODE_COUNT" >&2
    exit 2
  fi
}

compose() {
  COMPOSE_PARALLEL_LIMIT="${COMPOSE_PARALLEL_LIMIT}" docker compose -f "${COMPOSE_FILE}" "$@"
}

node_name() {
  printf "qwc-epose-node-%s" "$1"
}

node_rpc_port() {
  printf "%s" "$((RPC_BASE_PORT + $1))"
}

node_p2p_port() {
  printf "%s" "$((P2P_BASE_PORT + $1))"
}

json_rpc() {
  local port="$1"
  local method="$2"
  curl -fsS -X POST "http://127.0.0.1:${port}/json_rpc" \
    -H "Content-Type: application/json" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":{}}"
}

json_rpc_params() {
  local port="$1"
  local method="$2"
  local params="$3"
  curl -fsS -X POST "http://127.0.0.1:${port}/json_rpc" \
    -H "Content-Type: application/json" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":${params}}"
}

daemon_rpc() {
  local port="$1"
  local path="$2"
  local params="$3"
  curl -fsS -X POST "http://127.0.0.1:${port}/${path}" \
    -H "Content-Type: application/json" \
    -d "${params}"
}

now() {
  date +%s
}

generate_compose() {
  require_node_count
  mkdir -p "${STATE_DIR}"

  {
    cat <<YAML
services:
YAML
    for i in $(seq 0 "$((NODE_COUNT - 1))"); do
      local name rpc_port p2p_port
      name="$(node_name "${i}")"
      rpc_port="$(node_rpc_port "${i}")"
      p2p_port="$(node_p2p_port "${i}")"
      cat <<YAML
  ${name}:
    image: ${IMAGE}
    entrypoint: ["/usr/local/bin/qwertycoind"]
    command:
      - --testnet
      - --non-interactive
      - --p2p-bind-ip=0.0.0.0
      - --p2p-bind-port=8196
      - --rpc-bind-ip=0.0.0.0
      - --rpc-bind-port=8197
      - --confirm-external-bind
      - --no-igd
      - --hide-my-port
      - --disable-dns-checkpoints
      - --fixed-difficulty=${FIXED_DIFFICULTY}
YAML
      if [ "${OFFLINE}" = "1" ]; then
        cat <<YAML
      - --offline
YAML
      fi
      if [ "${i}" -lt "${SERVICE_NODE_COUNT}" ]; then
        cat <<YAML
      - --service-node
      - --service-reward-address=${REWARD_ADDRESS}
      - --service-reward-view-key=${REWARD_VIEW_KEY:?EPOSE_SERVICE_REWARD_VIEW_KEY is required for service-node mode}
      - --service-node-advertise-address=${name}:8196
YAML
      fi
      local peer
      for peer in $(seq 0 "$((NODE_COUNT - 1))"); do
        if [ "${peer}" -lt "${i}" ]; then
          cat <<YAML
      - --add-priority-node=qwc-epose-node-${peer}:8196
YAML
        fi
      done
      cat <<YAML
    environment:
      MONERO_RANDOMX_UMASK: "${RANDOMX_UMASK}"
    ports:
YAML
      if [ "${PUBLISH_P2P}" = "1" ]; then
        cat <<YAML
      - "127.0.0.1:${p2p_port}:8196"
YAML
      fi
      cat <<YAML
      - "127.0.0.1:${rpc_port}:8197"
    volumes:
      - ${name}-data:/root/.qwertycoin
    networks:
      - qwc-epose-local-net

YAML
    done

    cat <<YAML
networks:
  qwc-epose-local-net:
    driver: bridge
    name: ${NETWORK_NAME}
  qwc-epose-partition-a:
    driver: bridge
    name: ${PARTITION_A_NETWORK}
  qwc-epose-partition-b:
    driver: bridge
    name: ${PARTITION_B_NETWORK}

volumes:
YAML
    for i in $(seq 0 "$((NODE_COUNT - 1))"); do
      printf "  %s-data:\n" "$(node_name "${i}")"
    done
  } > "${COMPOSE_FILE}"

  echo "Wrote ${COMPOSE_FILE} for ${NODE_COUNT} nodes"
}

wait_for_node_rpc() {
  local i="$1"
  local deadline node port
  deadline="$(($(now) + RPC_READY_TIMEOUT))"
  node="$(node_name "${i}")"
  port="$(node_rpc_port "${i}")"
  while ! json_rpc "${port}" "get_epose_info" >/dev/null 2>&1; do
    if [ "$(now)" -gt "${deadline}" ]; then
      echo "RPC did not become ready for ${node} on port ${port}" >&2
      compose logs --tail=80 "${node}" >&2 || true
      exit 1
    fi
    sleep 2
  done
}

wait_for_rpc() {
  local i
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    wait_for_node_rpc "${i}"
  done
}

start_network() {
  if [ "${START_BATCH_SIZE}" -le 0 ] || [ "${START_BATCH_SIZE}" -ge "${NODE_COUNT}" ]; then
    compose up -d
    wait_for_rpc
    return
  fi

  local start end i services=()
  start=0
  while [ "${start}" -lt "${NODE_COUNT}" ]; do
    end="$((start + START_BATCH_SIZE - 1))"
    if [ "${end}" -ge "$((NODE_COUNT - 1))" ]; then
      end="$((NODE_COUNT - 1))"
    fi

    services=()
    for i in $(seq "${start}" "${end}"); do
      services+=("$(node_name "${i}")")
    done

    compose up -d "${services[@]}"
    for i in $(seq "${start}" "${end}"); do
      wait_for_node_rpc "${i}"
    done

    start="$((end + 1))"
  done
}

status() {
  require_node_count
  network_fingerprint
}

network_fingerprint() {
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port info_json epose_json height hash state_hash
    port="$(node_rpc_port "${i}")"
    if ! info_json="$(json_rpc "${port}" "get_info" 2>/dev/null)"; then
      printf "%s rpc=unreachable port=%s\n" "$(node_name "${i}")" "${port}"
      continue
    fi
    if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
      printf "%s rpc=unreachable port=%s epose_rpc=failed\n" "$(node_name "${i}")" "${port}"
      continue
    fi
    height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
    hash="$(printf "%s" "${info_json}" | jq -r '.result.top_block_hash')"
    state_hash="$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')"
    printf "%s height=%s top_hash=%s epose_state_hash=%s\n" "$(node_name "${i}")" "${height}" "${hash}" "${state_hash}"
  done
}

node_tip_state() {
  local i="$1"
  local port info_json epose_json
  port="$(node_rpc_port "${i}")"
  info_json="$(json_rpc "${port}" "get_info")"
  epose_json="$(json_rpc "${port}" "get_epose_info")"
  jq -r \
    --arg state_hash "$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')" \
    '.result | "\(.height):\(.top_block_hash):\($state_hash)"' \
    <<<"${info_json}"
}

node_rpc_url() {
  local i="$1"
  local container net ip
  container="$(compose ps -q "$(node_name "${i}")")"
  for net in "${NETWORK_NAME}" "${PARTITION_A_NETWORK}" "${PARTITION_B_NETWORK}"; do
    ip="$(docker inspect -f "{{with index .NetworkSettings.Networks \"${net}\"}}{{.IPAddress}}{{end}}" "${container}" 2>/dev/null || true)"
    if [ -n "${ip}" ]; then
      printf "http://%s:8197" "${ip}"
      return
    fi
  done
  printf "http://127.0.0.1:%s" "$(node_rpc_port "${i}")"
}

json_rpc_node() {
  local i="$1"
  local method="$2"
  curl -fsS -X POST "$(node_rpc_url "${i}")/json_rpc" \
    -H "Content-Type: application/json" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":{}}"
}

json_rpc_node_params() {
  local i="$1"
  local method="$2"
  local params="$3"
  curl -fsS -X POST "$(node_rpc_url "${i}")/json_rpc" \
    -H "Content-Type: application/json" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":${params}}"
}

daemon_rpc_node() {
  local i="$1"
  local path="$2"
  local params="$3"
  curl -fsS -X POST "$(node_rpc_url "${i}")/${path}" \
    -H "Content-Type: application/json" \
    -d "${params}"
}

node_tip_state_any_network() {
  local i="$1"
  local info_json epose_json
  info_json="$(json_rpc_node "${i}" "get_info")"
  epose_json="$(json_rpc_node "${i}" "get_epose_info")"
  jq -r \
    --arg state_hash "$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')" \
    '.result | "\(.height):\(.top_block_hash):\($state_hash)"' \
    <<<"${info_json}"
}

assert_restart_persists() {
  require_node_count
  local before after
  before="$(network_fingerprint)"
  compose down
  start_network
  after="$(network_fingerprint)"

  if [ "${before}" != "${after}" ]; then
    printf "%s\n" "${after}"
    echo "EPoSE local network state changed across container recreation" >&2
    exit 1
  fi

  printf "%s\n" "${after}"
}

start_mining_node() {
  local i="${1:-0}"
  local threads="${2:-1}"
  local port deadline response status
  port="$(node_rpc_port "${i}")"
  deadline="$(($(now) + MINE_TIMEOUT))"

  while true; do
    response="$(daemon_rpc "${port}" "start_mining" \
      "{\"miner_address\":\"${REWARD_ADDRESS}\",\"threads_count\":${threads},\"do_background_mining\":false,\"ignore_battery\":true}")"
    status="$(printf "%s" "${response}" | jq -r '.status // .result.status // empty')"
    case "${status}" in
      OK|"Already mining")
        return
        ;;
      BUSY)
        if [ "$(now)" -le "${deadline}" ]; then
          sleep 2
          continue
        fi
        ;;
    esac

    if node_mining_active "${i}"; then
      return
    fi

    printf "%s\n" "${response}" >&2
    echo "Failed to start mining on $(node_name "${i}")" >&2
    exit 1
  done
}

stop_mining_node() {
  local i="${1:-0}"
  local port
  port="$(node_rpc_port "${i}")"
  daemon_rpc "${port}" "stop_mining" "{}" >/dev/null 2>&1 || true
}

node_mining_active() {
  local i="$1"
  local port status_json
  port="$(node_rpc_port "${i}")"
  if ! status_json="$(daemon_rpc "${port}" "mining_status" "{}" 2>/dev/null)"; then
    return 1
  fi
  [ "$(printf "%s" "${status_json}" | jq -r '.active')" = "true" ]
}

wait_for_node_mining_stopped() {
  local i="$1"
  local deadline
  deadline="$(($(now) + 30))"
  while node_mining_active "${i}"; do
    if [ "$(now)" -gt "${deadline}" ]; then
      echo "Mining did not stop on $(node_name "${i}")" >&2
      exit 1
    fi
    sleep 1
  done
}

stop_mining_all_nodes() {
  local i
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    stop_mining_node "${i}"
    wait_for_node_mining_stopped "${i}"
  done
}

group_same_tip_state() {
  local start="$1"
  local end="$2"
  local first="" i tip
  for i in $(seq "${start}" "${end}"); do
    tip="$(node_tip_state "${i}")"
    if [ -z "${first}" ]; then
      first="${tip}"
    elif [ "${tip}" != "${first}" ]; then
      return 1
    fi
  done
}

group_same_tip_state_any_network() {
  local start="$1"
  local end="$2"
  local first="" i tip
  for i in $(seq "${start}" "${end}"); do
    tip="$(node_tip_state_any_network "${i}")"
    if [ -z "${first}" ]; then
      first="${tip}"
    elif [ "${tip}" != "${first}" ]; then
      return 1
    fi
  done
}

wait_for_group_same_tip_state() {
  local start="$1"
  local end="$2"
  local deadline
  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while ! group_same_tip_state "${start}" "${end}"; do
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      echo "EPoSE local partition ${start}-${end} did not converge to the same tip and state" >&2
      exit 1
    fi
    sleep 2
  done
}

wait_for_group_same_tip_state_any_network() {
  local start="$1"
  local end="$2"
  local deadline
  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while ! group_same_tip_state_any_network "${start}" "${end}"; do
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      echo "EPoSE local partition ${start}-${end} did not converge to the same tip and state" >&2
      exit 1
    fi
    sleep 2
  done
}

mine_node_blocks() {
  local i="$1"
  local blocks="$2"
  local port start_height target_height height deadline info_json
  port="$(node_rpc_port "${i}")"
  info_json="$(json_rpc "${port}" "get_info")"
  start_height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
  target_height="$((start_height + blocks))"
  deadline="$(($(now) + MINE_TIMEOUT))"

  start_mining_node "${i}" "${MINING_THREADS}"
  while true; do
    info_json="$(json_rpc "${port}" "get_info")"
    height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
    if [ "${height}" -ge "${target_height}" ]; then
      stop_mining_node "${i}"
      wait_for_node_mining_stopped "${i}"
      return
    fi
    if [ "$(now)" -gt "${deadline}" ]; then
      stop_mining_node "${i}"
      print_registration_status >&2
      echo "Mining on $(node_name "${i}") did not reach height ${target_height}" >&2
      exit 1
    fi
    sleep 2
  done
}

mine_until_height() {
  local i="$1"
  local target_height="$2"
  local port height info_json
  port="$(node_rpc_port "${i}")"
  info_json="$(json_rpc "${port}" "get_info")"
  height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
  if [ "${height}" -ge "${target_height}" ]; then
    return
  fi

  mine_node_blocks "${i}" "$((target_height - height))"
  converge_same_tip_state "${i}"
}

node_mining_active_any_network() {
  local i="$1"
  local status_json
  if ! status_json="$(daemon_rpc_node "${i}" "mining_status" "{}" 2>/dev/null)"; then
    return 1
  fi
  [ "$(printf "%s" "${status_json}" | jq -r '.active')" = "true" ]
}

wait_for_node_mining_stopped_any_network() {
  local i="$1"
  local deadline
  deadline="$(($(now) + 30))"
  while node_mining_active_any_network "${i}"; do
    if [ "$(now)" -gt "${deadline}" ]; then
      echo "Mining did not stop on $(node_name "${i}")" >&2
      exit 1
    fi
    sleep 1
  done
}

start_mining_node_any_network() {
  local i="${1:-0}"
  local threads="${2:-1}"
  local deadline response status
  deadline="$(($(now) + MINE_TIMEOUT))"

  while true; do
    response="$(daemon_rpc_node "${i}" "start_mining" \
      "{\"miner_address\":\"${REWARD_ADDRESS}\",\"threads_count\":${threads},\"do_background_mining\":false,\"ignore_battery\":true}")"
    status="$(printf "%s" "${response}" | jq -r '.status // .result.status // empty')"
    case "${status}" in
      OK|"Already mining")
        return
        ;;
      BUSY)
        if [ "$(now)" -le "${deadline}" ]; then
          sleep 2
          continue
        fi
        ;;
    esac

    if node_mining_active_any_network "${i}"; then
      return
    fi

    printf "%s\n" "${response}" >&2
    echo "Failed to start mining on $(node_name "${i}")" >&2
    exit 1
  done
}

stop_mining_node_any_network() {
  local i="${1:-0}"
  daemon_rpc_node "${i}" "stop_mining" "{}" >/dev/null 2>&1 || true
}

mine_node_for_seconds_any_network() {
  local i="$1"
  local seconds="$2"
  start_mining_node_any_network "${i}" "${MINING_THREADS}"
  sleep "${seconds}"
  stop_mining_node_any_network "${i}"
  wait_for_node_mining_stopped_any_network "${i}"
}

mine_node_blocks_any_network() {
  local i="$1"
  local blocks="$2"
  local start_height target_height height deadline info_json
  info_json="$(json_rpc_node "${i}" "get_info")"
  start_height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
  target_height="$((start_height + blocks))"
  deadline="$(($(now) + MINE_TIMEOUT))"

  start_mining_node_any_network "${i}" "${MINING_THREADS}"
  while true; do
    info_json="$(json_rpc_node "${i}" "get_info")"
    height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
    if [ "${height}" -ge "${target_height}" ]; then
      stop_mining_node_any_network "${i}"
      wait_for_node_mining_stopped_any_network "${i}"
      return
    fi
    if [ "$(now)" -gt "${deadline}" ]; then
      stop_mining_node_any_network "${i}"
      print_registration_status >&2
      echo "Mining on $(node_name "${i}") did not reach height ${target_height}" >&2
      exit 1
    fi
    sleep 1
  done
}

same_tip_state_any_network() {
  local first="" i tip
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    tip="$(node_tip_state_any_network "${i}")"
    if [ -z "${first}" ]; then
      first="${tip}"
    elif [ "${tip}" != "${first}" ]; then
      return 1
    fi
  done
}

wait_for_same_tip_state_any_network() {
  local anchor="${1:-}"
  local deadline
  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while ! same_tip_state_any_network; do
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      echo "EPoSE local network did not converge to the same tip and state" >&2
      exit 1
    fi
    if [ -n "${anchor}" ]; then
      mine_node_blocks_any_network "${anchor}" 1
      sleep 5
    else
      sleep 2
    fi
  done
}

assert_mined_registration() {
  require_node_count
  wait_for_rpc

  local port deadline height enabled registered service_count
  port="$(node_rpc_port 0)"
  deadline="$(($(now) + MINE_TIMEOUT))"
  start_mining_node 0 1

  while true; do
    local info_json epose_json
    info_json="$(json_rpc "${port}" "get_info")"
    epose_json="$(json_rpc "${port}" "get_epose_info")"
    height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
    enabled="$(printf "%s" "${epose_json}" | jq -r '.result.enabled')"
    registered="$(printf "%s" "${epose_json}" | jq -r '.result.local_service_node_registered')"
    service_count="$(printf "%s" "${epose_json}" | jq -r '.result.service_node_count')"

    if [ "${enabled}" = "true" ] && [ "${registered}" = "true" ] && [ "${service_count}" -ge 1 ]; then
      stop_mining_node 0
      printf "%s height=%s epose_enabled=%s service_node_count=%s local_registered=%s\n" \
        "$(node_name 0)" "${height}" "${enabled}" "${service_count}" "${registered}"
      return
    fi

    if [ "$(now)" -gt "${deadline}" ]; then
      stop_mining_node 0
      printf "%s height=%s epose_enabled=%s service_node_count=%s local_registered=%s\n" \
        "$(node_name 0)" "${height}" "${enabled}" "${service_count}" "${registered}" >&2
      echo "EPoSE mined registration did not appear before timeout" >&2
      exit 1
    fi

    sleep 2
  done
}

assert_mined_registration_restart_persists() {
  assert_mined_registration
  assert_restart_persists
}

reward_fingerprint() {
  local height="$1"
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port rewards_json
    port="$(node_rpc_port "${i}")"
    rewards_json="$(json_rpc_params "${port}" "get_service_rewards" "{\"height\":${height}}")"
    jq -r \
      --arg node "$(node_name "${i}")" \
      '.result | "\($node) height=\(.height) epoch=\(.epoch) service_reward_active=\(.service_reward_active) qualified_count=\(.qualified_count) expected_payee=\(.expected_payee_service_public_key)"' \
      <<<"${rewards_json}"
  done
}

assert_same_service_rewards() {
  require_node_count
  local height first=""
  height="$1"
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port rewards_json fingerprint
    port="$(node_rpc_port "${i}")"
    rewards_json="$(json_rpc_params "${port}" "get_service_rewards" "{\"height\":${height}}")"
    fingerprint="$(jq -r \
      '.result | "\(.height):\(.epoch):\(.service_reward_active):\(.service_reward_bps):\(.qualified_count):\(.expected_payee_service_public_key):\(.expected_reward_view_public_key):\(.expected_reward_spend_public_key)"' \
      <<<"${rewards_json}")"
    if [ -z "${first}" ]; then
      first="${fingerprint}"
    elif [ "${fingerprint}" != "${first}" ]; then
      reward_fingerprint "${height}"
      echo "EPoSE service reward RPC views differ" >&2
      exit 1
    fi
  done
  reward_fingerprint "${height}"
}

same_tip_state() {
  local first=""
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port info_json epose_json tip
    port="$(node_rpc_port "${i}")"
    if ! info_json="$(json_rpc "${port}" "get_info" 2>/dev/null)"; then
      return 1
    fi
    if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
      return 1
    fi
    tip="$(jq -r \
      --arg state_hash "$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')" \
      '.result | "\(.height):\(.top_block_hash):\($state_hash)"' \
      <<<"${info_json}")"
    if [ -z "${first}" ]; then
      first="${tip}"
    elif [ "${tip}" != "${first}" ]; then
      return 1
    fi
  done
}

wait_for_same_tip_state() {
  local deadline
  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while ! same_tip_state; do
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      echo "EPoSE local network did not converge to the same tip and state" >&2
      exit 1
    fi
    sleep 2
  done
}

converge_same_tip_state() {
  local anchor="${1:-0}"
  local deadline
  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while ! same_tip_state; do
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      echo "EPoSE local network did not converge to the same tip and state" >&2
      exit 1
    fi

    start_mining_node "${anchor}" "${MINING_THREADS}"
    sleep 5
    stop_mining_node "${anchor}"
    wait_for_node_mining_stopped "${anchor}"
    sleep 2
  done
}

same_epose_state() {
  local first=""
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port epose_json state
    port="$(node_rpc_port "${i}")"
    if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
      return 1
    fi
    state="$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')"
    if [ -z "${first}" ]; then
      first="${state}"
    elif [ "${state}" != "${first}" ]; then
      return 1
    fi
  done
}

wait_for_same_epose_state() {
  local deadline
  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while ! same_epose_state; do
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      echo "EPoSE local network did not converge to the same EPoSE state" >&2
      exit 1
    fi
    sleep 2
  done
}

all_nodes_locally_registered() {
  local i
  if [ "${SERVICE_NODE_COUNT}" -lt 1 ]; then
    return 1
  fi
  for i in $(seq 0 "$((SERVICE_NODE_COUNT - 1))"); do
    local port epose_json enabled registered service_count
    port="$(node_rpc_port "${i}")"
    if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
      return 1
    fi
    enabled="$(printf "%s" "${epose_json}" | jq -r '.result.enabled')"
    registered="$(printf "%s" "${epose_json}" | jq -r '.result.local_service_node_registered')"
    service_count="$(printf "%s" "${epose_json}" | jq -r '.result.service_node_count')"
    if [ "${enabled}" != "true" ] || [ "${registered}" != "true" ] || [ "${service_count}" -lt "${SERVICE_NODE_COUNT}" ]; then
      return 1
    fi
  done
}

node_locally_registered() {
  local i="$1"
  local port epose_json enabled registered service_count
  port="$(node_rpc_port "${i}")"
  if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
    return 1
  fi
  enabled="$(printf "%s" "${epose_json}" | jq -r '.result.enabled')"
  registered="$(printf "%s" "${epose_json}" | jq -r '.result.local_service_node_registered')"
  service_count="$(printf "%s" "${epose_json}" | jq -r '.result.service_node_count')"
  [ "${enabled}" = "true" ] && [ "${registered}" = "true" ] && [ "${service_count}" -ge 1 ]
}

all_nodes_reward_active() {
  local i
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port rewards_json service_reward_active qualified_count
    port="$(node_rpc_port "${i}")"
    if ! rewards_json="$(json_rpc "${port}" "get_service_rewards" 2>/dev/null)"; then
      return 1
    fi
    service_reward_active="$(printf "%s" "${rewards_json}" | jq -r '.result.service_reward_active')"
    qualified_count="$(printf "%s" "${rewards_json}" | jq -r '.result.qualified_count')"
    if [ "${service_reward_active}" != "true" ] || [ "${qualified_count}" -lt 1 ]; then
      return 1
    fi
  done
}

all_nodes_current_epoch_qualified() {
  local i
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port epose_json qualified_count
    port="$(node_rpc_port "${i}")"
    if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
      return 1
    fi
    qualified_count="$(printf "%s" "${epose_json}" | jq -r '.result.qualified_count')"
    if [ "${qualified_count}" -lt 1 ]; then
      return 1
    fi
  done
}

mine_node_until_registered() {
  local i="$1"
  local deadline
  deadline="$(($(now) + MINE_TIMEOUT))"
  start_mining_node "${i}" "${MINING_THREADS}"

  while true; do
    if node_locally_registered "${i}"; then
      stop_mining_node "${i}"
      wait_for_node_mining_stopped "${i}"
      converge_same_tip_state "${i}"
      if node_locally_registered "${i}"; then
        return
      fi
      start_mining_node "${i}" "${MINING_THREADS}"
    fi

    if [ "$(now)" -gt "${deadline}" ]; then
      stop_mining_node "${i}"
      print_registration_status >&2
      echo "EPoSE mined registration did not appear for $(node_name "${i}") before timeout" >&2
      exit 1
    fi

    sleep 2
  done
}

mine_until_all_nodes_registered() {
  local deadline i made_progress
  deadline="$(($(now) + MINE_TIMEOUT))"

  while true; do
    if all_nodes_locally_registered; then
      wait_for_same_tip_state
      return
    fi

    made_progress=0
    for i in $(seq 0 "$((SERVICE_NODE_COUNT - 1))"); do
      if ! node_locally_registered "${i}"; then
        mine_node_until_registered "${i}"
        made_progress=1
        if all_nodes_locally_registered; then
          wait_for_same_tip_state
          return
        fi
      fi
    done

    if [ "${made_progress}" -eq 0 ]; then
      converge_same_tip_state 0
    fi

    if [ "$(now)" -gt "${deadline}" ]; then
      print_registration_status >&2
      echo "EPoSE mined multi-node registration did not converge" >&2
      exit 1
    fi
  done
}

mine_until_current_epoch_qualified() {
  local deadline i
  deadline="$(($(now) + MINE_TIMEOUT))"

  while true; do
    if all_nodes_current_epoch_qualified; then
      wait_for_same_tip_state
      return
    fi

    for i in $(seq 0 "$((NODE_COUNT - 1))"); do
      start_mining_node "${i}" "${MINING_THREADS}"
      sleep 5
      stop_mining_node "${i}"
      wait_for_node_mining_stopped "${i}"
      converge_same_tip_state "${i}"
      if all_nodes_current_epoch_qualified; then
        return
      fi
    done

    if [ "$(now)" -gt "${deadline}" ]; then
      print_registration_status >&2
      echo "EPoSE current epoch did not qualify any service node before timeout" >&2
      exit 1
    fi
  done
}

mine_until_rewards_active() {
  local deadline i
  deadline="$(($(now) + MINE_TIMEOUT))"

  while true; do
    if all_nodes_reward_active; then
      wait_for_same_tip_state
      return
    fi

    for i in $(seq 0 "$((NODE_COUNT - 1))"); do
      start_mining_node "${i}" "${MINING_THREADS}"
      sleep 5
      stop_mining_node "${i}"
      wait_for_node_mining_stopped "${i}"
      converge_same_tip_state "${i}"
      if all_nodes_reward_active; then
        return
      fi
    done

    if [ "$(now)" -gt "${deadline}" ]; then
      print_registration_status >&2
      echo "EPoSE mined multi-node rewards did not become active before timeout" >&2
      exit 1
    fi
  done
}

assert_no_service_nodes_mining() {
  local i
  if [ "${SERVICE_NODE_COUNT}" -lt 1 ]; then
    echo "At least one service node is required" >&2
    exit 2
  fi
  for i in $(seq 0 "$((SERVICE_NODE_COUNT - 1))"); do
    if node_mining_active "${i}"; then
      echo "$(node_name "${i}") is mining but service-node mining must be disabled for this assertion" >&2
      exit 1
    fi
  done
}

assert_relay_non_service_miner() {
  require_node_count
  if [ "${NODE_COUNT}" -le "${SERVICE_NODE_COUNT}" ]; then
    echo "assert-relay-non-service-miner requires at least one non-service miner node" >&2
    exit 2
  fi
  if [ "${SERVICE_NODE_COUNT}" -lt 3 ]; then
    echo "assert-relay-non-service-miner requires at least 3 service nodes" >&2
    exit 2
  fi
  wait_for_rpc

  local miner_node deadline info_json height
  miner_node="$((NODE_COUNT - 1))"
  assert_no_service_nodes_mining
  deadline="$(($(now) + MINE_TIMEOUT))"

  while true; do
    if all_nodes_locally_registered && all_nodes_current_epoch_qualified && all_nodes_reward_active; then
      stop_mining_node "${miner_node}"
      wait_for_node_mining_stopped "${miner_node}"
      assert_no_service_nodes_mining
      wait_for_same_tip_state
      info_json="$(json_rpc "$(node_rpc_port "${miner_node}")" "get_info")"
      height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
      print_registration_status
      assert_same_service_rewards "${height}"
      return
    fi

    if [ "$(now)" -gt "${deadline}" ]; then
      stop_mining_node "${miner_node}"
      wait_for_node_mining_stopped "${miner_node}"
      print_registration_status >&2
      echo "EPoSE relay did not qualify service nodes with only non-service miner mining" >&2
      exit 1
    fi
    mine_node_blocks "${miner_node}" "${RELAY_MINE_BURST_BLOCKS}"
    wait_for_same_tip_state
    assert_no_service_nodes_mining
    sleep "${RELAY_IDLE_SECONDS}"
  done
}

print_registration_status() {
  local i
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port info_json epose_json height enabled registered service_count qualified_count state_hash
    port="$(node_rpc_port "${i}")"
    if ! info_json="$(json_rpc "${port}" "get_info" 2>/dev/null)"; then
      printf "%s rpc=unreachable port=%s\n" "$(node_name "${i}")" "${port}"
      continue
    fi
    if ! epose_json="$(json_rpc "${port}" "get_epose_info" 2>/dev/null)"; then
      printf "%s rpc=unreachable port=%s epose_rpc=failed\n" "$(node_name "${i}")" "${port}"
      continue
    fi
    height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
    enabled="$(printf "%s" "${epose_json}" | jq -r '.result.enabled')"
    registered="$(printf "%s" "${epose_json}" | jq -r '.result.local_service_node_registered')"
    service_count="$(printf "%s" "${epose_json}" | jq -r '.result.service_node_count')"
    qualified_count="$(printf "%s" "${epose_json}" | jq -r '.result.qualified_count')"
    state_hash="$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')"
    printf "%s height=%s epose_enabled=%s service_node_count=%s qualified_count=%s local_registered=%s epose_state_hash=%s\n" \
      "$(node_name "${i}")" "${height}" "${enabled}" "${service_count}" "${qualified_count}" "${registered}" "${state_hash}"
  done
}

assert_mined_network() {
  require_node_count
  wait_for_rpc

  local info_json height
  mine_until_all_nodes_registered

  mine_until_rewards_active
  info_json="$(json_rpc "$(node_rpc_port 0)" "get_info")"
  height="$(printf "%s" "${info_json}" | jq -r '.result.height')"
  print_registration_status
  assert_same_service_rewards "${height}"
}

assert_service_reward_finalized_epoch() {
  require_node_count
  wait_for_rpc

  local target_source_epoch target_height i port rewards_json reward_epoch active qualified
  target_source_epoch="${EPOSE_TARGET_REWARD_SOURCE_EPOCH:-1}"
  target_height="$(((target_source_epoch + 1) * EPOCH_LENGTH))"

  mine_until_all_nodes_registered
  mine_until_rewards_active
  mine_until_height 0 "$((target_source_epoch * EPOCH_LENGTH))"
  mine_until_current_epoch_qualified
  mine_until_height 0 "${target_height}"
  wait_for_same_tip_state

  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    port="$(node_rpc_port "${i}")"
    rewards_json="$(json_rpc_params "${port}" "get_service_rewards" "{\"height\":${target_height}}")"
    reward_epoch="$(printf "%s" "${rewards_json}" | jq -r '.result.epoch')"
    active="$(printf "%s" "${rewards_json}" | jq -r '.result.service_reward_active')"
    qualified="$(printf "%s" "${rewards_json}" | jq -r '.result.qualified_count')"
    if [ "${reward_epoch}" != "${target_source_epoch}" ] || [ "${active}" != "true" ] || [ "${qualified}" -lt 1 ]; then
      printf "%s\n" "${rewards_json}" >&2
      echo "EPoSE reward RPC did not use finalized reward source epoch ${target_source_epoch}" >&2
      exit 1
    fi
  done

  print_registration_status
  assert_same_service_rewards "${target_height}"
}

assert_same_tip() {
  require_node_count
  local first=""
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local port info_json epose_json tip
    port="$(node_rpc_port "${i}")"
    info_json="$(json_rpc "${port}" "get_info")"
    epose_json="$(json_rpc "${port}" "get_epose_info")"
    tip="$(jq -r \
      --arg state_hash "$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')" \
      '.result | "\(.height):\(.top_block_hash):\($state_hash)"' \
      <<<"${info_json}")"
    if [ -z "${first}" ]; then
      first="${tip}"
    elif [ "${tip}" != "${first}" ]; then
      status
      echo "EPoSE local network tips or state hashes differ" >&2
      exit 1
    fi
  done
  status
}

json_rpc_url() {
  local url="$1"
  local method="$2"
  curl -fsS -X POST "${url}/json_rpc" \
    -H "Content-Type: application/json" \
    -d "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"method\":\"${method}\",\"params\":{}}"
}

node_tip_state_from_url() {
  local url="$1"
  local info_json epose_json
  info_json="$(json_rpc_url "${url}" "get_info")"
  epose_json="$(json_rpc_url "${url}" "get_epose_info")"
  jq -r \
    --arg state_hash "$(printf "%s" "${epose_json}" | jq -r '.result.state_hash')" \
    '.result | "\(.height):\(.top_block_hash):\($state_hash)"' \
    <<<"${info_json}"
}

wait_for_fresh_node_rpc() {
  local url="$1"
  local name="$2"
  local deadline
  deadline="$(($(now) + RPC_READY_TIMEOUT))"
  while ! json_rpc_url "${url}" "get_epose_info" >/dev/null 2>&1; do
    if [ "$(now)" -gt "${deadline}" ]; then
      echo "RPC did not become ready for ${name}" >&2
      docker logs --tail=80 "${name}" >&2 || true
      exit 1
    fi
    sleep 2
  done
}

assert_fresh_sync_matches() {
  require_node_count
  wait_for_rpc
  assert_same_tip >/dev/null

  local name rpc_port url expected actual deadline
  name="${NETWORK_PREFIX}-qwc-epose-fresh-sync"
  rpc_port="$((RPC_BASE_PORT + NODE_COUNT))"
  url="http://127.0.0.1:${rpc_port}"
  expected="$(node_tip_state 0)"

  docker rm -f "${name}" >/dev/null 2>&1 || true
  docker run -d \
    --name "${name}" \
    --network "${NETWORK_NAME}" \
    --entrypoint /usr/local/bin/qwertycoind \
    --tmpfs /root/.qwertycoin:rw,size=2g \
    -p "127.0.0.1:${rpc_port}:8197" \
    -e "MONERO_RANDOMX_UMASK=${RANDOMX_UMASK}" \
    "${IMAGE}" \
      --testnet \
      --non-interactive \
      --p2p-bind-ip=0.0.0.0 \
      --p2p-bind-port=8196 \
      --rpc-bind-ip=0.0.0.0 \
      --rpc-bind-port=8197 \
      --confirm-external-bind \
      --no-igd \
      --hide-my-port \
      --disable-dns-checkpoints \
      --fixed-difficulty="${FIXED_DIFFICULTY}" \
      --add-priority-node=qwc-epose-node-0:8196 >/dev/null

  trap 'docker rm -f "${name}" >/dev/null 2>&1 || true' RETURN
  wait_for_fresh_node_rpc "${url}" "${name}"

  deadline="$(($(now) + CONVERGENCE_TIMEOUT))"
  while true; do
    actual="$(node_tip_state_from_url "${url}")"
    if [ "${actual}" = "${expected}" ]; then
      printf "%s height=%s top_hash=%s epose_state_hash=%s\n" \
        "${name}" \
        "$(printf "%s" "${actual}" | cut -d: -f1)" \
        "$(printf "%s" "${actual}" | cut -d: -f2)" \
        "$(printf "%s" "${actual}" | cut -d: -f3)"
      docker rm -f "${name}" >/dev/null 2>&1 || true
      trap - RETURN
      return
    fi
    if [ "$(now)" -gt "${deadline}" ]; then
      status
      docker logs --tail=80 "${name}" >&2 || true
      echo "Fresh synced validator did not converge to node 0 tip and EPoSE state" >&2
      echo "expected=${expected}" >&2
      echo "actual=${actual}" >&2
      exit 1
    fi
    sleep 2
  done
}

partition() {
  require_node_count
  compose up -d --no-recreate
  docker network inspect "${PARTITION_A_NETWORK}" >/dev/null 2>&1 || docker network create "${PARTITION_A_NETWORK}" >/dev/null
  docker network inspect "${PARTITION_B_NETWORK}" >/dev/null 2>&1 || docker network create "${PARTITION_B_NETWORK}" >/dev/null
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local name container target
    name="$(node_name "${i}")"
    container="$(compose ps -q "${name}")"
    target="${PARTITION_A_NETWORK}"
    if [ "${i}" -ge "$((NODE_COUNT / 2))" ]; then
      target="${PARTITION_B_NETWORK}"
    fi
    docker network disconnect "${NETWORK_NAME}" "${container}" >/dev/null 2>&1 || true
    docker network connect --alias "${name}" "${target}" "${container}" >/dev/null 2>&1 || true
  done
}

heal() {
  require_node_count
  compose up -d --no-recreate
  for i in $(seq 0 "$((NODE_COUNT - 1))"); do
    local name container
    name="$(node_name "${i}")"
    container="$(compose ps -q "${name}")"
    docker network disconnect "${PARTITION_A_NETWORK}" "${container}" >/dev/null 2>&1 || true
    docker network disconnect "${PARTITION_B_NETWORK}" "${container}" >/dev/null 2>&1 || true
    docker network connect --alias "${name}" "${NETWORK_NAME}" "${container}" >/dev/null 2>&1 || true
  done
}

assert_partition_heal() {
  require_node_count
  assert_same_tip >/dev/null
  partition
  heal
  wait_for_rpc
  assert_same_tip
}

assert_partition_heal_reorg() {
  require_node_count
  if [ "${NODE_COUNT}" -lt 3 ]; then
    echo "assert-partition-heal-reorg requires at least 3 nodes" >&2
    exit 2
  fi

  assert_same_tip >/dev/null

  local split b_start a_tip b_tip final_tip height
  split="$((NODE_COUNT / 2))"
  b_start="${split}"

  partition

  mine_node_for_seconds_any_network 0 1
  mine_node_for_seconds_any_network "${b_start}" 8

  a_tip="$(node_tip_state_any_network 0)"
  b_tip="$(node_tip_state_any_network "${b_start}")"
  if [ "${a_tip}" = "${b_tip}" ]; then
    status
    echo "EPoSE partition did not create competing chain tips" >&2
    exit 1
  fi

  heal
  compose down
  start_network
  wait_for_same_tip_state_any_network

  final_tip="$(node_tip_state_any_network 0)"
  if [ "${final_tip}" = "${a_tip}" ]; then
    status
    echo "EPoSE partition heal did not reorg away the shorter branch" >&2
    exit 1
  fi

  compose down
  start_network
  status
  height="$(printf "%s" "${final_tip}" | cut -d: -f1)"
  assert_same_service_rewards "${height}"
}

assert_epoch_boundary_reorg() {
  require_node_count
  if [ "${NODE_COUNT}" -lt 3 ]; then
    echo "assert-epoch-boundary-reorg requires at least 3 nodes" >&2
    exit 2
  fi

  wait_for_rpc
  mine_until_all_nodes_registered
  mine_until_current_epoch_qualified

  local boundary_height split b_start a_tip b_tip final_tip final_height
  boundary_height="${EPOSE_REORG_BOUNDARY_HEIGHT:-${EPOCH_LENGTH}}"
  split="$((NODE_COUNT / 2))"
  b_start="${split}"

  mine_until_height 0 "${boundary_height}"
  wait_for_same_tip_state
  assert_same_service_rewards "${boundary_height}"

  partition
  mine_node_blocks_any_network 0 "${EPOSE_REORG_SHORT_BRANCH_BLOCKS:-1}"
  mine_node_blocks_any_network "${b_start}" "${EPOSE_REORG_LONG_BRANCH_BLOCKS:-4}"

  a_tip="$(node_tip_state_any_network 0)"
  b_tip="$(node_tip_state_any_network "${b_start}")"
  if [ "${a_tip}" = "${b_tip}" ]; then
    status
    echo "EPoSE epoch-boundary partition did not create competing chain tips" >&2
    exit 1
  fi

  heal
  compose down
  start_network
  wait_for_same_tip_state_any_network

  final_tip="$(node_tip_state_any_network 0)"
  final_height="$(printf "%s" "${final_tip}" | cut -d: -f1)"
  if [ "${final_tip}" = "${a_tip}" ]; then
    status
    echo "EPoSE epoch-boundary heal did not reorg away the shorter branch" >&2
    exit 1
  fi
  if [ "${final_height}" -le "${boundary_height}" ]; then
    status
    echo "EPoSE epoch-boundary reorg did not progress past boundary height ${boundary_height}" >&2
    exit 1
  fi

  status
  assert_same_service_rewards "${final_height}"

  compose down
  start_network
  wait_for_same_tip_state
  assert_same_service_rewards "${final_height}"
}

assert_sigkill_recovery() {
  require_node_count
  if [ "${NODE_COUNT}" -lt 2 ]; then
    echo "assert-sigkill-recovery requires at least 2 nodes" >&2
    exit 2
  fi

  wait_for_rpc
  mine_until_all_nodes_registered
  mine_until_current_epoch_qualified
  wait_for_same_tip_state

  local victim="${EPOSE_CHAOS_VICTIM:-$((NODE_COUNT - 1))}"
  local anchor="${EPOSE_CHAOS_ANCHOR:-0}"
  local before after_restart after_progress final_height
  before="$(network_fingerprint)"

  docker kill --signal=KILL "$(compose ps -q "$(node_name "${victim}")")" >/dev/null
  compose up -d --no-recreate "$(node_name "${victim}")" >/dev/null
  wait_for_node_rpc "${victim}"
  wait_for_same_tip_state

  after_restart="$(network_fingerprint)"
  if [ "${after_restart}" != "${before}" ]; then
    printf "%s\n" "${after_restart}"
    echo "EPoSE state changed after SIGKILL restart without chain progress" >&2
    exit 1
  fi

  mine_node_blocks "${anchor}" "${EPOSE_CHAOS_POST_RESTART_BLOCKS:-5}"
  wait_for_same_tip_state

  after_progress="$(network_fingerprint)"
  final_height="$(node_tip_state 0 | cut -d: -f1)"
  printf "%s\n" "${after_progress}"
  assert_same_service_rewards "${final_height}"
}

command="${1:-}"
case "${command}" in
  generate)
    generate_compose
    ;;
  up)
    generate_compose
    start_network
    status
    ;;
  down)
    compose down
    ;;
  clean)
    compose down -v
    ;;
  status)
    status
    ;;
  assert-same-tip)
    assert_same_tip
    ;;
  assert-restart-persists)
    assert_restart_persists
    ;;
  assert-mined-registration)
    assert_mined_registration
    ;;
  assert-mined-registration-restart-persists)
    assert_mined_registration_restart_persists
    ;;
  assert-mined-network)
    assert_mined_network
    ;;
  assert-relay-non-service-miner)
    assert_relay_non_service_miner
    ;;
  assert-service-reward-finalized-epoch)
    assert_service_reward_finalized_epoch
    ;;
  assert-fresh-sync-matches)
    assert_fresh_sync_matches
    ;;
  assert-partition-heal)
    assert_partition_heal
    ;;
  assert-partition-heal-reorg)
    assert_partition_heal_reorg
    ;;
  assert-epoch-boundary-reorg)
    assert_epoch_boundary_reorg
    ;;
  assert-sigkill-recovery)
    assert_sigkill_recovery
    ;;
  partition)
    partition
    ;;
  heal)
    heal
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
