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
: "${QWC_SERVICE_NODE_KEY_PATH:=/service-node/service-node.key}"
: "${QWC_REWARD_ADDRESS:?QWC_REWARD_ADDRESS is required}"
: "${QWC_REWARD_VIEW_KEY:?QWC_REWARD_VIEW_KEY is required}"
: "${QWC_ADVERTISE_ADDRESS:?QWC_ADVERTISE_ADDRESS is required}"
: "${QWC_P2P_PORT:=8196}"
: "${QWC_RPC_PORT:=8197}"
: "${QWC_RESTRICTED_RPC_PORT:=}"
: "${QWC_ZMQ_RPC_PORT:=8199}"
: "${QWC_MAX_CONNECTIONS_PER_IP:=4}"
: "${QWC_PRIORITY_NODES:=}"
: "${QWC_PRIORITY_NODE_1:=}"
: "${QWC_PRIORITY_NODE_2:=}"
: "${QWC_NETWORK_NAME:=}"
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
  --service-node
  "--service-node-key=$QWC_SERVICE_NODE_KEY_PATH"
  "--service-reward-address=$QWC_REWARD_ADDRESS"
  "--service-reward-view-key=$QWC_REWARD_VIEW_KEY"
  "--service-node-advertise-address=$QWC_ADVERTISE_ADDRESS"
  --p2p-bind-ip=0.0.0.0
  "--p2p-bind-port=$QWC_P2P_PORT"
  --rpc-bind-ip=0.0.0.0
  "--rpc-bind-port=$QWC_RPC_PORT"
  "--zmq-rpc-bind-port=$QWC_ZMQ_RPC_PORT"
  --non-interactive
  --confirm-external-bind
  --no-igd
  --hide-my-port
  "--max-connections-per-ip=$QWC_MAX_CONNECTIONS_PER_IP"
  --disable-dns-checkpoints
)

if [[ -n "$QWC_RESTRICTED_RPC_PORT" ]]; then
  args+=(
    --rpc-restricted-bind-ip=0.0.0.0
    "--rpc-restricted-bind-port=$QWC_RESTRICTED_RPC_PORT"
  )
fi

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

ports=(
  -p "$QWC_P2P_PORT:$QWC_P2P_PORT"
  -p "127.0.0.1:$QWC_RPC_PORT:$QWC_RPC_PORT"
  -p "127.0.0.1:$QWC_ZMQ_RPC_PORT:$QWC_ZMQ_RPC_PORT"
)

if [[ -n "$QWC_RESTRICTED_RPC_PORT" ]]; then
  ports+=(-p "127.0.0.1:$QWC_RESTRICTED_RPC_PORT:$QWC_RESTRICTED_RPC_PORT")
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
