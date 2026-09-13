# Qwertycoin v2

Qwertycoin v2 is the new QWC core implementation. It keeps QWC as a
RandomX proof-of-work coin and adds EPoSe, a deterministic service-node reward
layer that can be verified by normal consensus nodes.

This repository is based on the Monero 0.18.x codebase and retains upstream
Monero, CryptoNote, RandomX, and third-party license notices where they still
apply.

## Network Reset And Legacy Compatibility

Qwertycoin v2 is a new network. It is not a continuation of the old Qwertycoin
chain database, wallet cache, peer state, or node state. It is a clean network
reset intended to give QWC its most stable technical baseline so far.

Old blockchain files, old daemon data directories, old wallet cache files, and
old P2P state files are not compatible with the v2 network. Do not point a v2
daemon or wallet at old production data directories.

If you still have an old mnemonic seed or private spend/view keys, you may be
able to import the key material into a v2 wallet for address/key recovery
experiments. That does not import old-chain balances and does not provide
automatic access to coins from the legacy network, because Qwertycoin v2 has a
new genesis block, network ID, address prefix, data directory, and consensus
rules.

Users who participated in the historical QWC swap should use the official
project support channel for swap-related questions.

## Coin Specs

| Property | Value |
| --- | --- |
| Ticker | QWC |
| Consensus | RandomX proof of work with EPoSe service-node rewards |
| Block target | 120 seconds |
| Decimals | 8 |
| Supply constant | 184,467,440.73709551 QWC |
| Tail emission | 0.3 QWC per minute, equal to 0.6 QWC at the 120-second block target |
| Current EPoSe service reward | 10% of actual issued subsidy; transaction fees remain with the miner |
| Address scheme | Qwertycoin v2 address prefixes; not legacy-chain compatible |
| P2P port | 8196 |
| Daemon RPC port | 8197 |
| Wallet RPC port | 8198 |
| ZMQ RPC port | 8199 |

The current EPoSE service reward value is `1000` basis points of actual issued
subsidy. Transaction fees are not shared with the service-node output. When no
qualified service node exists, the service portion falls back to the miner.

## Downloads

