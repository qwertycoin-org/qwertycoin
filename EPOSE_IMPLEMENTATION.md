# EPoSe Implementation Strategy

This document translates the original Qwertycoin EPoSe whitepaper idea into an
implementation plan for the Qwertycoin v2 PoC.

EPoSe must not be implemented as a large consensus rewrite in the first step.
The safe path is to build measurable service-node infrastructure first, then add
deterministic reward rules, and only later consider quorum-based consensus
features.

## Whitepaper Summary

The original EPoSe concept combines EPoW and EPoS ideas and rewards multiple
network service groups instead of miners only.

The relevant service groups are:

- miners
- mining pool operators
- public node operators
- developers
- community managers

For node operators, the whitepaper describes a system based on service duration,
software conformance, performance, reliability, and remote daemon usage. It also
mentions four components:

- nodes
- Sentinel
- Node Map
- Network Explorer

Sentinel periodically evaluates node conformance, hardware/network performance,
24-hour availability, and uptime. The explorer exposes the resulting data so
wallets and users can choose reliable remote daemons. Node rewards are intended
to come from accumulated transaction fees over a reward period, avoiding
additional inflation.

## Engineering Interpretation

EPoSe should be treated as a service-node reward and monitoring protocol first,
not as immediate Proof-of-Stake.

The original text does not define enough deterministic consensus rules to safely
implement it directly in a Monero-derived core. In particular, the following
questions are underspecified:

- How nodes register.
- How Sybil attacks are prevented.
- Who is allowed to measure service quality.
- How off-chain measurements become deterministic on-chain facts.
- How miners agree on the exact service-node payout set.
- How to handle dishonest Sentinel reports.
- How to handle privacy for public node operators.

These gaps must be closed before any mainnet consensus change.

## Recommended Architecture

### Phase 0: QWC-v2 PoW Base

Keep the base chain simple:

- Monero-derived codebase
- RandomX Proof of Work
- Dockerized node
- no EPoSe consensus enforcement
- token-holder migration handled separately

Goal: prove that QWC-v2 can build, run, mine, and transfer.

### Phase 1: EPoSe Lite, Off-Chain

Build Sentinel, Node Map, and Explorer outside consensus.

Service node operators register their node metadata:

- node public key
- QWC-v2 payout address
- P2P endpoint
- optional RPC endpoint
- software version
- operator-signed registration message

Sentinel measures:

- node reachability
- chain height lag
- protocol version
- uptime over rolling windows
- latency
- RPC availability, preferably restricted RPC only
- peer count
- whether the node serves expected endpoints

The Explorer displays:

- online/offline state
- current score
- uptime history
- software version
- geographic approximation
- reward eligibility preview

Rewards in this phase are not consensus-critical. If rewards are tested, they
come from a manually funded testnet faucet or migration-test wallet.

### Phase 2: Deterministic Eligibility

Move from "observed score" to "deterministic reward eligibility".

Required rule examples:

- Eligibility epochs are block-based, not wall-clock based.
- One reward epoch is `720` blocks at a 120-second block target, roughly one day.
- A node must be registered before the epoch starts.
- A node must meet minimum uptime and height-sync thresholds.
- Sentinel observations must be signed and timestamped.
- Observations are accepted only if they are anchored on-chain before the epoch
  closes.

At this phase, the chain still does not trust a single Sentinel. The design must
support multiple independent Sentinels.

### Phase 3: Consensus Reward Split

Only after Phase 2 works on testnet, add consensus validation for service-node
rewards.

Possible reward model:

- Miner receives block subsidy.
- Transaction fees are reserved for the epoch service-node reward pool.
- At epoch boundary, eligible service nodes receive deterministic payouts.
- Payout order and amounts are derived from on-chain registration and anchored
  observations.

The validation rule must allow every node to independently compute:

- eligible service-node set
- score/rank
- total reward pool
- exact payout amounts
- exact payout addresses

If two honest nodes cannot compute the same result from chain data alone, the
rule is not safe for consensus.

### Phase 4: Quorum Features

Only after service-node rewards are stable, consider quorum-based protection.

Possible features:

- signed service-node checkpoints
- chain-split alerts
- optional fast-confirmation hints
- miner block template advisory signals

Do not let service-node quorums override PoW in the first mainnet version.
RandomX PoW should remain the canonical consensus source until the quorum logic
has been heavily tested.

## Anti-Sybil Requirement

The original whitepaper says QWC masternodes should be free. That is attractive
for community participation, but it creates a Sybil problem when rewards are
attached.

For a production reward system, QWC-v2 needs at least one anti-Sybil mechanism:

- a small refundable registration bond
- proof-of-work identity registration
- reputation based on long-lived keys and long uptime
- capped reward weight per independently verified network location
- governance-approved Sentinel sets

The least bad technical option is a modest refundable bond plus long-lived node
identity. Without any cost to register, rewards are easy to farm.

## Monero Core Touchpoints

Likely core areas for later phases:

- `src/cryptonote_config.h`
  - epoch length
  - service reward constants
- `src/cryptonote_core/blockchain.cpp`
  - block validation
  - miner transaction validation
  - epoch payout validation
- `src/cryptonote_basic/`
  - transaction extra fields
  - service-node registration payloads
- `src/rpc/`
  - node status endpoints
  - service-node registration endpoints
- `src/p2p/`
  - optional signed service heartbeats
- wallet code
  - registration helper
  - payout address management

The Sentinel and Explorer should live outside the core tree at first, preferably
as separate services in Docker Compose.

## Practical MVP

The first implementable EPoSe MVP should be:

1. Keep QWC-v2 as RandomX PoW.
2. Add `epose-sentinel` as an external service.
3. Add a service-node registry file or API.
4. Measure node uptime, sync height, latency, and version.
5. Store signed observations.
6. Render a simple explorer table.
7. Generate a deterministic reward report for one epoch.
8. Manually test payouts on devnet.

Only then should QWC-v2 add consensus-level reward validation.

## Recommendation

Implement EPoSe in layers:

- PoW chain first
- off-chain Sentinel second
- deterministic reward reports third
- testnet fee/reward distribution fourth
- consensus enforcement last

This preserves the original QWC idea while avoiding the biggest risk: putting an
underspecified, centralized, off-chain measurement system directly into
consensus.
