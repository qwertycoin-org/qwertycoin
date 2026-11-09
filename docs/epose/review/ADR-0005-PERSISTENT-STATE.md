# ADR-0005: EPoSE v2 Persistent Index Boundary

- **Status:** Boundary semantic images integrated with LMDB and validated; pruned-node support pending
- **Date:** 2026-10-31
- **Scope:** CO-08, non-activating

## Decision

EPoSE v2 persistence is an optimization and recovery index, never an alternate
consensus source. Canonical blocks remain the replay oracle.

`state_index_v2.*` defines schema version 1 and a canonical checksummed image of
recent checkpoints. Each checkpoint binds height, block hash, parent hash,
EPoSE state root, and the hash of canonical EPoSE payload bytes. The index:

- accepts only contiguous parent-linked checkpoints;
- treats an exact tip replay as idempotent and rejects a same-height conflict;
- retains a bounded undo horizon;
- returns `rebuild_required` instead of inventing state beyond that horizon;
- validates schema, parameter-set identity, checksum, lengths, and chain links
  before atomically replacing loaded state;
- rebuilds from canonical checkpoint history and produces the same root after
  A → B → A fork replacement as fresh replay.

[ADR-0011](ADR-0011-ROLLING-EPOCH-WINDOW.md) defines the eight-epoch semantic
window. The boundary image uses the index's schema and checksum framing and
contains canonical lifecycle and membership state. LMDB retains images inside
the three-epoch disconnect horizon and the newest earlier canonical image.

## LMDB integration

The `epose_snapshots` table stores images at epoch boundaries. A block
connection writes its image in the same LMDB transaction as the block and its
state commitment. A block disconnection removes an image for the popped height
in the same transaction. State images contain the semantic state; the canonical
block table remains the source for replay and validation.

A stored image/root is never trusted on its own. On startup and deep reorg,
schema, parameter set, height, canonical block hash, checksum, canonical image,
and recomputed state root must agree with the stored block commitment. Missing
or corrupt data triggers explicit rebuild from blocks. Replay after a loaded
image also falls back to a full rebuild if any block transition disagrees.

## Crash and deep-reorg rule

Because an LMDB transaction commits both chain and EPoSE index or neither, a
restart observes the old state or the complete new state. Reorgs within the
undo horizon disconnect incrementally. A deeper reorg discards/rebuilds the
derived index from sufficient canonical history.

## Pruning

Pruned nodes may discard ordinary historical payload bytes only if every datum
needed to validate current/future EPoSE state is retained in a consensus-bound,
rebuildable form. Until that proof exists, QWC-HF17 EPoSE v2 on pruned nodes remains
unsupported and activation is blocked.
