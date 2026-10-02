# EPoSE receipt-relay hardening at height 21,600

## Release candidate

- **Core version:** 2.0.3
- **Internal compatible version:** 2.0.3.0
- **Candidate tag:** v2.0.3-rc1
- **Previous compatible release:** v2.0.2

Version 2.0.3 changes local receipt production and relay policy only. It does
not change block acceptance, the block version, EPoSE state calculation or
reward calculation relative to 2.0.2. Published v2.0.2 metadata and historical
compatibility fixtures remain immutable.

## Activation

- **Network block version:** remains 17
- **Local-policy activation height:** 21,600
- **First completely covered EPoSE epoch:** 30, heights 21,600–22,319
- **Qualification finalization:** after block 22,259
- **First subsequent reward epoch:** 31, beginning at block 22,320

Height 21,600 is not a hard fork. Updated and older nodes validate the same
HF17 blocks before, at and after the boundary. The change is limited to local
receipt production, relay queues, peer delivery and mining-template policy.

## Recommended operator update

Update every component that creates or supplies block templates before height
21,600, in this order:

1. mining daemons and pool block-template daemons;
2. the relay paths and seed daemons used by those template providers;
3. public RPC daemons; and
4. EPoSE service nodes.

An older node does not split from the chain, but it retains relay behavior that
can delay or starve valid receipts. Updating only service nodes does not prove
that miners receive the evidence. Record the exact build SHA for each known
template provider and keep unknown mining coverage labelled unknown.

## What changes

- each semantic receipt slot and canonical snapshot/anchor context occupies at
  most one pending queue entry;
- expired round evidence and stale reorg context are removed before the
  template budget is allocated;
- a valid replacement may enter after its old canonical anchor is reorganized;
- transport retries reuse the identical authenticated envelope and are bounded
  per stable peer identity and time window;
- an intermediary can re-forward an exact pending envelope without increasing
  queue occupancy;
- authentic late variants of already canonical slots are treated as expected
  races, while forged signatures still fail closed; and
- authenticated operator diagnostics distinguish local production, queueing,
  redelivery, template selection, canonical inclusion and qualification.

## What does not change

- block version 17 and every block-validity rule;
- RandomX block production and chain selection;
- EPoSE protocol/wire version 2;
- the EPoSE parameter-set hash and state commitment;
- the 6-of-9 receipt threshold and two-of-three-round qualification rule;
- registration, reward or payout economics; and
- wallet keys or wallet-file format.

There is no retrospective qualification and no recognition of receipts after
their canonical round window. A node still needs valid registration,
reachability and sufficient authentic receipts from its assigned verifiers.

## Epoch-30 boundaries

| Event | Inclusive height range / height |
| --- | --- |
| Round 0 carriers | 21,600–21,799 |
| Round 1 anchor | 21,800 |
| Round 1 carriers | 21,801–21,999 |
| Round 2 anchor | 22,000 |
| Round 2 carriers | 22,001–22,259 |
| Qualification finalization | after block 22,259 |
| Reward epoch 31 begins | 22,320 |

Individual payout timing within epoch 31 remains subject to the existing
reward scheduler and wallet unlock rules.

## Release gate

Do not publish or deploy a candidate until the final immutable commit passes
all required Linux, Windows and ASan checks plus the focused multi-node,
mixed-version, reorg, restart and activation-boundary profiles. If operational
lead time is no longer sufficient, choose a later fixed epoch boundary and
rebuild/retest a new candidate; never mutate an already published binary under
the same version. For activation at 21,600, publish the verified release no
later than canonical height 19,440 to preserve the planned 2,160-block lead.
If that gate is missed, compute a later planning candidate as
`720 * ceil((release_height + 2160) / 720)` and publish one fixed height in the
replacement build.

## Rollback

Before rollout, retain the verified v2.0.2 package, its checksum, the active
data-directory path and a cold backup of the LMDB and EPoSE state. To roll
back, stop 2.0.3 cleanly, preserve its logs and data directory, start the
verified 2.0.2 binaries against the same canonical database, and verify the
reported tip hash and EPoSE state before restoring pool traffic. Do not delete
or resynchronize the database merely to hide a mismatch.

If 2.0.2 rejects the 2.0.3-produced database, reports a different canonical
tip/state/qualification/payment result, or requires a destructive migration,
stop the rollout: that is a release-blocking consensus-compatibility failure.
Roll back template providers and their relay peers first, then public RPC and
service nodes. Never roll back only half of a pool's template/relay path without
confirming receipt delivery remains available.

The design decision and regression requirements are recorded in
[ADR-0010](../epose/review/ADR-0010-HEIGHT-GATED-RELAY-SLOT-HARDENING.md).
