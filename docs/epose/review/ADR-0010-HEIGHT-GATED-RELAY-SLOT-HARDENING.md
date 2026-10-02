# ADR-0010: height-gated receipt-relay slot hardening

- **Status:** Proposed for activation; implementation in review
- **Date:** 2026-10-01
- **Activation:** local policy at height 21,600; block version remains 17
- **Scope:** EPoSE-v2 local relay queue and mining-template selection

## Observed failure

Finalized mainnet epoch 17 had 18 active service nodes and zero qualified
nodes. Probes and signatures largely succeeded, but only a small fraction of
the locally submitted service receipts reached canonical blocks. Long-lived
daemon queues were dominated by repeated randomized signatures for receipt
slots that had already been submitted.

The canonical membership pipeline identifies a receipt slot by epoch, round,
service kind, subject and verifier. The local relay queue instead identified a
record by the hash of the complete signed envelope. Re-signing the same logical
slot therefore consumed another bounded queue/template entry. Round-zero
variants accumulated faster than canonical inclusion and starved later rounds.

## Decision

1. Keep QWC block version 17 and every consensus rule unchanged.
2. Activate only local relay/template policy at block height 21,600.
3. Before height 21,600, preserve complete-record relay identity.
4. At and after height 21,600, admit at most one usable pending receipt for each
   `(epoch, round, service kind, subject, verifier)` slot.
5. If pre-activation variants remain in memory at the boundary,
   deterministically keep the variant with the lowest complete-record relay ID
   and erase the rest.
6. When a receipt becomes canonical, erase every queued variant of its slot.
7. Preserve exact-record behavior for lifecycle and admission records.
8. Report an expired record or exhausted queue as submission failure instead
   of returning success with no accepted envelope.
9. A transport retry reuses the same authenticated envelope, is bounded by
   stable peer identity and time, and remains relayable through an
   intermediary without increasing queue occupancy.
10. Receipt expiry uses the same inclusive per-round carrier windows as
    canonical validation. Expired or wrong-context entries are removed before
    allocating the template budget.
11. A reorganization invalidates stale snapshot/anchor context so a valid
    replacement for the same logical slot can enter the queue.
12. Authentic late variants of a canonical slot are expected races, not peer
    faults; malformed signatures still fail closed.

## Consensus and compatibility

Receipt-slot deduplication is local relay/template policy. Complete blocks are
still parsed and validated against canonical EPoSE state. Mainnet, testnet and
stagenet continue scheduling only block version 17, so updated and older nodes
accept the same blocks across height 21,600. An older node may keep the faulty
queue policy until upgraded, but it cannot create a version-driven chain split.

There is no state migration. Relay policy is derived from the active chain
height, so disconnect/reconnect across the boundary deterministically switches
the local cache behavior. Height 21,600 is the first block of epoch 30, making
that entire service epoch measurable under one relay policy.

## Security properties

- Randomized signature variants cannot multiply queue occupancy for one slot.
- A maximum-size 18-node, three-round, nine-verifier workload is bounded by
  486 pending evidence slots rather than unbounded retries.
- The relay cache remains non-authoritative; only canonical blocks determine
  qualification and rewards.
- The quorum is not weakened to conceal transport loss.
- Mixed old/new deployments retain identical block validity and chain
  selection across the boundary.

## Required evidence

- exact activation tests at heights 21,599, 21,600 and 21,601;
- unchanged HF17 block acceptance before, at and after height 21,600;
- mixed old/new relay-policy templates for unique canonical slots;
- reorg below and recross above the boundary;
- restart/resync reconstruction from canonical height;
- semantic-slot duplicate and canonical-purge tests;
- an 18-node/two-epoch relay, template, canonical inclusion and qualification
  regression with real keys and signatures; and
- full daemon and EPoSE unit builds on a clean runner.
