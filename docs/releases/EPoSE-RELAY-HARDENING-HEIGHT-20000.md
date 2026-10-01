# EPoSE receipt-relay hardening at height 20,000

## Activation

- **Network block version:** remains 17
- **Local-policy activation height:** 20,000
- **First complete hardened EPoSE epoch:** heights 20,160–20,879

Height 20,000 is not a hard fork. Updated and older nodes validate the same
HF17 blocks before, at and after the boundary. The change is deliberately
limited to the non-consensus receipt relay queue and mining-template policy.

## Recommended operator update

Update every component that creates or supplies block templates before height
20,000, especially:

- mining daemons and pool block-template daemons;
- public RPC and seed daemons; and
- EPoSE service nodes.

An older node does not split from the chain, but it retains the queue behavior
that can starve later receipt rounds. Broad deployment is therefore required
for the operational fix to be effective even though it is not required for
block-version compatibility.

## What changes

- retries for one semantic receipt slot occupy one pending slot;
- confirmation removes all queued signature variants of that slot;
- template selection operates on unique receipt slots so later rounds cannot
  be starved by randomized re-signatures of earlier-round evidence;
- an expired record or full queue is reported as submission failure rather
  than as a successful no-op; and
- a full 18-node, three-round, nine-verifier workload is bounded by 486 pending
  receipt slots.

## What does not change

- block version 17 and all block-validity rules;
- RandomX block production and chain selection;
- EPoSE protocol/wire version 2;
- the EPoSE parameter-set hash and state commitment;
- the 6-of-9 receipt quorum and two-of-three-round qualification rule;
- registration, reward or payout economics; and
- wallet keys or wallet-file format.

No state migration occurs at height 20,000. Height 20,000 falls inside epoch
27, so that partial epoch has pre- and post-activation relay behavior. Epoch 28
starts at height 20,160 and is the first epoch entirely covered by the hardened
path.

## Mixed-version and reorg behavior

Updated and older nodes produce and accept the same blocks when presented with
the same unique receipt slots. The updated node additionally compacts redundant
signature variants. Disconnecting below height 20,000 restores exact-record
queue identity; reconnecting at height 20,000 re-applies semantic slot
compaction deterministically. Restarted nodes derive the policy from canonical
height and rebuild their non-persistent queues under the correct rule.

The design decision and regression requirements are recorded in
[ADR-0010](../epose/review/ADR-0010-HEIGHT-GATED-RELAY-SLOT-HARDENING.md).
