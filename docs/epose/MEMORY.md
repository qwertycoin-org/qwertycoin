# EPoSE-v2 memory ownership, undo and retention

## Scope and reference

Audited reference: `1e479cc9feda48e07dd07ba6abcb89ca3934b5e2`
(`qwertycoin-org/qwertycoin`, v2.0.4 baseline). The schema-1 journal is a
local representation optimization. The contracts fork adds the rolling-window
schema-2 commitment described below. RandomX still determines block
production and chain selection. Reward rules and record authentication remain
unchanged.

Existing solutions preflight: reuse the existing append-only lifecycle/receipt
containers, deterministic boundary transitions and bounded undo horizon, with
ordinary write-ahead undo. No external cache, database or new hashing scheme is
needed to eliminate full undo snapshots. A generic cache cannot safely evict
data committed by this protocol.

## Observed ownership and root cause

`block_transition_v2::apply_block` previously copied `state_` for each block and
retained another full pre-block semantic state in every undo entry. The compiled
production path in `coordinator_v2.cpp` sets `max_recent_undo_blocks = 2160`.
Those entries owned independent vectors and map values, not shared storage.
Bounding their count did **not** bound their payload as the historical state grew.

`semantic_state_v2::apply_transaction` additionally copied both the lifecycle
registry and membership pipeline for every transaction, including empty record
lists. While applying a transaction at a full horizon, the canonical state,
2160 retained snapshots, block candidate, and transaction candidate could all
coexist: approximately 2163 state-sized payloads (with different historical
sizes). At block commit the new snapshot temporarily made 2161 retained entries.
This is owned historical growth multiplied by snapshots, not an unreachable
allocation leak.

Growing containers:

- `lifecycle_registry_v2::histories_`: every identity, including expired and
  deregistered identities; each identity owns all its lifecycle records.
- Expiry-only folds release an identity's keys for re-registration; explicit
  deregistration permanently retires its operator and service keys.
- Schema-2 lifecycle records have a maximum 30-epoch lease from their effective
  epoch; schema-1 records retain their original lease rule.
- A fold batches retired-key additions, sorts and deduplicates them once, and
  compacts retired-key storage only when the set grows.
- `membership_pipeline_v2::leases_`: one stored lease per identity/target epoch;
  higher sequence replaces an existing slot, new epochs append slots.
- `receipts_`: accepted receipt-slot hashes/metadata across all epochs (not full
  signed wire receipts).
- `snapshots_`: frozen membership values, each with its member vector.
- `qualifications_`: closed qualification values and qualified-key vectors.

Other paths reviewed:

- `state_hash()`: lifecycle sorts pointers; membership previously sorted full
  lease/receipt copies. Schema 1 sorts pointers with the identical comparator
  and serializes identical fields. Schema 2 uses cached closed-epoch digests
  and recomputes the open bucket's transcript.
- `relay_pool_v2.cpp`: relay admission checkpoints the canonical semantic
  state under the blockchain lock and rolls accepted envelope previews back
  in reverse order. Slot variants roll back their own preview. Receipt pruning
  rolls each probe back. Late and same-slot receipt classification uses a
  receipt-length journal and does not copy the membership pipeline.
- `Blockchain::select_epose_template_records_v2`: a checkpoint on the canonical
  state previews selections and rolls them back before returning. In-order
  dependencies and exclusion of failed records are preserved by transactional
  application.
- RPC getters in `blockchain.cpp` copy requested descriptors, memberships or
  qualified lists, not the coordinator or undo history. `get_service_nodes`
  materializes active descriptors before limiting its response;
  `get_service_rewards` and epoch/round queries can request historical data.
  Explorer is an observer, never the source of consensus state.
- `replay_v2.cpp` applies blocks sequentially and compares every recorded
  commitment. `Blockchain::rebuild_epose_state` keeps the old coordinator alive
  until a fresh coordinator has replayed and verified successfully. Two states
  and two undo windows may coexist; the windows now contain journals. This
  intentional peak is retained for failure recovery, not silently discarded.
- LMDB `add/remove/get_epose_state_commitment_v2` stores schema, block hash,
  state hash and parameter hash in the canonical block write transaction.
  Epoch-boundary semantic images are written in that transaction. Startup and
  deep reorg validate image bindings, then replay canonical blocks and check
  commitments. A failed image falls back to an earlier image or full replay.

### Schema-1 commitment serialization

The hash constructors and their domain separators are retained:

- Lifecycle: network/genesis/parameters, identity count, histories sorted by
  identity ID; each history's record count and lifecycle signing-message hashes
  in their original sequence order. The signatures themselves are not part of
  the lifecycle state hash.
- Membership: network/genesis/parameters and existing timing/committee fields;
  lease count and leases sorted by target epoch then service key/sequence,
  serializing target, complete frozen member, lease hash and inclusion height;
  snapshot count and snapshot hashes in epoch-map order; receipt count and
  receipts sorted by epoch, round, subject, verifier, serializing the existing
  epoch/round/kind/keys/receipt-hash/inclusion-height fields; qualification count
  and qualification hashes in epoch-map order. Receipt hashes include the
  subject signature even though stored receipts contain only metadata/hashes.
- Semantic: the same semantic domain separator followed by the lifecycle and
  membership hashes. Existing little-endian integers/counts and raw byte fields
  are unchanged. No digest of a pruned set or alternate incremental digest is
  introduced.

