# EPoSE v2 Implementation Report

Date: 2026-09-04
Source: current `main` at `60ccc9d08`

## Executive Summary

Qwertycoin v2 is a Monero v0.18.5.1-derived chain with QWC monetary,
networking, branding, and EPoSE changes. Current `main` contains the EPoSE
registration, admission, attestation relay, qualification, reward selection, and
governance-style reward output path from PR #8.

The implementation is suitable for controlled mainnet validation, not yet final
public mainnet release. The largest remaining protocol-hardening items are:

- dynamic 2/3 qualification quorum instead of fixed `2`,
- larger verifier committee evaluation,
- benchmarked RandomX admission difficulty,
- final fee/subsidy tokenomics decision,
- longer fuzz/sanitizer and multi-node adversarial validation.

## Current Core Baseline

| Item | Current value |
| --- | --- |
| Upstream base | Monero v0.18.5.1-derived |
| QWC protocol | HF17 from genesis |
| PoW | RandomX |
| Display decimals | 8 |
| Supply | `184,467,440.73709551 QWC` |
| Final subsidy | `0.3 QWC/min` |
| Block target | 120 seconds |
| Mainnet P2P | 8196 |
| Daemon RPC | 8197 |
| Restricted/wallet RPC convention | 8198 |
| ZMQ RPC | 8199 |
| Mainnet standard address prefix | `0x14820c` |

## EPoSE Architecture

EPoSE is derived from canonical chain data:

```text
service-node registration
=> RandomX-bound admission proof
=> deterministic verifier committee
=> signed attestation
=> on-chain inclusion by any miner
=> qualified set
=> finalized reward-source epoch
=> deterministic payee rotation
=> service reward validation
```

Service nodes are not miners. A miner does not need a service-node key, and a
service node does not need local mining hashpower. Signed EPoSE payloads can be
relayed through normal nodes and included by normal miner daemons.

## Admission

Each service-node registration carries a RandomX-bound admission proof. The hash
is bound to:

- QWC network ID,
- service public key,
- reward address,
- reward private view key,
- endpoint commitment,
- registration epoch,
- previous finalized epoch hash,
- nonce.

Current target: `EPOSE_ADMISSION_LEADING_ZERO_BITS = 8`.

That value is a bootstrap/validation value. It must be increased only after
realistic solver/verification benchmarks and Sybil simulations.

## Attestation Relay

`NOTIFY_NEW_EPOSE_PAYLOADS` carries serialized registration and attestation
blobs. Nodes validate and deduplicate payloads before storing them in a bounded
in-memory pool:

- max pool entries: `4096`,
- max relay batch: `32`.

The pool is template/relay policy only. It is not consensus state. Consensus
state changes only when payloads are included on-chain and accepted by the
EPoSE chain-state application path.

## Qualification

Current qualification rule:

```text
unique_valid_attestations(subject, epoch) >= EPOSE_MIN_ATTESTATIONS
```

Current `EPOSE_MIN_ATTESTATIONS = 2`.

The deterministic verifier committee target is `5`. The subject is excluded
from its own verifier committee. Duplicate verifier votes do not add weight.

This fixed threshold is the next major hardening target. The intended direction
is a dynamic 2/3 rule based on actual committee size, but that is not implemented
in current `main`.

## Rewards

Current reward split:

```text
total_reward = base_reward + transaction_fees
service_reward = floor(total_reward * 1000 / 10000)
miner_reward = total_reward - service_reward
```

This pays 10% to the selected service node and 90% to the miner in controlled
validation. Whether final public mainnet should split transaction fees or only
split subsidy remains an explicit tokenomics decision.

Rewards are selected from a finalized reward-source epoch. Payloads in the
current epoch cannot change the current block's payee.

## Wallet Compatibility

The old transparent service reward output design has been removed from current
`main`. Service rewards now use the historical governance principle:

- selected service node registers a normal QWC reward address,
- registration discloses the matching private view key,
- spend key remains secret,
- coinbase derives normal one-time output keys,
- validators recompute expected one-time keys from the coinbase tx public key
  and disclosed view key.

