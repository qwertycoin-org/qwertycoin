# EPoSE-v2 memory ownership, undo and retention

## Scope and reference

Audited reference: `1e479cc9feda48e07dd07ba6abcb89ca3934b5e2`
(`qwertycoin-org/qwertycoin`, v2.0.4 baseline). This change is a local
representation optimization, **not** a consensus activation or pruning rule.
RandomX still determines block production and chain selection. Reward rules,
record authentication, wire encodings, parameter hash, DB schema and canonical
state commitments are unchanged. No hard fork or migration is required.

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
- `membership_pipeline_v2::leases_`: one stored lease per identity/target epoch;
  higher sequence replaces an existing slot, new epochs append slots.
- `receipts_`: accepted receipt-slot hashes/metadata across all epochs (not full
  signed wire receipts).
- `snapshots_`: frozen membership values, each with its member vector.
- `qualifications_`: closed qualification values and qualified-key vectors.

Other paths reviewed:

- `state_hash()`: lifecycle sorts pointers; membership previously sorted full
  lease/receipt copies. Membership now sorts pointers with the **identical**
  comparator and serializes the identical fields. The full canonical byte string
  still exists temporarily; hashing remains history-sized CPU and memory.
- `relay_pool_v2.cpp`: relay admission retains one isolated semantic preview and
  a bounded queue candidate. Per-record semantic candidates are replaced with
  journals (slot variants undo their preview). Receipt pruning now reuses one
  isolated preview and rolls each probe back. Exceptional late-receipt
  classifications still copy a membership pipeline.
- `Blockchain::select_epose_template_records_v2`: one isolated semantic preview
  remains; redundant per-selection copies are removed. In-order dependencies
  and exclusion of failed records are preserved by transactional application.
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
  There is no persistent materialized historical lifecycle/receipt index today.
  Startup, deep reorg and successful DB-batch abort recovery rebuild from the
  canonical chain (`cleanup_handle_incoming_blocks`), checking commitments.

### Commitment serialization

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

## Necessary retention

All five historical container types enter the existing `state_hash()`.
The table separates the recent *validation working set* from the current
commitment/historical-query requirement. Persistence today means original
on-chain records and per-block commitments, with deterministic replay; it does
not mean an existing random-access historical-state index.

| Data | Last necessary logical access | RAM required today | Persistence / possible eviction condition | Reorg behavior |
| --- | --- | --- | --- | --- |
| Lifecycle records/descriptors | Future authorized admissions, reward binding for source epoch, service-key interval conflicts, exact idempotence, latest sequence and terminal deregistration, historical RPC and every state hash | Complete canonical history, including a descriptor created before a proposed window but still active inside it | On-chain records; no age-only deletion. An archive index must preserve latest/terminal identity facts, every relevant half-open key interval and predecessor descriptor spanning the retained range | Reverse appended records/new identities; deep replay reconstructs the exact history |
| Admission leases | Target-epoch enrollment cutoff/freeze; upcoming leases whose canonical admission context is available; then historical commitment | All canonical slots, including future leases regardless of age | On-chain records; possible cold backing only after preserving slot replacement, duplicate checks and exact hash ordering | Restore prior lease plus inclusion height for replacements; truncate new slots |
| Membership snapshots | Receipt authentication/committee selection through the inclusive evidence deadline, next-epoch payout source, historical epoch/round RPC, every state hash | All snapshots including old verifier assignments | Derived by replay; cold backing must support historical query/commitment reads before releasing values | Erase only snapshots inserted by disconnected blocks; refreeze on reconnect |
| Receipt hashes/metadata | Slot idempotence/conflicts in the admissible round window and qualification close; later historical receipt coverage and every state hash | All accepted slot metadata | Original signed records on-chain; old metadata could move to indexed cold storage only with exact canonical serialization and historical query access | Truncate appended receipt slots; qualification close undone separately |
| Qualification sets | Source epoch is payout epoch minus one, used throughout the next epoch; then historical reward queries and every state hash | All closed sets | Derived by replay; archive access required for old queries and hashes | Erase only sets closed by disconnected blocks; close again using canonical receipts |
| Undo journals | Disconnect of any of the last 2160 accepted blocks | Fixed per-block metadata plus actual changes in that window; not historic state copies | Not persisted; evict oldest **only after** accepting a new block | Inside horizon LIFO journal; outside horizon return `deep_replay_required` without altering state |
| Relay records/diagnostics | Local transport deadlines / bounded template selection | Existing queue item/byte limits and local counters | Never authoritative; expiry/dedup rules unchanged | Revalidate against canonical contexts; never restore relay as consensus state |
| Hash/RPC/template/replay temporaries | End of call or rebuild | Call-scoped allocations, sometimes history-sized | None; destruct after use | Canonical caller locking and replay verification unchanged |

Receipt windows are derived by `receipt_inclusion_window_v2`: round 0 starts at
epoch start, later rounds at round-anchor height + 1; nonfinal rounds end before
the next anchor. The final round ends at `evidence_deadline`, which is inclusive.
Membership freezes at `committee_anchor` before that block's records; its cutoff
is anchor - 1. Qualification closes after the deadline block's records. Reward
planning reads the settled previous source epoch throughout the payout epoch.

### Current epoch plus seven previous epochs

Eight epochs comfortably cover ordinary recent receipt and payout lookups, and
the 2160-block undo horizon is three 720-block epochs. But eight is **not a valid
global deletion rule**: old records still enter every state hash; old identities
can authorize current/future descriptors; active predecessor descriptors can
predate the window; service-key conflict checks inspect historical validity
intervals; admissions can target future epochs; RPC requests can address older
epochs. A fixed recent window alone therefore cannot preserve present behavior.

