# Qwertycoin v2 PoC

This repository is an experimental Qwertycoin v2 proof of concept based on
Monero `v0.18.5.1`.

It is not a mainnet release and must not be presented as a public QWC relaunch.
The first target is a single Dockerized development node that proves the
modified codebase can build and run independently.

## Current PoC Scope

- Base code: Monero `v0.18.5.1`
- Proof of work: RandomX, inherited from Monero
- Coin name: Qwertycoin
- Ticker: QWC
- Runtime data directory: `.qwertycoin`
- Mainnet-like PoC ports:
  - P2P: `29980`
  - RPC: `29981`
  - ZMQ RPC: `29982`
- Address prefixes reuse the legacy QWC prefix family as a PoC placeholder.
- The network UUIDs are QWC-specific and do not match Monero.

## Deliberate Non-Goals

- No public mainnet.
- No final genesis transaction.
- No final emission schedule.
- No token-holder claim implementation.
- No final EPoSE mainnet admission difficulty.
- No final service reward privacy implementation.

The public daemon and wallet binaries are now Qwertycoin-branded:
`qwertycoind`, `qwertycoin-wallet-cli`, and `qwertycoin-wallet-rpc`.

## Docker Build

```bash
docker build --build-arg NPROC=2 -t qwertycoin-v2-node .
```

For larger machines, omit `NPROC` or increase it.

The initial OpenClaw workspace used to create this PoC did not have Docker,
CMake, or a C++ compiler installed, so the first real container build still has
to be run on a Docker-capable server.

## Run A Single PoC Node

```bash
docker run --rm -it \
  -p 29980:29980 \
  -p 29981:29981 \
  -v qwertycoin-v2-chain:/home/qwertycoin/.qwertycoin \
  qwertycoin-v2-node
```

This starts an isolated QWC-v2 PoC node with QWC-specific P2P/RPC ports and
network IDs. It intentionally has no production seed nodes and starts with DNS
checkpoints disabled.

## Next Technical Steps

1. Generate a QWC-specific genesis transaction and nonce.
2. Define the token snapshot block for the ERC-20 QWC contract.
3. Decide the migration ratio, preferably `1 token = 1 QWC-v2`.
4. Define the migration reserve mechanics.
5. Build a claim prototype:
   - Ethereum address signs a claim message.
   - Claim service verifies balance at the snapshot block.
   - Claim service sends QWC-v2 to the provided native address.
6. Prototype EPoSe as an off-chain Sentinel/Explorer layer before adding any
   consensus rules. See `EPOSE_IMPLEMENTATION.md`.
7. Continue classifying and cleaning remaining public branding strings while
   preserving upstream attribution and inherited compatibility identifiers.

## License

The upstream Monero license and copyright notices must be retained. QWC-specific
copyright notices should only be added for new QWC code.