The service reward amount is decomposed into standard digit denominations. This
is required for spendability because large unique non-RCT amount classes do not
provide enough decoys for wallet coin selection.

Live controlled testing showed wallet reward detection in the web wallet. The
final release gate still requires a confirmed spend from a matured reward wallet
to a recipient wallet after the denominated reward fix.

## Service Node Identity

Service identity is the `service_public_key`, not an IP address and not the
reward address. The service private key signs registrations and attestations but
cannot spend wallet funds.

Docker mainnet deployment stores the service-node key outside the chain volume:

```text
/service-node/service-node.key
```

The shared compose file mounts a separate identity volume. Chain resets should
delete chain data only; preserving the identity volume preserves the service
public key.

## Registration Lifecycle

Current implemented lifecycle is:

```text
REGISTER -> ACTIVE -> EXPIRE
```

The registry and relay pool reject overlapping active registrations for:

- the same service public key,
- the same endpoint commitment.

Endpoint uniqueness is an operational duplicate guard, not a complete Sybil
resistance model. Explicit renewal, update, and deregistration semantics remain
open.

## Reorg / Persistence

EPoSE state is currently reconstructed from canonical chain data. Block
application uses snapshots so invalid EPoSE payload batches can roll back
without leaving partial state. Older rollback paths can rebuild from the
canonical chain.

Dedicated LMDB-derived EPoSE indexes are not implemented yet.

## RPC / CLI

Implemented daemon/operator surfaces include:

- `--service-node`,
- `--service-node-key`,
- `--service-reward-address`,
- `--service-reward-view-key`,
- `--service-node-advertise-address`,
- `get_epose_info`,
- `get_service_nodes`,
- `get_service_node_status`,
- `get_epose_epoch`,
- `get_service_rewards`,
- `get_service_node_registration_payload`,
- daemon console helpers `epose_status` and `prepare_service_node_registration`,
- wallet CLI helper `register_service_node`.

## Deployment

`deploy/mainnet` now uses one shared `docker-compose.yml` and host-specific
env files:

- `seed-00.env`,
- `seed-01.env`,
- `seed-02.env`.

The env files carry the image tag, data volume, identity volume, service-key
path, reward address, reward private view key, advertised address, and priority
peers. The Docker-only helper remains supported for hosts without the Compose
plugin.

## Tests / Validation

Current known validation from PR #8 and controlled deployment:

- EPoSE unit tests passed after governance-style reward implementation.
- EPoSE fuzz corpus passed.
- Denominated service reward outputs were added after live wallet coin
  selection exposed that one large amount output was not practically spendable.
- A fresh three-seed controlled chain reset reached common genesis state.
- Mining produced denominated EPoSE reward outputs.
- Explorer grouped denominated outputs as one logical service reward for
  presentation while leaving raw outputs visible.
- Web wallet detected service rewards.

Remaining release gates:

- matured denominated reward spend from reward wallet,
- recipient wallet receipt,
- longer multi-node adversarial runs,
- larger network and partition/reorg validation,
- clean broader ASan/UBSan coverage,
- admission and quorum hardening.

## Known Limitations

- Fixed `EPOSE_MIN_ATTESTATIONS = 2` is too weak as a long-term mainnet rule.
- Committee target `5` needs simulation against `7/9/11`.
- Admission target `8` leading zero bits is a bootstrap value.
- Subject-authenticated live service proof is not complete.
- Dedicated EPoSE LMDB indexes are not implemented.
- Renewal/update/deregister semantics are not implemented.
- Stronger service-reward privacy remains future protocol research.
- Final public mainnet fee/subsidy split is not decided.

## Open Hardening Items

The next PR should implement and validate:

- dynamic 2/3 quorum,
- selected committee target,
- benchmarked admission difficulty,
- small-network liveness behavior,
- offline-verifier liveness,
- Sybil/collusion simulations,
- upgrade/reset impact documentation.