The current published Core candidate is
[`v2.0.0-rc1`](https://github.com/qwertycoin-org/qwertycoin/releases/tag/v2.0.0-rc1)
for Linux x86_64, macOS Apple Silicon, and Windows x86_64. It is a prerelease
for testing, not a stable activation release. Stable publication remains
blocked while the candidate-bound EPoSE release gate reports NO-GO.
Native candidates and releases are produced only by the manual, candidate-bound
process in [docs/releases/RELEASE_PROCESS.md](docs/releases/RELEASE_PROCESS.md).
If no matching release is visible there, build from source and treat other
binaries as unverified development artifacts.

## Build From Source

Clone the repository and initialize submodules:

```bash
git clone --recursive https://github.com/qwertycoin-org/qwertycoin
cd qwertycoin
git submodule update --init --recursive
```

Build outputs are written below `build/<platform>/<branch>/release/bin` when
the multi-build-directory layout is active.

### Linux

Install build dependencies on Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config git ccache python3 \
  libboost-all-dev libssl-dev libzmq3-dev libpgm-dev libunbound-dev \
  libsodium-dev libhidapi-dev libusb-1.0-0-dev libunwind-dev \
  liblzma-dev libreadline-dev libexpat1-dev
```

Build dynamically linked release binaries:

```bash
make release
```

Build statically linked Linux x86_64 binaries where static system libraries are
available:

```bash
make release-static-linux-x86_64
```

If your distribution does not ship the required static libraries for HIDAPI,
libusb, libevent, or unbound, use the normal `make release` build or build the
missing static dependencies first.

### Windows

Use MSYS2 with the MinGW-w64 toolchain. Install the usual development packages
from the MSYS2 MinGW shell, including CMake, Make, Git, Boost, OpenSSL, ZeroMQ,
libsodium, unbound, HIDAPI, and libusb.

For 64-bit Windows:

```bash
make release-static-win64
```

For 32-bit Windows:

```bash
make release-static-win32
```

The Windows targets use the repository toolchain files in `cmake/` and expect
`MINGW_PREFIX` to be set by the matching MSYS2 shell.

### macOS

Install Xcode Command Line Tools and Homebrew dependencies:

```bash
xcode-select --install
brew update
brew install cmake boost hidapi openssl zmq libpgm unbound \
  libunwind-headers protobuf ccache
```

For Intel macOS builds:

```bash
make release
```

For Apple Silicon CPUs such as M1, M2, and M3, prefer a native ARM64 CMake
build instead of x86_64 emulation:

```bash
cmake -S . -B build/macos-arm64-release \
  -DARCH=armv8-a \
  -DBUILD_64=ON \
  -DBUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DMANUAL_SUBMODULES=1 \
  -DUSE_DEVICE_TREZOR=OFF

cmake --build build/macos-arm64-release \
  --parallel "$(sysctl -n hw.logicalcpu)"
```

Docker `linux/amd64` emulation on Apple Silicon is useful for smoke tests, but
it is slow and does not represent native RandomX performance.

## Run A Node

Start a mainnet-mode daemon:

```bash
./build/release/bin/qwertycoind \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 8197 \
  --zmq-rpc-bind-port 8199
```

Keep RPC bound to localhost unless you intentionally protect and expose it
behind a secure reverse proxy or firewall policy.

Start a wallet against a local daemon:

```bash
./build/release/bin/qwertycoin-wallet-cli \
  --daemon-address 127.0.0.1:8197
```

The primary command-line binaries are:

```text
qwertycoind
qwertycoin-wallet-cli
qwertycoin-wallet-rpc
```

## Docker

Build a local development image:

```bash
docker build --build-arg NPROC=2 -t qwertycoin-node:local .
```

Run a local daemon:

```bash
docker run --rm -it \
  -v qwertycoin-chain:/home/qwertycoin/.qwertycoin \
  -p 8196:8196 \
  -p 127.0.0.1:8197:8197 \
  -p 127.0.0.1:8199:8199 \
  qwertycoin-node:local \
  --p2p-bind-ip=0.0.0.0 \
  --rpc-bind-ip=0.0.0.0 \
  --confirm-external-bind \
  --non-interactive
```

## EPoSe Overview

EPoSe adds a service-node registry and deterministic reward selection on top of
proof of work. It is designed so that every honest node can reconstruct the
same EPoSE state from canonical chain data and reject blocks with missing,
duplicate, wrong-recipient, or wrong-amount service rewards.

The current `main` implementation includes:

- service-node key generation and loading,
- signed service-node registration payloads with reward address and disclosed
  reward private view key,
- epoch-based registration lifetime,
- RandomX-bound admission proof,
- P2P relay for signed EPoSe registrations and attestations,
- deterministic verifier committee selection,
- signed attestations,
- qualification snapshots,
- deterministic service reward rotation,
- governance-style service rewards as normal denominated CryptoNote one-time
  outputs,
- separate service-node identity storage for Docker deployments,
- `get_epose_info`, `get_epose_epoch`, `get_service_rewards`, and
  `get_service_node_registration_payload` daemon RPCs,
- a wallet CLI `register_service_node` helper.

Current EPoSE parameters:

| Parameter | Value |
| --- | --- |
| Epoch length | 720 blocks |
| Admission lease | 1 epoch |
| Verifier committee size | 9 |
| Committee threshold | 6 |
| Required attestation rounds | 2 of 3 |
| Service reward | 1000 bps |
| Reward fee policy | Subsidy only |
| Admission leading-zero bits | 18 |

`1000 bps` means a 10% service-node share of actual issued subsidy and a 90%
miner subsidy share. Transaction fees remain with the miner. The machine-
readable parameter manifest and release-gate ledger remain authoritative; the
current ledger is a release **NO-GO** until every mandatory gate is satisfied
by evidence bound to the selected candidate revision.

Service reward outputs are no longer direct long-term spend-public-key outputs.
The selected service node registers a normal QWC reward address plus the
matching private view key. Blocks derive standard one-time output keys from the
coinbase transaction public key, and validation recomputes the expected keys
from the disclosed view key. The 10% service reward is decomposed into normal
amount denominations so wallet coin selection can find decoys after coinbase
maturity.

## Register A Service Node

Start a fully synced daemon in service-node mode:

```bash
qwertycoind \
  --service-node \
  --service-node-key /secure/path/service-node.key \
  --service-reward-address <mainnet QWC primary address> \
  --service-reward-view-key <matching private view key> \
  --service-node-advertise-address <public-host>:8196 \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 8197
```

The reward address must be a primary address for the selected network. The
reward private view key must derive to that address' public view key. It is
disclosed for consensus validation and reveals incoming activity for that
reward wallet, but it cannot spend funds. The private spend key must stay
secret and must not be placed in service-node env files.

Inspect local EPoSE status:

```bash
curl -s http://127.0.0.1:8197/get_epose_info
curl -s http://127.0.0.1:8197/get_service_node_registration_payload
```

Open a funded wallet connected to that daemon:

```bash
qwertycoin-wallet-cli \
  --daemon-address 127.0.0.1:8197 \
  --wallet-file /path/to/operator-wallet
```

Inside the wallet, submit the registration transaction:

```text
refresh
register_service_node
```

Without arguments, `register_service_node` creates a transaction that sends one
atomic unit back to the wallet's primary address and attaches the daemon's
signed EPoSE registration payload. To fund another address while registering:

```text
register_service_node <funding_address> <amount>
```

The wallet must be able to sign and relay transactions. Watch-only and multisig
wallets are rejected by the current registration wrapper. After registration, monitor
`get_epose_info` until the local service public key is registered, active, and
qualified. Confirm `get_service_rewards` against multiple peers before treating
the local reward view as healthy.

## Useful Links

| Resource | URL |
| --- | --- |
| Website | https://qwertycoin.org/ |
| Core repository | https://github.com/qwertycoin-org/qwertycoin |
| Bitcointalk ANN | https://bitcointalk.org/index.php?topic=2881418.0 |
| CoinGecko | https://www.coingecko.com/en/coins/qwertycoin |
| Discord | https://qwertycoin.org/discord |
| X / Twitter | https://x.com/Qwertycoin_QWC |

Old release, donation, web-wallet, pool, and node-map links were intentionally
removed from this README because they are either not part of the current
release process or were not reachable during this refresh.

## Tests

Run a regular release test build:

```bash
make release-test
```

Build focused EPoSE tests through the development Dockerfile:

```bash
docker build \
  --build-arg NPROC=2 \
  --build-arg EPOSE_BUILD_TARGETS="epose_unit_tests epose_fuzz_tests" \
  -f Dockerfile.epose-dev \
  -t qwertycoin-node:epose-tests .
```

Run the focused unit tests:

```bash
docker run --rm --entrypoint /usr/local/bin/qwertycoin-epose-unit-tests \
  qwertycoin-node:epose-tests
```

Run the EPoSE fuzz seed corpus:

```bash
docker run --rm --entrypoint /usr/local/bin/qwertycoin-epose-fuzz-tests \
  qwertycoin-node:epose-tests
```

## Documentation

Further EPoSE notes are in:

```text
docs/epose/README.md
docs/epose/PROTOCOL.md
docs/epose/THREAT_MODEL.md
docs/epose/CONSENSUS_INVARIANTS.md
docs/epose/REWARDS.md
docs/epose/SERVICE_NODE.md
docs/epose/COMMUNITY_SETUP.md
docs/epose/MACOS_M1_TESTING.md
docs/epose/SERVICE_REWARD_PRIVACY.md
docs/epose/MAINNET_TESTPHASE.md
docs/epose/IMPLEMENTATION_REPORT.md
```

## License

See [LICENSE](LICENSE) for licensing terms.

Copyright (c) 2026 The Qwertycoin Project. Portions copyright (c) 2014-2022 The
Monero Project and copyright (c) 2012-2013 The CryptoNote developers.
