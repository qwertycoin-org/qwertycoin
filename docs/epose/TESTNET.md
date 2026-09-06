# Testnet

> Historical snapshot - not current protocol documentation. This file remains
> useful for older harness notes, but current `main` is documented in
> `README.md`, `PROTOCOL.md`, `IMPLEMENTATION_REPORT.md`, and
> `MAINNET_VALIDATION.md`.

## Current Two-Server Baseline

Node A:

```text
seed host A
/opt/docker/qwertycoin-testnode
P2P 8196
RPC 127.0.0.1:8197
```

Node B:

```text
seed host B
<host-deploy-path>
P2P 8196
RPC 127.0.0.1:8197
```

Both nodes currently run `--testnet` with mutual priority nodes and the
host-specific compose files under `deploy/testnet-beta/`, which keep P2P public
and bind daemon/ZMQ RPC to localhost only.

Latest two-server HF17 beta seed check on 2026-08-29:

```text
image: qwertycoin-v2-node:v2.0.0-beta.1
binary version: Qwertycoin 'EPoSE Testnet Beta' (v2.0.0-beta.1-6d2177a66)
seed host A image manifest: sha256:49f40160062461a5d7d1246633d1f0ad7e43d1ff6e383094a8aead34b252c1fc
seed host B image manifest: sha256:49f40160062461a5d7d1246633d1f0ad7e43d1ff6e383094a8aead34b252c1fc
image config: sha256:3214e16df9c43aad521075cce19a5754a7ea110caf914b13f773f0eebec138ad
ports: 0.0.0.0:8196->8196/tcp, 127.0.0.1:8197->8197/tcp, 127.0.0.1:8199->8199/tcp
height: 1
top hash: 415c36bf6dd59821aab2d2792a718be9f57f122f4eb611f649899231434f0a4e
peers: 1 incoming, 1 outgoing on each host
P2P reachability: both seed host P2P ports open
EPoSE enabled: true at height 1
EPoSE protocol: 1
EPoSE epoch: 0
EPoSE state hash: 09eca94b8c3c2b1757e9c6a7143ade035ac98472b6f2d0c1e7cc2b47352f74f5
service nodes: 0
```

The final image was rebuilt from commit `6d2177a66` with a serialized
`NPROC=1` depends build on seed host A after parallel Docker builds exhausted the
small build host. The same image archive was loaded on seed host B, and both seed
containers were recreated from the identical image manifest.

Both seed datadirs were reset for the HF17 beta chain. Previous development
chain volumes were archived before removal. The browser explorer validation
endpoint was restarted after the seed host A chain volume reset.

The current QWC v2 compiled default ports are:

```text
mainnet:  P2P 8196, daemon RPC 8197, wallet RPC 8198, ZMQ RPC 8199
testnet:  P2P 8196, daemon RPC 8197, wallet RPC 8198, ZMQ RPC 8199
stagenet: P2P 8196, daemon RPC 8197, wallet RPC 8198, ZMQ RPC 8199
```

The QWC v2 testnet beta is a new chain. Existing development chain data from
earlier EPoSE/HF activation experiments is incompatible and must be deleted
before joining the beta network.

## Testnet Beta Candidate Baseline

The beta candidate consensus baseline is:

```text
genesis -> inherited current Monero consensus rules -> QWC HF17 / EPoSE active
```

Mainnet, testnet, and stagenet hardfork schedules start directly at
`HF_VERSION_QWC_EPOSE = 17` from height `0`, with EPoSE format version 2.
Historical Monero hardforks are
not replayed through later activation heights.

Current configured network constants:

```text
mainnet:
  network id: QWC2MAIN2026POC
  p2p: 8196
  daemon rpc: 8197
  wallet rpc: 8198
  zmq rpc: 8199
  genesis nonce: 10000

testnet:
  network id: QWC2TEST2026POC
  p2p: 8196
  daemon rpc: 8197
  wallet rpc: 8198
  zmq rpc: 8199
  genesis nonce: 10001

stagenet:
  network id: QWC2STAG2026POC
  p2p: 8196
  daemon rpc: 8197
  wallet rpc: 8198
  zmq rpc: 8199
  genesis nonce: 10002
```

The final public testnet beta genesis must be treated as immutable once
published. Do not reuse old development datadirs after the beta genesis is
announced.

## Required Baseline Tests

The following must be run before claiming EPoSE integration complete:

