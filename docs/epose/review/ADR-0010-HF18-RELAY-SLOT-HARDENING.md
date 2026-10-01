# ADR-0010: HF18 receipt-relay slot hardening

- **Status:** Proposed for activation
- **Date:** 2026-10-01
- **Activation:** QWC block version 18 at height 20,000
- **Scope:** EPoSE-v2 local relay queue and mining-template selection

## Observed failure

Finalized mainnet epoch 17 had 18 active service nodes and zero qualified
nodes. Probes and signatures largely succeeded, but only a small fraction of
the locally submitted service receipts reached canonical blocks. The long-lived
daemon queues were dominated by repeated randomized signatures for receipt
slots that had already been submitted.

The canonical membership pipeline identifies a receipt slot by epoch, round,
service kind, subject and verifier. The local relay queue instead identified a
record by the hash of the complete signed envelope. Re-signing the same logical
slot therefore consumed another bounded queue/template entry. Round-zero
variants accumulated faster than canonical inclusion and starved later rounds.

## Decision

1. Schedule QWC hardfork version 18 at block height 20,000.
2. Keep EPoSE protocol version 2, the parameter-set hash, state commitment,
   membership, 6-of-9 quorum, two-of-three-round rule and reward semantics
   unchanged.
3. Before height 20,000, preserve the existing complete-record relay identity.
4. At and after height 20,000, admit at most one pending receipt for each
   `(epoch, round, service kind, subject, verifier)` slot.
5. If pre-activation variants remain in memory at the boundary, deterministically
   keep the variant with the lowest complete-record relay ID and erase the rest.
6. When a receipt becomes canonical, erase every queued variant of its slot.
7. Preserve exact-record behavior for lifecycle and admission records.
8. Report an expired record or exhausted queue as submission failure instead
   of returning success with no accepted envelope.
9. A local bounded retry may rebroadcast its new signed variant directly.
   Peers that already cache any variant of the slot stop propagation; peers
   that missed the earlier broadcast can still admit it.

## Consensus and compatibility

Receipt-slot deduplication is local relay/template policy; complete blocks are
still parsed and validated against the canonical EPoSE state. The explicit
HF18 schedule nevertheless makes height 20,000 a real network hard fork:
pre-HF18 daemons reject version-18 blocks and remain on an incompatible chain.
Miners, pools, public daemons, service nodes and wallets with an embedded fork
schedule must upgrade before the boundary.

HF18 performs no state migration. ADR-0008 applies: block disconnect/reconnect
continues the same parameter-bound EPoSE-v2 state. Height 20,000 lies inside
epoch 27; the first complete epoch under the new relay policy starts at height
20,160.

## Security properties

- Randomized signature variants cannot multiply queue occupancy for one slot.
- A maximum-size 18-node, three-round, nine-verifier workload is bounded by
  486 pending evidence slots rather than unbounded retries.
- The relay cache remains non-authoritative; only canonical blocks determine
  qualification and rewards.
- The quorum is not weakened to conceal transport loss.

## Required evidence

- exact activation tests at heights 19,999 and 20,000;
- reorg behavior below and above the boundary;
- semantic-slot duplicate and canonical-purge tests;
- an 18-node/three-round repeated-resubmission stress regression;
- full daemon and EPoSE unit builds on a clean runner; and
- a release and operator upgrade notice before height 20,000.