## Rolling retention and recovery

[ADR-0011](review/ADR-0011-ROLLING-EPOCH-WINDOW.md) defines the fork-gated
schema-2 memory bound. The open epoch and eight closed epochs remain in the
membership pipeline. At each boundary, older epoch buckets are folded in order
into a committed history root. Closed-bucket transcript digests are cached, so
state-changing blocks recompute the open bucket only. Schema 1 keeps its original
full-history commitment and behavior until the contracts fork.

Lifecycle folding removes expired or deregistered identities before the retained
floor and older records of live identities. Each live identity keeps one
baseline record before the floor and all later effective records. Explicitly
retired identity and service-key hashes remain in a sorted committed vector;
expiry-only keys may be reused. Future schema-2 records are limited to eight
epochs ahead and 30-epoch leases. Future-dated schema-1 records accepted before
the fork remain until their effective epochs pass.

The bounded resident state includes retained buckets, live baselines, the
retired-key vector and removed records owned by fold journals within the
2160-block disconnect horizon. Old member and qualified-key vectors may be
pruned after two epochs; ChainLock reconstructs them from retained archived
records and verifies their commitments. The archive is limited by the
ChainLock lookback and disconnect horizon. Boundary images persist canonical
semantic state in LMDB with the block transaction. Startup and deep reorg
validate image bindings and replay canonical blocks; invalid images trigger
an earlier image or full replay. Historical RPC reads outside the retained
window report `EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW`.

## Journal design and failure behavior

The block journal records lifecycle appends, service-index insertions, folded
and trimmed histories, lease replacements and appends, receipt appends,
snapshot and qualification insertions, folded epoch buckets, and prior window
metadata. Removed histories and buckets remain owned by their fold entries.
Repeated mutations are undone in reverse order. Boundary-only blocks are
journaled too. Retired-key storage includes the canonical vector capacity and
keys held in retained fold journals; 32 bytes per retired key alone is not a
complete bound.

Retained undo is proportional to the undo window plus the changes inside it,
including histories and buckets removed by folds. Schema 1 keeps its full
history; schema 2 keeps the rolling window and permanent retired-key vector.
The journal alone does not impose a hard bound on total RAM.

Each nonempty transaction uses a nested checkpoint. On failure or exception its
scope guard restores it, without appending partial logical entries to the block
journal. On success all destination journal vectors reserve before any entries
are merged; entries have nonthrowing moves. Mutations log
before potentially throwing allocations. A block scope guard restores earlier
successful transactions and boundary transitions on any later failure. The
oldest undo is not evicted until the entire block is accepted. Empty transactions
still execute the original configuration/height checks but allocate no journal.

Rollback truncation releases appended objects and their owned nested vectors;
evicting fold journals releases removed histories and epoch buckets. Live
vectors can retain high-water capacity after truncation, as reported by
`memory_usage()`. No `shrink_to_fit` occurs during a `noexcept`
rollback: it could allocate/throw. Allocators may retain freed pages, so lower
logical ownership is **not** a promise that process RSS immediately falls.
Diagnostic payload accounting includes vector capacity and map values, excludes
map/deque node/allocator overhead, and is not a new RPC or serialized state.

## Verification

`epose_unit_tests` includes new cases for nested new/existing lifecycle histories,
lease replacement, transaction/block exceptions, rejection at a full undo horizon,
failed freeze blocks, empty freeze/close blocks, all 2160 in-window disconnects,
deep replay signaling and a fixed-size undo window across 360 fast epochs.
Existing suites cover authenticated duplicates, receipt races, wire order,
rewards, replay mismatch/missing commitments, persistent state and reorgs.

`epose_memory_probe [epochs]` is an additional deterministic synthetic workload:
four identities, real signed lifecycle/admission/receipt records, low-cost test
RandomX proofs, three rounds per 18-block test epoch, all four qualifying, state
hashes at every block and 36 verified disconnects. It prints every block's hash,
a chain digest of those hashes and per-epoch payload accounting. It does not change
production parameters and is not a prediction of mainnet memory use.
The probe alone links the existing upstream `tests/crypto/random.c` test RNG:
receipt commitments include randomized subject signatures, so deterministic
keys alone are insufficient for reproducible traces. Production RNG/targets are
unchanged; none of these synthetic identities may be used for real funds.

For the independent baseline comparison, build the *identical probe source* on
the pinned reference with `-DEPOSE_MEMORY_BASELINE` (diagnostic calls excluded)
and add only its CMake target. Compare the final trace and common epoch/receipt
counts, and measure each process in isolation using `resource.getrusage` or an
equivalent peak-RSS tool. Keep compiler, build flags, epoch count and host fixed.
Raw logs are local build artifacts, not committed binaries.

For independently built probe binaries on POSIX, the standard-library helper
compares every block's hash, every epoch's receipt count and the full trace, and uses
`wait4` for per-process (not cumulative) peak RSS:

```sh
python3 tests/epose/memory_compare.py \
  --reference /path/to/reference/tests/unit_tests/epose_memory_probe \
  --optimized /path/to/optimized/tests/unit_tests/epose_memory_probe --epochs 32
```

Native macOS/Windows, production replay, and a persistent cold-history cache are
not proven by the Linux-only synthetic probe; those remain distinct gates/work.
