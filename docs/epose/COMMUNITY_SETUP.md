# Qwertycoin v2 Community Setup Guide

This guide explains how to install Qwertycoin v2, start a wallet, mine blocks,
and register an EPoSE service node.

EPoSE service nodes are intended to run on always-on Linux servers. Desktop
wallets on Windows, macOS, and Linux can mine, receive funds, and submit the
registration transaction once the wallet has spendable QWC.

## What You Need

For a normal wallet:

- the Qwertycoin GUI or CLI package for your operating system,
- a synced daemon connection,
- a wallet with your recovery seed stored safely.

For an EPoSE service node:

- a Linux server with a stable public IP address or DNS name,
- Docker or native Qwertycoin binaries,
- TCP port `8196` reachable from the internet,
- a normal QWC reward wallet address,
- the matching private view key for that reward address,
- a funded wallet that can pay the service-node registration transaction.

Never share your wallet private spend key. The service-node reward private view
key is intentionally disclosed in the registration so the network can validate
service rewards. It can see incoming reward activity, but it cannot spend coins.

## Ports

Default ports:

| Purpose | Port | Public? |
| --- | ---: | --- |
| P2P node traffic | `8196` | Yes |
| unrestricted daemon RPC | `8197` | No, keep local/private |
| restricted daemon RPC | `8198` | Optional |
| ZMQ | `8199` | No, keep local/private |

Only expose P2P port `8196` unless you know why you need public RPC. For a
public service node, firewall rules should allow inbound TCP `8196`.

## Install On Windows

1. Download the Windows release ZIP.
2. Extract the ZIP into a folder such as `C:\Qwertycoin`.
3. Start `qwertycoin-gui.exe`.
4. Create a new wallet or restore an existing wallet from seed.
5. Let the wallet connect to a synced daemon.
6. Save the 25-word recovery seed offline before receiving funds.

If Windows Defender or SmartScreen warns about the binary, confirm that the file
name and checksum match the official release. Early community builds may be
unsigned.

## Install On macOS Apple Silicon

1. Download the macOS ARM64 release archive.
2. Extract the archive.
3. Start the `qwertycoin-gui` binary.
4. Create a new wallet or restore an existing wallet from seed.
5. Let the wallet connect to a synced daemon.
6. Save the 25-word recovery seed offline before receiving funds.

Early release-candidate builds may be unsigned and not notarized. If macOS
blocks the app, open it once through Finder with right click and `Open`, or
remove the quarantine attribute after verifying the checksum:

```bash
xattr -dr com.apple.quarantine /path/to/qwertycoin-gui
```

## Install On Linux Desktop

1. Download the Linux x86_64 release archive.
2. Extract the archive.
3. Make the binaries executable if needed:

```bash
chmod +x qwertycoin-gui qwertycoind qwertycoin-wallet-cli
```

4. Start the GUI:

```bash
./qwertycoin-gui
```

5. Create or restore a wallet.
6. Let the wallet connect to a synced daemon.
7. Save the 25-word recovery seed offline before receiving funds.

## Start Mining From A Desktop Wallet

Mining can be started from the GUI when the wallet is connected to a daemon.
For CLI users, open the wallet and use:

```text
start_mining <threads>
```

Example:

```text
start_mining 4
```

Mining rewards are locked for `60` blocks. You can see mined coins before they
are spendable, but you cannot use them for service-node registration until they
are unlocked.

## Run A Linux EPoSE Service Node With Docker

The recommended community setup is a Linux server with Docker.

### 1. Prepare The Server

Install Docker and allow inbound P2P traffic:

```bash
sudo apt update
sudo apt install -y ca-certificates curl docker.io
sudo systemctl enable --now docker
sudo ufw allow 8196/tcp
```

If your server uses a different firewall tool, open TCP `8196` there instead.

### 2. Create A Reward Wallet

Create a normal QWC wallet with the GUI or CLI. Use a dedicated reward wallet if
possible.

You need:

- the primary QWC address, starting with `QWC...`,
- the private view key for that address.

In `qwertycoin-wallet-cli`, the private view key can be shown with:

```text
viewkey
```

Do not put the private spend key into any service-node configuration.

### 3. Start The Service-Node Daemon

Start `qwertycoind` in service-node mode:

```bash
docker run -d \
  --name qwertycoin-mainnet \
  --restart unless-stopped \
  -p 8196:8196 \
  -p 127.0.0.1:8197:8197 \
  -p 127.0.0.1:8198:8198 \
  -p 127.0.0.1:8199:8199 \
  -v qwertycoin-mainnet-chain:/home/qwertycoin/.qwertycoin \
  -v qwertycoin-service-node:/service-node \
  -v qwertycoin-wallets:/wallet \
  qwertycoin-v2-node:latest \
  --service-node \
  --service-node-key /service-node/service-node.key \
  --service-reward-address QWC_REWARD_ADDRESS_HERE \
  --service-reward-view-key PRIVATE_VIEW_KEY_HERE \
  --service-node-advertise-address your-node.example.org:8196 \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 0.0.0.0 \
  --rpc-bind-port 8197 \
  --restricted-rpc-bind-ip 0.0.0.0 \
  --restricted-rpc-bind-port 8198 \
  --zmq-rpc-bind-ip 0.0.0.0 \
  --zmq-rpc-bind-port 8199
```

