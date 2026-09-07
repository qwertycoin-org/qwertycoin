# ADR-0005: EPoSE v2 Persistent Index Boundary

- **Status:** Indexed-journal primitive implemented; LMDB integration required
- **Date:** 2026-09-06
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

## Required LMDB integration

The primitive is deliberately not attached to the current HF17 database. The
activation implementation must add versioned LMDB tables for checkpoint/meta,
canonical lifecycle/evidence records, and undo data. Updates must occur in the
same LMDB write transaction as the corresponding block connect/disconnect.

A stored image/root is never trusted on its own. On startup, schema, parameter
set, last-applied height/hash, and canonical blockchain tip must agree. Missing
or corrupt data triggers explicit rebuild from blocks. It must never produce an
empty qualified set as a silent fallback.

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
