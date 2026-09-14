#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <node-env-file>" >&2
  exit 64
fi

env_file="$1"
if [[ ! -f "$env_file" ]]; then
  echo "missing env file: $env_file" >&2
  exit 66
fi

# shellcheck disable=SC1090
source "$env_file"

: "${QWC_IMAGE:?QWC_IMAGE is required}"
: "${QWC_CONTAINER_NAME:=qwertycoin-mainnet}"
: "${QWC_DATA_VOLUME:?QWC_DATA_VOLUME is required}"
: "${QWC_IDENTITY_VOLUME:?QWC_IDENTITY_VOLUME is required}"
: "${QWC_EPOSE_V2_KEYSTORE_PATH:=/service-node/epose-v2-keystore}"
: "${QWC_REWARD_ADDRESS:?QWC_REWARD_ADDRESS is required}"
: "${QWC_EPOSE_V2_ENDPOINT_HOST:?QWC_EPOSE_V2_ENDPOINT_HOST is required}"
: "${QWC_EPOSE_V2_ENDPOINT_PORT:=8198}"
: "${QWC_EPOSE_V2_DISCOVERY_ENDPOINTS:?QWC_EPOSE_V2_DISCOVERY_ENDPOINTS is required}"
: "${QWC_P2P_PORT:=8196}"
: "${QWC_RPC_PORT:=8197}"
: "${QWC_RESTRICTED_RPC_PORT:=}"
: "${QWC_RESTRICTED_RPC_HOST_IP:=0.0.0.0}"
: "${QWC_ZMQ_RPC_PORT:=8199}"
: "${QWC_MAX_CONNECTIONS_PER_IP:=4}"
: "${QWC_PRIORITY_NODES:=}"
: "${QWC_PRIORITY_NODE_1:=}"
: "${QWC_PRIORITY_NODE_2:=}"
: "${QWC_NETWORK_NAME:=}"
: "${QWC_RPC_BAN_EXEMPT_ADDRESSES:=}"
: "${QWC_RESET_CHAIN:=0}"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required" >&2
  exit 69
fi

if docker ps -a --format '{{.Names}}' | grep -Fxq "$QWC_CONTAINER_NAME"; then
  docker stop "$QWC_CONTAINER_NAME" >/dev/null 2>&1 || true
  docker rm "$QWC_CONTAINER_NAME" >/dev/null
fi

if [[ "$QWC_RESET_CHAIN" == "1" ]]; then
  docker volume rm "$QWC_DATA_VOLUME" >/dev/null 2>&1 || true
fi

docker volume create "$QWC_DATA_VOLUME" >/dev/null
docker volume create "$QWC_IDENTITY_VOLUME" >/dev/null

args=(
  --epose-v2-service
  "--epose-v2-keystore=$QWC_EPOSE_V2_KEYSTORE_PATH"
  "--epose-v2-reward-address=$QWC_REWARD_ADDRESS"
  "--epose-v2-endpoint-host=$QWC_EPOSE_V2_ENDPOINT_HOST"
  "--epose-v2-endpoint-port=$QWC_EPOSE_V2_ENDPOINT_PORT"
  --p2p-bind-ip=0.0.0.0
  "--p2p-bind-port=$QWC_P2P_PORT"
  --rpc-bind-ip=0.0.0.0
  "--rpc-bind-port=$QWC_RPC_PORT"
  --zmq-rpc-bind-ip=0.0.0.0
  "--zmq-rpc-bind-port=$QWC_ZMQ_RPC_PORT"
  --confirm-zmq-rpc-external-bind
  --non-interactive
  --confirm-external-bind
  --no-igd
  "--max-connections-per-ip=$QWC_MAX_CONNECTIONS_PER_IP"
  --disable-dns-checkpoints
)

if [[ -n "$QWC_RESTRICTED_RPC_PORT" ]]; then
  args+=(
    --rpc-restricted-bind-ip=0.0.0.0
    "--rpc-restricted-bind-port=$QWC_RESTRICTED_RPC_PORT"
  )
fi

IFS=',' read -r -a rpc_ban_exempt_addresses <<< "$QWC_RPC_BAN_EXEMPT_ADDRESSES"
for address in "${rpc_ban_exempt_addresses[@]}"; do
  address="${address//[[:space:]]/}"
  if [[ -n "$address" ]]; then
    args+=("--rpc-ban-exempt-address=$address")
  fi
done

if [[ -z "$QWC_PRIORITY_NODES" ]]; then
  QWC_PRIORITY_NODES="${QWC_PRIORITY_NODE_1},${QWC_PRIORITY_NODE_2}"
fi

IFS=',' read -r -a priority_nodes <<< "$QWC_PRIORITY_NODES"
for node in "${priority_nodes[@]}"; do
  node="${node//[[:space:]]/}"
  if [[ -n "$node" ]]; then
    args+=("--add-priority-node=$node")
  fi
done

IFS=',' read -r -a discovery_endpoints <<< "$QWC_EPOSE_V2_DISCOVERY_ENDPOINTS"
for endpoint in "${discovery_endpoints[@]}"; do
  endpoint="${endpoint//[[:space:]]/}"
  if [[ -n "$endpoint" ]]; then
    args+=("--epose-v2-discovery-endpoint=$endpoint")
  fi
done

ports=(
  -p "$QWC_P2P_PORT:$QWC_P2P_PORT"
  -p "127.0.0.1:$QWC_RPC_PORT:$QWC_RPC_PORT"
  -p "127.0.0.1:$QWC_ZMQ_RPC_PORT:$QWC_ZMQ_RPC_PORT"
)

if [[ -n "$QWC_RESTRICTED_RPC_PORT" ]]; then
  ports+=(-p "$QWC_RESTRICTED_RPC_HOST_IP:$QWC_RESTRICTED_RPC_PORT:$QWC_RESTRICTED_RPC_PORT")
fi

network_args=()
if [[ -n "$QWC_NETWORK_NAME" ]]; then
  docker network inspect "$QWC_NETWORK_NAME" >/dev/null 2>&1 || docker network create "$QWC_NETWORK_NAME" >/dev/null
  network_args+=(--network "$QWC_NETWORK_NAME")
fi

docker run -d \
  --name "$QWC_CONTAINER_NAME" \
  --restart unless-stopped \
  "${network_args[@]}" \
  "${ports[@]}" \
  -v "$QWC_DATA_VOLUME:/home/qwertycoin/.qwertycoin" \
  -v "$QWC_IDENTITY_VOLUME:/service-node" \
  "$QWC_IMAGE" \
  "${args[@]}"
