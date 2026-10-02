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

## Candidate verification record

The release gate is evaluated against the immutable PR head, not merely a
version string. The final PR description must name that SHA and link its CI
checks. The following checks are required again if the head changes:

- the complete EPoSE unit suite, including the height-21,600 boundary, late
  receipt authentication, production retry limits, qualification close at
  22,259, reward-source transition at 22,320 and the reorg profiles;
- daemon, wallet CLI and wallet RPC builds and version output;
- release-request validation and release-tool tests; and
- Linux, Windows and ASan CI jobs.

On 2026-10-02 the signed `v2.0.2` Linux daemon and the locally built `2.0.3`
candidate were also exercised against the same receipt-bearing mainnet LMDB.
The EPoSE processing path was active (`protocol_version=2`, epoch 19,
2,337 attestations). Both binaries reported the same height 14,020, tip
`ed7ebf58e5435e57f5370116c3f61d941539eb01f90e2a6be272426873c897ff`,
EPoSE state hash
`9a2d02edfcc7c2ecf5dcf6779d7ba14f5b5fbd07f833a16157b25ea648a20e12`,
service-node count 16, qualified count 0 and reward view for height 14,019.
The candidate database opened unchanged under `v2.0.2`; the candidate then
rewound three blocks, restarted, replayed the canonical chain and restored
the same height-14,019 block hash and EPoSE state.

This runtime result proves current-chain forward/backward database operation,
restart and bounded canonical replay. Mainnet was below future heights
21,599/21,600/21,601, 22,259 and 22,320 during the check, so those exact live
blocks cannot be observed before the chain reaches them. Their deterministic
production-parameter coverage is a release gate; live mixed-version
observation at those heights is a rollout and post-activation evidence item.
Any observed block-acceptance, EPoSE-state, qualification or payment
divergence between 2.0.2 and 2.0.3 is an immediate release/deployment blocker.
No such divergence was observed in the current-chain binary run.

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
