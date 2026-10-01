# HF18: EPoSE receipt-relay hardening

## Activation

- **Network block version:** 18
- **Activation height:** 20,000
- **First complete hardened EPoSE epoch:** height 20,160

Height 20,000 is a mandatory network hard-fork boundary. Nodes that have not
upgraded reject version-18 blocks and remain on an incompatible chain.

## Who must upgrade

Before height 20,000, upgrade every:

- mining daemon and pool block-template daemon;
- public RPC and seed daemon;
- EPoSE service node; and
- wallet distribution that embeds the Core hard-fork schedule.

Operators should verify that the daemon reports the release containing HF18 and
that all mining and service processes are connected to that daemon before the
activation boundary.

## What changes

HF18 hardens the local EPoSE receipt relay queue and mining-template selection:

- retries for one semantic receipt slot can occupy only one pending slot;
- confirmation removes all queued signature variants of that slot;
- template selection operates on unique receipt slots so later rounds cannot be
  starved by randomized re-signatures of earlier-round evidence; and
- an expired record or full queue is reported as submission failure rather than
  as a successful no-op; and
- a full 18-node, three-round, nine-verifier workload is bounded by 486 pending
  receipt slots.

## What does not change

- RandomX block production and chain selection;
- EPoSE protocol/wire version 2;
- the EPoSE parameter-set hash and state commitment;
- the 6-of-9 receipt quorum and two-of-three-round qualification rule;
- registration, reward, or payout economics; and
- wallet keys or wallet-file format.

No state migration occurs at height 20,000. The current EPoSE-v2 state is
continued across the version boundary. Height 20,000 falls inside epoch 27, so
that partial epoch has pre- and post-activation relay behavior; epoch 28 begins
at height 20,160 and is the first epoch entirely covered by the hardened path.

## Reorg behavior

Relay policy follows the active chain height. Disconnecting below height 20,000
restores exact-record queue identity; reconnecting height 20,000 re-applies
semantic receipt-slot compaction deterministically. Canonical block validation
remains authoritative on both sides of the boundary.

The design decision and required regression evidence are recorded in
[ADR-0010](../epose/review/ADR-0010-HF18-RELAY-SLOT-HARDENING.md).