No history pruning is implemented here. A genuinely bounded resident history
would require a separate, explicitly designed LMDB-backed historical store:

1. Versioned identity/sequence records, an interval index by service key,
   identity/target-epoch lease slots, epoch membership/qualification values and
   `(epoch, round, subject, verifier)` receipt-slot metadata, plus exact canonical
   ordering indexes for the existing hash serialization.
2. Atomic updates with block/commitment writes and inverse operations on reorg;
   deterministic, resumable rebuild and consistency checks against canonical
   records and existing commitments. DB batch abort must discard uncommitted
   index/cache generations together.
3. Bounded read-through caches pinned for validation/admission/payout, including
   active predecessor descriptors and future leases; streaming the **same**
   canonical bytes from historical indexes to the existing hash construction.
   This needs serialization-equivalence tests, not a substitute incremental hash.
4. Historical RPCs must read indexed cold data without full-chain scans, and deep
   replay/restart must operate correctly with an empty cache.

That is separate storage work, with migration/rebuild/rollback tests. Replacing
the commitment with a Merkle or different incremental digest would be a protocol
change and is explicitly outside this optimization.

## Journal design and failure behavior

The block journal checkpoints initial identity/lease/receipt lengths and previous
tip metadata. It owns only old lengths for modified lifecycle histories, prior
values/inclusion heights of replaced leases, and inserted snapshot/qualification
epoch keys. Newly appended records are undone by truncation. Repeated mutations
are logged in order and undone in reverse. No pointer into a reallocating vector
is retained. Boundary-only blocks are journaled too.

Retained undo is proportional to the undo window plus the changes inside it,
not the history preceding it. The single canonical state still grows with all
historical records; this optimization does not claim a hard bound on total RAM.

Each nonempty transaction uses a nested checkpoint. On failure or exception its
scope guard restores it, without appending partial logical entries to the block
journal. On success all destination journal vectors reserve before any entries
are merged; stored entry types are fixed-size, nonthrowing values. Mutations log
before potentially throwing allocations. A block scope guard restores earlier
successful transactions and boundary transitions on any later failure. The
oldest undo is not evicted until the entire block is accepted. Empty transactions
still execute the original configuration/height checks but allocate no journal.

Rollback truncation releases removed objects and their owned nested vectors;
erasing maps and evicting journals releases their allocations. Outer lifecycle,
lease and receipt vectors retain high-water capacity after truncation, as
reported by `memory_usage()`. No `shrink_to_fit` occurs during a `noexcept`
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

### Local build resource qualification

The Debian 12 / GCC 12.2 / CMake 3.25.1 container has a 3.5-GiB cgroup memory
limit shared with other processes. The unchanged `core_rpc_server.cpp` compiler
was OOM-killed at default Release flags and again with `-O0` alone; the unchanged
`daemon.cpp` and `rpc_command_executor.cpp` also exceeded available compiler memory. The local validation build
therefore uses one compiler and build-directory-only source overrides for those
three files: `-O0 -g0 --param=ggc-min-expand=5
--param=ggc-min-heapsize=4096`. EPoSE and the probes retain the normal `-Ofast`,
`-DNDEBUG`, native-architecture build flags. The override is not a source or CI
change, and this is not evidence of an EPoSE runtime failure. A default-flags
daemon build remains a separate required CI gate; do not describe the local
daemon result as an entirely default Release build.

The local focused suite completed **282/282 tests**, with no disabled tests,
failures or errors (109.446 seconds). This includes all six new regressions and
the 360-fast-epoch/6480-block workload with a constant 2160-entry rolling undo
payload, full-window parent-hash restoration and deep-replay signaling.

The local daemon also compiled and linked successfully with the qualified
low-memory overrides above. No production daemon was started or changed.

### Independent reference comparison

Identical probe source was built against the unmodified EPoSE implementation at
the pinned reference and against this change, using the same pinned dependencies
and toolchain. `EPOSE_MEMORY_BASELINE` excludes only the new diagnostic calls.
The reference worktree adds only the probe/CMake target; dependency sources are
reused at their unchanged pinned revisions. No production code is substituted
into the reference EPoSE library.

The POSIX helper compared **all 591 individual block hashes**, all 32 epoch
receipt counts and the final trace. Both binaries accepted all blocks, stored
1152 receipts, qualified all four identities each epoch, and restored each of
36 disconnected parents. Their trace is identical:

```text
5755eda11e4e2642ec4288ca68f0671601edd16d32a1e3c645d4f8953580409c
```

| Single synthetic 32-epoch run | Snapshot reference | Journal implementation |
| --- | ---: | ---: |
| Child-process peak RSS (KiB) | 343304 | 275820 |
| Wall time (seconds, observational only) | 33.375 | 34.340 |
| Canonical block hashes compared | 591 | 591 |
| Verified disconnects | 36 | 36 |

Peak RSS decreased by 67484 KiB (about **19.7%**, 335.3 to 269.4 MiB) even
though the common RandomX cache dominates this small workload. The journal's
epoch-32 measured undo payload was 85616 bytes (83.6 KiB); canonical state
payload/capacity accounting was 344064 bytes. This workload does not fill the
2160-block horizon; the separate 6480-block regression exercises the full rolling
window. These are one-run diagnostic observations, not latency statistics, a
speedup claim, an allocator-overhead census, or a mainnet memory forecast.
