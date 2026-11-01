# EPoSE history retained in memory

At 120 seconds per block, one year has 262,800 blocks and 365 epochs of 720
blocks. The payload figures below use one accepted lifecycle record, admission,
and receipt per block, plus one snapshot and qualification per epoch. They
exclude allocator overhead, map indexes, vector spare capacity, and undo
entries. They are illustrative rates, not consensus maxima.

| Record | Approximate payload per insertion | Approximate annual payload |
| --- | ---: | ---: |
| Lifecycle record | 392 bytes, plus 56 for a new identity | 103,017,600 bytes, plus identity overhead |
| Admission lease | 336 bytes | 88,300,800 bytes |
| Authenticated receipt | 128 bytes | 33,638,400 bytes |
| Snapshot | At least 120 bytes, plus 200 per frozen member | At least 43,800 bytes, plus membership payload |
| Qualification | At least 112 bytes, plus 32 per qualified key | At least 40,880 bytes, plus qualified-key payload |

Membership now stores records in per-epoch buckets. Closed buckets cache their
canonical lease and receipt transcript segments. Admission and receipt indexes
avoid historical linear scans. Block connection records changed leases,
receipts, snapshots, qualifications and folded buckets in a LIFO undo journal.
Lifecycle records use the same journal for appends, trims, removals and index
changes. Schema 1 still retains every historical record. The fork-gated schema-2
path folds membership buckets older than eight epochs, identities that have
expired or deregistered before the retained floor, and older records of live
identities. It keeps a sorted vector of retired identity and service-key hashes to
reject re-registration. Each live identity keeps its most recent record before
the floor and every record effective at or after the floor. Retroactive-change
and duplicate checks need no older record; service-key overlap and epoch
descriptor lookup use the baseline and retained records; key reuse uses the
retired-key vector. Schema-2 records can be scheduled at most eight epochs
ahead and have a maximum 30-epoch lease. Future-dated schema-1 records accepted before activation remain until
their effective epochs pass; no lifecycle rule needs unbounded past history.
Boundary images
persist the semantic state in LMDB as described in
[ADR-0011](review/ADR-0011-ROLLING-EPOCH-WINDOW.md).

The existing eight-epoch soak registers one identity at each epoch start and
allows a one-megabyte post-warmup increase. The 48-epoch semantic-state soak
admits fresh identities, accepts receipts, retires the preceding identity,
checks rejected re-registration after folding, and bounds the increase in
`state_bytes` to retained-window contents plus the retired-key vector and
allocator capacity. Retained fold journals also own removed records until
their undo entries expire. The
permanent retired-key vector grows with explicitly deregistered identities and
their service keys. Expiry-only folds permit re-registration with the same keys.

ChainLock carrier epochs retain their snapshot and the preceding qualification
inside the same epoch-bucket representation. Other old buckets keep their
lease and receipt records and commitment hashes while their member and
qualified-key vectors are pruned after two epochs. The unrestricted ChainLock
RPC reconstructs those vectors from the retained bucket records and verifies
the original commitments before selecting a quorum. Carrier retention and
pruning do not enter the EPoSE state hash or epoch digest. Full archived buckets
are retained only when their distance from the tip epoch is at most
`ceil(2160 / epoch_length) + 1`: four epochs with the 720-block production
epoch. The extra epoch supplies the preceding qualification for the oldest
eligible ChainLock. Carrier markers use the same bound. The archive, including its map
entries and vector payloads, is counted in `memory_usage()`. At one admission
and receipt per block, the approximate archived payload is 334,080 bytes per
720-block epoch (720 × (336 + 128) bytes), plus snapshot members, qualification
keys, indexes, allocator overhead, and vector capacity. Boundary images persist
only retained archived buckets and carrier markers. Reconstruction of an older
epoch reports it as pruned. A carrier arriving after pruning records its epoch
and uses the reconstruction path while the bucket remains retained. Fork choice
skips a ChainLock whose membership is unavailable and uses the normal fork rule.
