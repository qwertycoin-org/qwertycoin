# EPoSE receipt-relay hardening at height 20,160

## Activation

- **Network block version:** remains 17
- **Local-policy activation height:** 20,160
- **First completely covered EPoSE epoch:** 28, heights 20,160–20,879
- **Qualification finalization:** after block 20,819
- **First subsequent reward epoch:** 29, beginning at block 20,880

Height 20,160 is not a hard fork. Updated and older nodes validate the same
HF17 blocks before, at and after the boundary. The change is limited to local
receipt production, relay queues, peer delivery and mining-template policy.

## Recommended operator update

Update every component that creates or supplies block templates before height
20,160, in this order:

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

## Epoch-28 boundaries

| Event | Inclusive height range / height |
| --- | --- |
| Round 0 carriers | 20,160–20,359 |
| Round 1 anchor | 20,360 |
| Round 1 carriers | 20,361–20,559 |
| Round 2 anchor | 20,560 |
| Round 2 carriers | 20,561–20,819 |
| Qualification finalization | after block 20,819 |
| Reward epoch 29 begins | 20,880 |

Individual payout timing within epoch 29 remains subject to the existing
reward scheduler and wallet unlock rules.

## Release gate

Do not publish or deploy a candidate until the final immutable commit passes
all required Linux, Windows and ASan checks plus the focused multi-node,
mixed-version, reorg, restart and activation-boundary profiles. If operational
lead time is no longer sufficient, choose a later fixed epoch boundary and
rebuild/retest a new candidate; never mutate an already published binary under
the same version. For activation at 20,160, publish the verified release no
later than canonical height 18,000 to preserve the planned 2,160-block lead.
If that gate is missed, compute a later planning candidate as
`720 * ceil((release_height + 2160) / 720)` and publish one fixed height in the
replacement build.

The design decision and regression requirements are recorded in
[ADR-0010](../epose/review/ADR-0010-HEIGHT-GATED-RELAY-SLOT-HARDENING.md).
