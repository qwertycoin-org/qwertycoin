# Qwertycoin v2 Community Setup Guide

This guide covers the public Qwertycoin v2 mainnet test: install a wallet,
connect a node, mine, and run the current EPoSE service producer.

## Verify Downloads First

Use only the published repositories:

- Core CLI: <https://github.com/qwertycoin-org/qwertycoin/releases>
- GUI wallet: <https://github.com/qwertycoin-org/qwertycoin-gui/releases>

Download the archive for your operating system and verify it against the
`SHA256SUMS` file in the same release before extracting it. The current Linux
and Windows Core/GUI packages are unsigned. The current macOS Core and GUI
packages are ad-hoc signed but are not Developer ID signed or notarized. A
SHA-256 match verifies file integrity, not publisher identity.

Qwertycoin v2 uses a new genesis block, network ID, address prefix, data
directory, and consensus rules. Do not reuse an old-chain daemon database or
wallet cache.

## What You Need

For a normal wallet:

- the Qwertycoin GUI or Core CLI package for your operating system,
- a synced local daemon or an intended public node,
- a wallet with its recovery seed stored offline.

For an EPoSE service node:

- an always-on Linux host with a stable public IP address or lowercase DNS name,
- the Core CLI package or a locally built image from the same source revision,
- enough storage for an unpruned chain,
- public TCP ports `8196` and `8198`,
- a dedicated primary QWC reward address,
- protected storage for the EPoSE keystore.

An EPoSE node does not need a wallet private view key, wallet spend key, or a
funded registration transaction. Never place wallet secrets in node config.

## Ports

| Purpose | Port | Exposure |
| --- | ---: | --- |
| P2P node traffic | `8196` | Public |
| unrestricted daemon RPC | `8197` | Local/private only |
| restricted EPoSE probe RPC | `8198` | Public for service nodes |
| ZMQ | `8199` | Local/private only |

## Install A Desktop Wallet

### Windows x86_64

1. Download and verify the Windows GUI release ZIP.
2. Extract it to a normal user-writable folder.
3. Start `qwertycoin-gui.exe`.
4. Create a new wallet or restore one from seed.
5. Record the recovery seed offline before receiving funds.

Windows may warn because the current public-test package is unsigned. Do not
bypass a warning until the archive checksum matches the official release.

### macOS Apple Silicon

1. Download and verify the macOS ARM64 GUI release archive.
2. Extract it and open `qwertycoin-gui`.
3. Create or restore a wallet and record the seed offline.

The current app is ad-hoc signed and not notarized. After verifying its
checksum, open it through Finder with right click and `Open` if Gatekeeper
requests confirmation.

### Linux x86_64

1. Download and verify the Linux GUI release archive.
2. Extract it and start the bundled launcher or `qwertycoin-gui` binary.
3. Create or restore a wallet and record the seed offline.

## Run A Normal Core Node

From the extracted Core package:

```bash
./qwertycoind \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 8197
```

Keep unrestricted RPC on loopback. Check synchronization locally:

```bash
curl -s http://127.0.0.1:8197/get_info
```

Open the CLI wallet against it:

```bash
./qwertycoin-wallet-cli --daemon-address 127.0.0.1:8197
```

## Start Mining

The GUI can start mining when it is connected to a daemon. In the CLI wallet:

```text
start_mining 4
```

Replace `4` with a suitable thread count. Mining remains RandomX proof of work;
EPoSE does not replace block production or chain selection. Coinbase outputs
must mature before they can be spent.

## Run An EPoSE Service Node

### 1. Prepare The Host

Allow inbound P2P and restricted probe traffic:

```bash
sudo ufw allow 8196/tcp
sudo ufw allow 8198/tcp
```

Create private storage for the service identity:

```bash
sudo install -d -m 700 /var/lib/qwertycoin-epose
```

Use an equivalent firewall and private directory on systems without `ufw`.

### 2. Choose A Reward Address

Create a dedicated normal QWC wallet and copy only its primary public address.
Back up the wallet seed offline. Do not copy its private view or spend keys to
the service host.

### 3. Start The Producer

Replace the reward address and public host below. The host must resolve to this
machine and the public restricted endpoint must be reachable on port `8198`.

```bash
./qwertycoind \
  --epose-service \
  --epose-keystore /var/lib/qwertycoin-epose/keystore \
  --epose-reward-address QWC_REWARD_ADDRESS_HERE \
  --epose-host node.example.org \
  --epose-port 8198 \
  --epose-discovery-endpoint http://seed-00.qwertycoin.org:8198 \
  --epose-discovery-endpoint http://seed-01.qwertycoin.org:8198 \
  --epose-discovery-endpoint http://seed-02.qwertycoin.org:8198 \
  --epose-discovery-endpoint http://seed-03.qwertycoin.org:8198 \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 8197 \
  --rpc-restricted-bind-ip 0.0.0.0 \
  --rpc-restricted-bind-port 8198 \
  --confirm-external-bind
```

The discovery endpoints bootstrap signed service descriptors. They are not an
operator allowlist: an external participant uses the same consensus admission,
epoch, committee, and qualification rules as the project-operated nodes.

The daemon creates or loads its bound keystore. Once fully synchronized, it
automatically builds and relays its lifecycle and RandomX admission records for
the next eligible epoch. Do not run `register_service_node`; that command and
the old `--service-node` flags are retired v1 compatibility surfaces.

### 4. Inspect The State

```bash
curl -s http://127.0.0.1:8197/get_info
curl -s http://127.0.0.1:8197/get_epose_info
curl -s http://127.0.0.1:8197/get_epose_service_endpoint_v2
```

Expected progression:

```text
producer ready -> registered -> active -> qualified
```

These states do not occur immediately. The producer waits for synchronization,
an eligible enrollment window, the RandomX admission proof, the two-epoch
warm-up, and sufficient canonical service receipts. Qualification and rewards
are never guaranteed. Epoch zero has no EPoSE rewards.

## Troubleshooting

### Legacy `--service-node` is rejected

This is intentional. Use `--epose-service` and the EPoSE options shown above.
The retired v1 path requested a wallet private view key and a funded
registration transaction; those are not inputs to EPoSE.

### `local_service_node_key_loaded` is false

Check that the process can securely create/read the keystore path, the reward
address belongs to mainnet, the public host is canonical, and the endpoint port
is nonzero. A keystore copied from another genesis or parameter set is rejected.

### The producer is ready but not registered

Confirm the daemon is fully synchronized and has peers. Enrollment targets a
future eligible epoch and may wait for the current cutoff. Check logs for the
bounded admission search and envelope relay; do not bypass the epoch rules.

### Registered but not qualified

Confirm both public ports are reachable, the restricted RPC answers
`get_epose_service_endpoint_v2`, and other service nodes are online. Normal
committee and receipt thresholds still apply.

### Wallet cannot connect

Do not expose unrestricted RPC. Use a local daemon or an SSH tunnel to
`127.0.0.1:8197` on a host you control.

## Safe Operating Notes

- Back up wallet seeds and the EPoSE keystore separately.
- Keep wallet secrets off the service host.
- Keep unrestricted RPC and ZMQ private.
- Keep the unpruned chain and stable keystore across normal restarts.
- Verify release checksums before running downloaded programs.
- Report reproducible public-test issues through the relevant GitHub repository.
