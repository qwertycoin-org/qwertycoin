# ADR-0011: Rolling EPoSE epoch window

- **Status:** Rolling window implemented; activation scheduling pending
- **Date:** 2026-10-16
- **Scope:** QWC-42, contracts hard fork only

## Decision

Retain eight closed service epochs in addition to the open epoch after the
contracts hard fork. The HF17 state transcript and parameter commitment remain
unchanged. The first contracts block changes the EPoSE state commitment to
schema 2 and folds all epochs older than `open_epoch - 8` in ascending epoch
order. Each later epoch boundary folds the next eligible epoch in that same
order. Undo journals retain removed buckets and the prior commitment,
so disconnect within the undo horizon restores the exact parent state.

The canonical epoch transcript contains the effective leases, frozen snapshot,
authenticated receipts, and closed qualification in the same ordering used by
schema 1. An epoch digest binds a dedicated domain, the epoch number, and that
transcript. The history root update binds a second domain, the previous root,
and the epoch digest. A schema-2 state hash binds its own domain, the contracts
parameter commitment, the history root, retained closed-epoch digests in epoch
order, the open-epoch transcript, and the lifecycle digest. Lengths and absence
markers must be encoded canonically. Every state-changing block recomputes only
the open bucket's transcript; closed-bucket transcripts are immutable.

An identity whose final action is deregistration or whose final registration
has expired before the retained floor has its lifecycle record hashes folded
into `history_root`. The registry drops that history and, after explicit
deregistration, keeps its identity and service-key hashes in a sorted vector. This preserves the
permanent re-registration rejection after explicit deregistration. Expiry-only
folds leave operator and service keys reusable; a renewal remains valid until
the identity is folded. Schema 2 commits the vector with a cached `fast_hash`
of its sorted contents, updated at folds; schema 1 retains its original hash
encoding. A live identity keeps its most recent record before the floor as a
baseline and all records effective at or after the floor. Earlier records are
folded in identity and record order. The memory bound is the retained window,
one baseline record per live identity, retired-key vector capacity, and the
removed records still owned by fold journals inside the disconnect horizon.
New schema-2 records cannot become effective more than eight epochs after
inclusion, and their leases cannot exceed 30 epochs from their effective epoch,
so future scheduling stays within the window-sized bound. Schema 1
keeps its original scheduling rule.
The lifecycle future-effective bound uses the same configured retention value
as the fold floor.

The lifecycle rules that read older records are: retroactive-change rejection
uses the inclusion and minimum-effective epochs; duplicate detection compares
record hashes, but a duplicate older than the floor is rejected as retroactive;
service-key conflict checks compare effective intervals and explicitly retired
service keys, for which the
baseline record and in-window records suffice; descriptor lookup selects the
latest effective record at the requested retained epoch; key reuse checks the
permanent retired-key vector. Already accepted future-dated schema-1 records
must be kept until their effective epochs pass; they are the only temporary
exception to the post-fork window-sized lifecycle history. No rule requires
full past record history.

The fork-gated implementation folds membership buckets and old lifecycle
records, caches epoch digests, and commits the contracts parameter
hash in schema 2. Boundary images are persisted and used for startup and deep
reorg replay after full validation. Mainnet and stagenet remain unscheduled;
the testnet and devnet test schedules are unchanged.

## Historical-read audit

The maximum lookback below is relative to the open epoch at the time of the
read. The HF17 path keeps the existing behavior. Under schema 2, a request for
an epoch older than the retained floor must return an explicit
`outside_retained_window` status before attempting lookup. A missing retained
record is a different status.