Replace:

- `QWC_REWARD_ADDRESS_HERE` with your reward wallet primary address,
- `PRIVATE_VIEW_KEY_HERE` with the matching private view key,
- `your-node.example.org:8196` with your public DNS name or IP address.

The `qwertycoin-service-node` volume stores the service-node identity key. Keep
this volume when resetting the blockchain database. Delete it only if you want a
new service-node identity.

The `qwertycoin-wallets` volume is optional, but useful if you want to run
`qwertycoin-wallet-cli` inside the same container for a controlled test setup.

### 4. Check That The Daemon Is Ready

On the server, run:

```bash
curl -s http://127.0.0.1:8197/get_info
curl -s http://127.0.0.1:8197/get_epose_info
curl -s http://127.0.0.1:8197/get_service_node_registration_payload
```

The node must be synced and the registration payload must be available. If the
payload endpoint returns an error, fix the daemon configuration first. Common
causes are an invalid reward address, a wrong private view key, or a missing
advertise address.

## Register The Service Node On-Chain

Starting a service-node daemon is not enough. The node becomes registered only
after a wallet submits a real registration transaction and that transaction is
mined into a block.

### Option A: Register From A Wallet On The Server

Use this only for test deployments or if you are comfortable keeping the
operator wallet on the server.

Open a funded wallet against the local daemon:

```bash
docker exec -it qwertycoin-mainnet qwertycoin-wallet-cli \
  --daemon-address 127.0.0.1:8197 \
  --wallet-file /wallet/operator-wallet
```

Inside the wallet:

```text
refresh
register_service_node
```

Confirm the transaction when prompted. The wallet creates a normal transaction,
attaches the daemon's signed EPoSE registration payload, and relays it.

### Option B: Register From Your Desktop Wallet Through SSH

This keeps your wallet spend key on your desktop machine.

Create an SSH tunnel from your desktop to the service-node server:

```bash
ssh -L 18197:127.0.0.1:8197 user@your-node.example.org
```

In another terminal, open your local funded wallet against the tunnel:

```bash
qwertycoin-wallet-cli \
  --daemon-address 127.0.0.1:18197 \
  --wallet-file /path/to/your-wallet
```

Inside the wallet:

```text
refresh
register_service_node
```

Repeat this once for each service node, always tunneling to the daemon of the
node you want to register.

### Optional Funding Address

By default, `register_service_node` sends one atomic unit back to the wallet's
primary address and attaches the service-node registration payload.

To send funds to another address while registering:

```text
register_service_node <funding_address> <amount>
```

Most operators should use the default command first.

## Confirm Registration

After the transaction is mined, check:

```bash
curl -s http://127.0.0.1:8197/get_epose_info
```

Expected progression:

```text
not registered -> registered -> active -> qualified
```

`registered` means the on-chain registration was accepted.

`active` means the registration is valid for the current EPoSE epoch.

`qualified` means enough valid attestations exist for the node to participate in
service rewards.

With too few registered service nodes, qualification may be delayed or fragile.
For a small controlled test network, three service nodes can work. For healthier
operation, run four or more. The target committee design is more comfortable
with ten or more registered service nodes.

## Register Multiple Service Nodes

For three service nodes, repeat the same pattern three times:

1. Start service-node daemon `A`.
2. Register daemon `A` through a wallet connected to daemon `A`.
3. Start service-node daemon `B`.
4. Register daemon `B` through a wallet connected to daemon `B`.
5. Start service-node daemon `C`.
6. Register daemon `C` through a wallet connected to daemon `C`.

The same funded operator wallet can submit all three registration transactions,
but each registration must be created while connected to the daemon being
registered. That is how the wallet gets the correct local signed payload.

## Troubleshooting

### `No registered service nodes yet.`

This is normal on a fresh chain before registration transactions are mined.
Start the service-node daemon, submit `register_service_node` from a funded
wallet, mine the transaction, and check again.

### `Not enough unlocked money`

Your wallet does not have spendable funds yet. Newly mined coins need `60`
blocks before they can be spent.

### Registration payload is not ready

Check that the daemon was started with:

- `--service-node`,
- `--service-node-key`,
- `--service-reward-address`,
- `--service-reward-view-key`,
- `--service-node-advertise-address`.

Also confirm that the reward address is a primary QWC address and that the
private view key belongs to that address.

### Node is registered but not qualified

The node has an accepted registration, but it still needs valid attestations
from other service nodes. Confirm that multiple service nodes are registered,
online, reachable on P2P port `8196`, and synced to the same chain tip.

### The desktop wallet cannot connect to the service-node daemon

Do not expose unrestricted RPC publicly just to fix this. Use an SSH tunnel to
reach `127.0.0.1:8197` on the server, or run the wallet on the server only for a
controlled test deployment.

## Safe Operating Notes

- Back up wallet seeds before mining or registering.
- Back up the service-node identity volume or key file.
- Keep RPC ports private unless there is a clear operational reason.
- Do not reuse public documentation examples as real keys or addresses.
- Keep enough QWC in the operator wallet to pay transaction fees.
- After any chain reset, service-node registrations must be submitted again.
