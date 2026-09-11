# Current State Audit

Date: 2026-09-04
Repository: `<repository-url>`
Source: current `main` at `60ccc9d08`

## Baseline

Qwertycoin v2 is derived from Monero v0.18.5.1 and starts a new QWC chain with
the latest inherited Monero consensus rule set active from genesis plus QWC
EPoSE HF17.

| Item | Current value |
| --- | --- |
| Public daemon | `qwertycoind` |
| Wallet CLI | `qwertycoin-wallet-cli` |
| Wallet RPC | `qwertycoin-wallet-rpc` |
| PoW | RandomX |
| Target block time | 120 seconds |
| Display decimals | 8 |
| Atomic units per QWC | `100000000` |
| Money supply | `18446744073709551` atomic / `184,467,440.73709551 QWC` |
| Final subsidy | `30000000` atomic per minute / `0.3 QWC/min` |
| Mainnet P2P | 8196 |
| Daemon RPC | 8197 |
| Restricted/wallet RPC convention | 8198 |
| ZMQ RPC | 8199 |
| Mainnet standard address prefix | `0x14820c` |

## Hardforks

QWC does not replay Monero's historical hardfork activation schedule. Mainnet,
testnet, and stagenet start at `HF_VERSION_QWC_EPOSE = 17` from height 0.
Older Monero hardfork constants remain in the code because validation and wallet
logic use them as inherited feature gates. EPoSE format/protocol version 2 is a
separate version domain and is the only QWC-HF17 EPoSE launch format.

## EPoSE Current State

Current `main` includes:

- bounded tx-extra registration and attestation payloads,
- RandomX-bound admission proof,
- deterministic verifier committee target `5`,
- fixed qualification threshold `EPOSE_MIN_ATTESTATIONS = 2`,
- signed attestations with deterministic challenge/response binding,
- `NOTIFY_NEW_EPOSE_PAYLOADS` P2P relay for registrations and attestations,
- bounded in-memory relay/template pool,
- on-chain-only consensus state,
- finalized reward-source epoch,
- deterministic payee rotation,
- 10% service reward / 90% miner reward,
- governance-style service rewards as normal one-time outputs,
- denomination decomposition for service reward spendability,
- duplicate guards for overlapping service keys and endpoint commitments,
- service-node identity storage outside chain volumes in mainnet deployment.

The fixed threshold, committee size, and admission target are current bootstrap
parameters, not final long-term mainnet hardening.

## Deployment

`deploy/mainnet` now uses:

- one shared `docker-compose.yml`,
- per-host `seed-00.env`, `seed-01.env`, `seed-02.env`,
- `/service-node/service-node.key` for durable service identity,
- separate Docker volumes for chain data and service identity,
- reward address plus matching private view key per service node,
- two priority nodes per seed.

The Docker-only start helper remains supported for hosts without the Compose
plugin.

## Validation Status

Known current validation:

- PR #8 EPoSE unit tests passed.
- PR #8 EPoSE fuzz corpus passed.
- Three-seed controlled reset reached common Genesis.
- Mining produced denominated EPoSE reward outputs.
- Web wallet detected EPoSE rewards through normal wallet scanning.
- Explorer can group denominated outputs as one logical service reward for UI.

Open release gates:

- confirmed matured reward spend and recipient receipt after denomination fix,
- dynamic quorum hardening,
- admission difficulty benchmark/hardening,
- larger committee simulation,
- deeper multi-node, partition, reorg, sanitizer, and long-fuzz coverage.