| Read and call site | Maximum normal lookback | Status outside window |
| --- | ---: | --- |
| Admission target descriptor, `semantic_batch_v2.cpp:150`; admission bucket, `membership_v2.cpp:485` | Target is next epoch; context one earlier | `semantic_status_v2::outside_retained_window`; `pipeline_status_v2::outside_retained_window`. |
| Freeze descriptor and bucket, `semantic_batch_v2.cpp:307`, `membership_v2.cpp:577` | Target next epoch | `pipeline_status_v2::outside_retained_window`, propagated as `block_transition_status_v2::outside_retained_window`. |
| Receipt challenge snapshot, committee and slot, `membership_v2.cpp:707` | Current service epoch | `pipeline_status_v2::outside_retained_window`, propagated through semantic and transition statuses. |
| Qualification close, `membership_v2.cpp:791` | Current service epoch | `pipeline_status_v2::outside_retained_window`, propagated by the transition. |
| Payout qualification, snapshot and lifecycle descriptor, `coordinator_v2.cpp:153-200` | 1 closed epoch (`payout_epoch - 1`) | `coordinator_status_v2::outside_retained_window`; reward planning fails. |
| Historical payout mapping, `blockchain.cpp:1750-1860` | 1 epoch for validation; arbitrary for old RPC block | RPC `EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW`; lower mapping sets `outside_retained_window`. |
| Lifecycle latest identity and service-key collision, `lifecycle_v2.cpp:273-395` | Retained epochs plus one baseline record | Retired operator-key hashes remain committed. |
| Direct lifecycle epoch descriptor, `lifecycle_v2.cpp:423-458` | Caller supplied, unbounded | **Open:** pointer/vector return values do not distinguish a folded epoch from absence. |
| Direct snapshot, qualification, admission, slot, coverage and committee lookups, `membership_v2.cpp:850-1105` | Caller supplied, unbounded | **Open:** pointer, boolean and vector return values do not distinguish a folded epoch from absence. |
| Relay duplicate and committee checks, `relay_pool_v2.cpp:326,553,588` | Current receipt round | `relay_ingress_status_v2::outside_retained_window` on ingress; stale queued receipts are removed. |
| Chain and producer helpers, `blockchain.cpp:get_epose_membership_snapshot_v2` and related queries | Current or next epoch normally; arbitrary when called directly | `Blockchain::epose_query_status::outside_retained_window` distinguishes folded epochs from absent retained records. |
| ChainLock quorum source, `blockchain.cpp:restore_chainlock_membership` | 1 epoch live; arbitrary historical | Reconstructs retained historical membership and qualification from canonical state, verifies commitments and reports unavailable data beyond ChainLock retention. |
| RPC service nodes, epoch/status, rewards and diagnostics, `core_rpc_server.cpp` | Caller supplied, unbounded | Historical endpoints return `EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW`; current epoch endpoints stay inside. |
| Schema-1 state hash, `membership_v2.cpp` and `lifecycle_v2.cpp` | Entire history | Continue exact schema-1 bytes before activation. |

The block context source may fetch a round anchor from canonical block storage;
that is a block-hash read, not a semantic-state-history read. A deep reorg may
need canonical blocks older than the window and must use a validated snapshot
or full replay.

The coordinator, semantic transaction, epoch transition, relay ingress and
chain query paths return explicit stale-history statuses. ChainLock quorum
selection reconstructs retained historical inputs when their member and
qualified-key vectors have been pruned.

## Persistence and recovery

Each epoch-boundary image binds state schema, parameter commitment, height,
canonical block hash, state root, canonical semantic serialization and checksum.
Images inside the three-epoch disconnect horizon remain available, along with
the newest earlier canonical image. Images above the tip or without a matching
main-chain commitment are ignored and pruned. Each image is written in the same
LMDB transaction as its block; disconnection removes the block's image. Startup
and deep reorg select the newest valid image at or below the target, verify its
bindings against the canonical block and stored commitment, recompute the state
root, then replay to the tip. A failed restore falls back to an earlier image;
replay mismatch triggers full block replay. Serialization of active lifecycle
histories and retained membership buckets is canonical and has no pointer or
allocator data.

## Compatibility gate

The schema-1 golden fixture pins the state hash after each block of a
multi-epoch chain and across an epoch-close reorg. Schema-2 tests compare
genesis replay with a node that ran through the fork and cover reorgs across
the fork and a fold. The 48-epoch soak exercises admission, receipts and new
retired and expired identities, plus live renewals and updates, and compares a
restored semantic image with uninterrupted replay. The soak bounds state growth
by retained-window contents, retired-key vector capacity, and removed records
owned by retained fold journals. Image
tests reject corrupt, truncated, wrong-parameter and wrong-block-hash data.