```text
A mines -> B validates
B mines -> A validates
wallet A sends to wallet B
wallet B sends to wallet A
stop/restart both nodes
partition/reconnect
reorg across epoch boundary
pruned/full sync combinations
```

No successful test should be documented without command, environment, and result.

## Local Multi-Node Harness

The first reproducible local EPoSE network harness lives in:

```text
tests/epose/integration/local_epose_network.sh
```

It generates a Docker Compose file under `.epose-local-net/` and supports 5, 10, and 25 logical nodes through `EPOSE_NODE_COUNT`.

Example:

```bash
docker build --build-arg NPROC=2 -f Dockerfile.epose-dev -t qwertycoin-v2-node:epose-dev-tests .
EPOSE_NODE_COUNT=5 tests/epose/integration/local_epose_network.sh up
EPOSE_NODE_COUNT=5 tests/epose/integration/local_epose_network.sh assert-same-tip
EPOSE_NODE_COUNT=5 tests/epose/integration/local_epose_network.sh assert-restart-persists
EPOSE_NODE_COUNT=1 EPOSE_OFFLINE=1 tests/epose/integration/local_epose_network.sh assert-mined-registration
EPOSE_NODE_COUNT=1 EPOSE_OFFLINE=1 tests/epose/integration/local_epose_network.sh assert-mined-registration-restart-persists
EPOSE_NODE_COUNT=5 tests/epose/integration/local_epose_network.sh assert-fresh-sync-matches
EPOSE_NODE_COUNT=5 tests/epose/integration/local_epose_network.sh assert-partition-heal
EPOSE_NODE_COUNT=3 tests/epose/integration/local_epose_network.sh assert-epoch-boundary-reorg
EPOSE_NODE_COUNT=3 tests/epose/integration/local_epose_network.sh assert-sigkill-recovery
EPOSE_NODE_COUNT=5 tests/epose/integration/local_epose_network.sh down
```

This harness currently automates node startup, JSON-RPC readiness, tip plus EPoSE state-hash comparison, container recreation with persisted volumes, fresh non-service validator sync from an empty datadir, SIGKILL/restart recovery, partition/heal verification, epoch-boundary competing-branch reorg/restart checks, a single-node mined-registration smoke, and a mined-registration restart persistence check. It binds RPC ports on localhost by default, keeps P2P internal to the Docker bridge, persists the daemon datadir at `/root/.qwertycoin`, commits each service node to its internal `qwc-epose-node-N:8196` advertised endpoint, uses explicit Docker network names so partition/heal commands can target running container IDs reliably, accepts `EPOSE_RPC_READY_TIMEOUT` for larger networks, accepts `EPOSE_COMPOSE_PARALLEL_LIMIT` to throttle large Compose operations, accepts `EPOSE_START_BATCH_SIZE` to start large networks in readiness-gated batches, accepts `EPOSE_FIXED_DIFFICULTY` for deterministic local mining, accepts `EPOSE_MINE_TIMEOUT` for slow EPoSE registration smoke runs, and accepts `EPOSE_OFFLINE=1` for the isolated single-node registration smoke where daemon mining would otherwise wait for network synchronization; set `EPOSE_PUBLISH_P2P=1` when host-level P2P port inspection is needed. It is the base for the later service-registration, epoch, reward, and competing-chain scenarios.

Fresh HF17 beta candidate validation on seed host A:

```text
image: qwertycoin-v2-node:epose-runtime-tests
commit: 7611f7510
state: /tmp/qwc-epose-beta-3-current-20260829112905
nodes: 3
result: pass
height: 67
top hash: 00f1dc4867d12c3db6af29e01e1734ef3b8d2b7e75a6cf6472bf1a26fe59659d
service nodes: 3
qualified nodes: 1
service reward active: true
epose state hash: 4bbe413556a88c9879d558f017bee74671ea8c0c47c56a586d27f0ab89186ba6
reward RPC: identical on all nodes
```

Fresh HF17 3-node restart/persistence validation on seed host A:

```text
result: pass
containers recreated with persistent volumes kept
height: 67
top hash: 00f1dc4867d12c3db6af29e01e1734ef3b8d2b7e75a6cf6472bf1a26fe59659d
epose state hash: 4bbe413556a88c9879d558f017bee74671ea8c0c47c56a586d27f0ab89186ba6
```

Earlier HF17 beta candidate validation on seed host A:

