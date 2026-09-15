#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <image-reference> <expected-version>" >&2
  exit 64
fi

image=$1
expected_version=$2
[[ "$expected_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-rc[1-9][0-9]*)?$ ]] || {
  echo "invalid expected version" >&2
  exit 64
}
command -v docker >/dev/null || { echo "docker is required" >&2; exit 1; }
command -v curl >/dev/null || { echo "curl is required" >&2; exit 1; }

suffix="${GITHUB_RUN_ID:-local}-${RANDOM}"
suffix=${suffix//[^A-Za-z0-9_.-]/-}
prefix="qwc-docker-smoke-$suffix"
network="$prefix-net"
chain_volume="$prefix-chain"
identity_volume="$prefix-identity"
wallet_volume="$prefix-wallet"
config_volume="$prefix-config"
daemon_name="$prefix-daemon"
wallet_name="$prefix-wallet-rpc"
temporary=$(mktemp -d)

cleanup() {
  docker rm --force "$wallet_name" "$daemon_name" >/dev/null 2>&1 || true
  docker network rm "$network" >/dev/null 2>&1 || true
  docker volume rm "$chain_volume" "$identity_volume" "$wallet_volume" "$config_volume" >/dev/null 2>&1 || true
  if command -v trash >/dev/null 2>&1; then
    trash "$temporary" >/dev/null 2>&1 || true
  else
    find "$temporary" -type f -exec shred -u {} + 2>/dev/null || true
    rmdir "$temporary" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

version_marker="v${expected_version%%-rc*}-release"
check_program() {
  local selector=$1 output
  if [[ -n "$selector" ]]; then
    output=$(docker run --rm "$image" "$selector" --version 2>&1)
    docker run --rm "$image" "$selector" --help >/dev/null
  else
    output=$(docker run --rm "$image" --version 2>&1)
    docker run --rm "$image" --help >/dev/null
  fi
  grep -F "$version_marker" <<<"$output" >/dev/null || {
    echo "program selector '${selector:-daemon-default}' did not report $version_marker" >&2
    exit 1
  }
}

check_program ""
check_program daemon
check_program wallet
check_program wallet-rpc

actual_programs=$(docker run --rm --entrypoint /bin/sh "$image" -ec \
  'find /opt/qwertycoin -maxdepth 1 -type f -perm /111 -printf "%f\n" | LC_ALL=C sort')
expected_programs=$(printf '%s\n' qwertycoin-wallet-cli qwertycoin-wallet-rpc qwertycoind)
test "$actual_programs" = "$expected_programs"

test "$(docker image inspect --format '{{.Config.User}}' "$image")" = "10001:10001"
test "$(docker image inspect --format '{{json .Config.Entrypoint}}' "$image")" = '["/usr/local/bin/qwertycoin-entrypoint"]'
test "$(docker image inspect --format '{{.Config.StopSignal}}' "$image")" = SIGINT

docker network create --internal "$network" >/dev/null
for volume in "$chain_volume" "$identity_volume" "$wallet_volume" "$config_volume"; do
  docker volume create "$volume" >/dev/null
done
docker run --rm --user 0:0 \
  -v "$chain_volume:/data" -v "$identity_volume:/service-node" -v "$wallet_volume:/wallet" \
  --entrypoint /bin/sh "$image" -ec \
  'install -d -o 10001 -g 10001 -m 0700 /data /service-node /wallet'

reward_address=QWC1fGDnm8EVSfVVGp55GEdVqDpWjzpAXhSX4VHBz8xqLwvAC9HTWaSEy3rTvyLW7f9meNKDL4sKAJ46UYmPUaa68q7pouWt57

start_daemon() {
  docker run --detach --name "$daemon_name" --network "$network" --network-alias daemon \
    --read-only --tmpfs /tmp:rw,noexec,nosuid,nodev,size=64m \
    --cap-drop ALL --security-opt no-new-privileges:true \
    -v "$chain_volume:/data" -v "$identity_volume:/service-node" \
    "$image" daemon \
      --data-dir=/data \
      --epose-v2-service \
      --epose-v2-keystore=/service-node/epose-v2-keystore \
      --epose-v2-reward-address="$reward_address" \
      --epose-v2-endpoint-host=smoke.invalid \
      --epose-v2-endpoint-port=8198 \
      --epose-v2-discovery-endpoint=http://127.0.0.1:65534 \
      --p2p-bind-ip=127.0.0.1 \
      --p2p-bind-port=8196 \
      --rpc-bind-ip=0.0.0.0 \
      --rpc-bind-port=8197 \
      --rpc-restricted-bind-ip=127.0.0.1 \
      --rpc-restricted-bind-port=8198 \
      --confirm-external-bind \
      --no-zmq \
      --non-interactive \
      --max-concurrency=1 \
      --log-level=0 >/dev/null
}

container_ip() {
  docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$1"
}

wait_for_daemon() {
  local deadline
  deadline=$((SECONDS + 120))
  while (( SECONDS < deadline )); do
    if docker logs "$daemon_name" 2>&1 | grep -F 'Starting p2p net loop' >/dev/null && \
       docker run --rm -v "$identity_volume:/service-node:ro" --entrypoint /bin/sh "$image" -ec \
         'test -s /service-node/epose-v2-keystore'; then
      return 0
    fi
    if [[ "$(docker inspect --format '{{.State.Running}}' "$daemon_name" 2>/dev/null || true)" != true ]]; then
      echo "network-isolated EPoSe daemon exited before RPC became ready" >&2
      docker logs --tail 80 "$daemon_name" >&2 || true
      return 1
    fi
    sleep 1
  done
  echo "timed out waiting for network-isolated EPoSe daemon" >&2
  return 1
}

stop_daemon() {
  docker stop --time 120 "$daemon_name" >/dev/null
  test "$(docker inspect --format '{{.State.ExitCode}}' "$daemon_name")" = 0
  docker rm "$daemon_name" >/dev/null
}

start_daemon
wait_for_daemon
daemon_ip=$(container_ip "$daemon_name")
daemon_info=$(curl --silent --show-error --max-time 5 \
  -H 'Content-Type: application/json' \
  --data '{"jsonrpc":"2.0","id":"0","method":"get_info"}' \
  "http://${daemon_ip}:8197/json_rpc")
grep -F '"nettype":"mainnet"' <<<"${daemon_info// /}" >/dev/null
docker run --rm -v "$identity_volume:/service-node:ro" --entrypoint /bin/sh "$image" -ec \
  'test -s /service-node/epose-v2-keystore'
docker run --rm -v "$chain_volume:/data:ro" --entrypoint /bin/sh "$image" -ec \
  'test -d /data/lmdb'
identity_before=$(docker run --rm -v "$identity_volume:/service-node:ro" --entrypoint sha256sum "$image" \
  /service-node/epose-v2-keystore | awk '{print $1}')

rpc_username=docker-smoke
rpc_password=$(od -An -N24 -tx1 /dev/urandom | tr -d ' \n')
{
  printf '%s\n' \
    'wallet-dir=/wallet' \
    'shared-ringdb-dir=/wallet/shared-ringdb' \
    'rpc-bind-ip=0.0.0.0' \
    'rpc-bind-port=8198' \
    'confirm-external-bind=1' \
    "rpc-login=${rpc_username}:${rpc_password}" \
    'daemon-address=http://daemon:8197' \
    'trusted-daemon=1' \
    'rpc-ssl=disabled' \
    'daemon-ssl=disabled' \
    'log-file=/tmp/wallet-rpc.log' \
    'log-level=0'
} | docker run --rm --interactive --user 0:0 -v "$config_volume:/config" \
  --entrypoint /bin/sh "$image" -ec \
  'umask 077; cat > /config/wallet-rpc.conf; chown 10001:10001 /config/wallet-rpc.conf'
cat >"$temporary/curl.conf" <<EOF
silent
show-error
max-time = 5
digest
user = "${rpc_username}:${rpc_password}"
header = "Content-Type: application/json"
EOF
chmod 0600 "$temporary/curl.conf"
unset rpc_password
docker run --rm -v "$wallet_volume:/wallet" --entrypoint /bin/sh "$image" -ec \
  'touch /wallet/.packaging-write-test && test -O /wallet/.packaging-write-test'

start_wallet_rpc() {
  docker run --detach --name "$wallet_name" --network "$network" \
    --log-driver none \
    --read-only --tmpfs /tmp:rw,noexec,nosuid,nodev,size=64m \
    --cap-drop ALL --security-opt no-new-privileges:true \
    -v "$wallet_volume:/wallet" -v "$config_volume:/run/secrets:ro" \
    "$image" wallet-rpc --config-file=/run/secrets/wallet-rpc.conf >/dev/null
}
wait_for_wallet_rpc() {
  local deadline
  deadline=$((SECONDS + 30))
  while (( SECONDS < deadline )); do
    if [[ "$(docker inspect --format '{{.State.Running}}' "$wallet_name" 2>/dev/null || true)" != true ]]; then
      echo "wallet RPC exited before becoming ready" >&2
      return 1
    fi
    if (( SECONDS + 20 >= deadline )); then
      docker exec "$wallet_name" /bin/sh -ec \
        'test -r /run/secrets/wallet-rpc.conf && test -w /wallet'
      return 0
    fi
    sleep 1
  done
  echo "wallet RPC did not become ready" >&2
  return 1
}
stop_wallet_rpc() {
  docker stop --time 120 "$wallet_name" >/dev/null
  test "$(docker inspect --format '{{.State.ExitCode}}' "$wallet_name")" = 0
  docker rm "$wallet_name" >/dev/null
}

start_wallet_rpc
wait_for_wallet_rpc
wallet_ip=$(container_ip "$wallet_name")
unauthenticated_status=$(curl --silent --output /dev/null --write-out '%{http_code}' --max-time 5 \
  -H 'Content-Type: application/json' \
  --data '{"jsonrpc":"2.0","id":"0","method":"get_version"}' \
  "http://${wallet_ip}:8198/json_rpc")
test "$unauthenticated_status" = 401
wallet_response=$(curl --config "$temporary/curl.conf" \
  --data '{"jsonrpc":"2.0","id":"0","method":"get_version"}' \
  "http://${wallet_ip}:8198/json_rpc")
grep -F '"result"' <<<"${wallet_response// /}" >/dev/null
stop_wallet_rpc
docker run --rm -v "$wallet_volume:/wallet:ro" --entrypoint /bin/sh "$image" -ec \
  'test -f /wallet/.packaging-write-test'

stop_daemon
start_daemon
wait_for_daemon
identity_after=$(docker run --rm -v "$identity_volume:/service-node:ro" --entrypoint sha256sum "$image" \
  /service-node/epose-v2-keystore | awk '{print $1}')
test "$identity_after" = "$identity_before"
stop_daemon

echo "Docker packaging smoke passed: selectors, network-isolated mainnet daemon, wallet RPC auth, clean shutdown and persistent EPoSe identity"