```text
image: qwertycoin-v2-node:epose-runtime-tests
commit: 5e31382dd runtime image, with later harness-only commits run from checkout
state: /tmp/qwc-epose-beta-3-20260829095428
nodes: 3
result: pass
height: 46
service nodes: 3
qualified nodes: 1
service reward active: true
epose state hash: 69f2ef... identical on all nodes
```

Fresh HF17 3-node restart/persistence validation on seed host A:

```text
result: pass
containers recreated with persistent volumes kept
height: 46
top hash: identical on all nodes
epose state hash: identical on all nodes
```

Fresh HF17 beta candidate validation on seed host A:

```text
image: qwertycoin-v2-node:epose-runtime-tests
commit: 7611f7510
state: /tmp/qwc-epose-beta-5-current-20260829113004
nodes: 5
result: pass
height: 121
top hash: ee367f9524db1f3d36d282d9f4578646648fd6d81861bba9b47c1c2c9a5ae369
service nodes: 5
qualified nodes: 3
service reward active: true
epose state hash: 4e7b7ce3609fcc1e324e49a88a33de9b6a1f21e0ead0acea973bb2eee6cfecd8
reward RPC: identical on all nodes
```

Fresh HF17 5-node restart/persistence validation on seed host A:

```text
result: pass
containers recreated with persistent volumes kept
height: 121
top hash: ee367f9524db1f3d36d282d9f4578646648fd6d81861bba9b47c1c2c9a5ae369
epose state hash: 4e7b7ce3609fcc1e324e49a88a33de9b6a1f21e0ead0acea973bb2eee6cfecd8
```

Fresh HF17 5-node mined partition/heal/reorg validation on seed host A:

```text
state: /tmp/qwc-epose-beta-5-current-20260829113004
nodes: 5
result: pass
initial mined network height: 121
final height after partition/heal/reorg: 245
final top hash: 9db3c361564cc7bda404f147f9de1f6661b32784f097253633e588c43a9c983a
final epose state hash: fa2da16e11ff9d1dbac125c41947d52c758a5ad9c84e234418d2b269b5c1a9de
service reward active: true
qualified nodes: 3
reward RPC: identical on all nodes
```

Wallet and non-mining operator registration smoke on seed host A:

```text
image: qwertycoin-v2-node:v2.0.0-beta.1
commit: 6d2177a66
network: qwc-opflow-50
miner container: qwc-opflow-miner-50
service-node container: qwc-opflow-service-50
wallet daemon address: qwc-opflow-service-50:8197
result: pass
height: 260
top hash: a18b6e791d3e3f01445335e616ef1f50c40e390e88eb1911ac9d3a27d099832c
service nodes: 1
qualified nodes: 0
epose state hash: c2e70148d74d9c44bf999060f7b1ad6ae79dbdca83d75ca0dfe375be138316de
tx pool: 0
registration tx: 2910fb666e33...
```

The operator smoke generated a fresh testnet wallet, mined spendable testnet
coins into it, connected the wallet CLI to the service-node daemon over the
same Docker bridge network, ran `register_service_node`, relayed the resulting
normal wallet transaction, mined it, and verified that the daemon reported the
service node in canonical EPoSE state. `qualified nodes: 0` is expected for this
single-service-node smoke because the beta qualification rule requires
attestations from other service nodes. The same run caught and fixed the
wallet/daemon genesis mismatch by making the default genesis generator use
QWC HF17 instead of the inherited v1 defaults.

Earlier HF17 beta candidate validation on seed host A:

```text
image: qwertycoin-v2-node:epose-runtime-tests
state: /tmp/qwc-epose-beta-5-20260829101655
nodes: 5
result: pass
height: 166
top hash: 77efa8... identical on all nodes
service nodes: 5
qualified nodes: 3
service reward active: true
epose state hash: 062939... identical on all nodes
reward RPC: identical on all nodes
```

Fresh HF17 5-node restart/persistence validation on seed host A:

```text
result: pass
containers recreated with persistent volumes kept
height: 166
top hash: 77efa8... identical on all nodes
epose state hash: 062939... identical on all nodes
```

Fresh HF17 5-node mined partition/heal/reorg validation on seed host A:

```text
state: /tmp/qwc-epose-beta-5-reorg-20260829104303
nodes: 5
result: pass
initial mined network height: 115
final height after partition/heal/reorg: 250
final top hash: d6730cffe08cf99a25896cc96a124d8290bf944944c60759fe9e939fa291f4f6
final epose state hash: eba46bef34c2a3babd971b212a4b9a5bc92f48d657db6a8d338313220ef484c7
service reward active: true
qualified nodes: 3
reward RPC: identical on all nodes
```

Validation on seed host A:

```text
EPOSE_NODE_COUNT=5
EPOSE_RPC_BASE_PORT=40197
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: all five service-node daemons started, `/get_epose_info` became ready, `partition` and `heal` completed against running container IDs, and `assert-same-tip` reported height `1`, top hash `ff006d25bc907b14a0895495ed915f49a2292816dc0490f9c99c1a5a81495e53`, and identical EPoSE state hashes on every node.

Validation on seed host A:

```text
EPOSE_NODE_COUNT=10
EPOSE_RPC_BASE_PORT=42197
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: all ten service-node daemons started, `/get_epose_info` became ready, and `assert-same-tip` reported height `1`, top hash `ff006d25bc907b14a0895495ed915f49a2292816dc0490f9c99c1a5a81495e53`, and identical EPoSE state hashes on every node.

Validation on seed host A:

```text
EPOSE_NODE_COUNT=5
EPOSE_RPC_BASE_PORT=46197
EPOSE_RPC_READY_TIMEOUT=180
EPOSE_START_BATCH_SIZE=2
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: the readiness-gated batch startup path started all five service-node daemons in batches of two, `/get_epose_info` became ready for every node, then `assert-restart-persists` deleted/recreated the containers while preserving volumes. After restart, every node still reported height `1`, top hash `ff006d25bc907b14a0895495ed915f49a2292816dc0490f9c99c1a5a81495e53`, and EPoSE state hash `09eca94b8c3c2b1757e9c6a7143ade035ac98472b6f2d0c1e7cc2b47352f74f5`.

Validation on seed host A:

```text
EPOSE_NODE_COUNT=5
EPOSE_RPC_BASE_PORT=47197
EPOSE_RPC_READY_TIMEOUT=180
EPOSE_START_BATCH_SIZE=2
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: `assert-partition-heal` verified a clean 5-node state before partition, split the running containers into A/B Docker bridge networks, healed every container back to the shared bridge network, waited for `/get_epose_info`, and then verified the same height `1`, top hash `ff006d25bc907b14a0895495ed915f49a2292816dc0490f9c99c1a5a81495e53`, and EPoSE state hash `09eca94b8c3c2b1757e9c6a7143ade035ac98472b6f2d0c1e7cc2b47352f74f5` on all five nodes.

Mined registration smoke on seed host A:

```text
EPOSE_NODE_COUNT=1
EPOSE_RPC_BASE_PORT=19297
EPOSE_OFFLINE=1
EPOSE_MINE_TIMEOUT=900
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: after rebuilding the current `e7cf279a0` development image with `NPROC=2`, `assert-mined-registration` started an isolated service-node daemon, started mining through RPC, mined through the then-temporary EPoSE activation height, and verified `height=798`, `epose_enabled=true`, `service_node_count=1`, and `local_registered=true`.

Mined registration restart persistence smoke on seed host A:

```text
EPOSE_NODE_COUNT=1
EPOSE_RPC_BASE_PORT=19497
EPOSE_OFFLINE=1
EPOSE_MINE_TIMEOUT=900
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: after fixing the harness volume mount to the daemon's actual `/root/.qwertycoin` datadir, `assert-mined-registration-restart-persists` mined a local registration, stopped and recreated the container with the same Docker volume, and verified the restored node at height `737`, top hash `0e55eb6554accb3ea235d9fdf2fe641112235e54da4d6003e7c029eb02e32a0c`, and EPoSE state hash `01cc165ad1ab9f3962ffcb1432be22a4e9fc43a76ba3e29d234d57d660a8be05`.

25-node capacity check on seed host A:

```text
EPOSE_NODE_COUNT=25
EPOSE_RPC_BASE_PORT=43197
EPOSE_RPC_READY_TIMEOUT=600
EPOSE_COMPOSE_PARALLEL_LIMIT=2
EPOSE_START_BATCH_SIZE=5
EPOSE_IMAGE=qwertycoin-v2-node:epose-dev-tests
```

Result: not passed on seed host A. The host has 8 GiB RAM and no swap; each running testnet daemon used about 530 MiB, so the 25-node run exceeded available memory and Docker reported daemon exits with code `137`. The harness-side batch startup works, but a real 25-daemon run needs a larger host or a lighter daemon profile before it can be counted as a completed 25-node validation.
