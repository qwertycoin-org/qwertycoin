# ChainLock quorum derivation for light clients

`/get_chainlock` returns the lock and an ordered quorum for discovery. Decode
`lock_blob` and require its epoch and the reply's `quorum_epoch` to equal
`lock.height / epoch_length` using integer division and the client's pinned
`epoch_length`. Reject a mismatch. Fetch quorum inputs for this derived epoch,
never for a node-supplied epoch, and derive the quorum
locally before checking any signature. The separate query keeps the lock reply
small. Its request is `{"epoch": e, "offset": 0, "limit": 256}`. `limit` must be
1–256; the default is 128. Both `snapshot_members` and `qualified_members` use
the same offset into their respective arrays. Advance by the requested limit
until each collected length equals `snapshot_total` or `qualification_total`.
Keep every page's metadata identical; restart if any block ID, hash, total, or
other metadata changes. A `Quorum inputs unavailable` status means the epoch
has not been reached or its membership state is unavailable. The endpoint is
also available through JSON RPC as `get_chainlock_quorum_inputs` and on
restricted RPC.

## Bind the inputs to the followed chain

All keys are 32 raw bytes encoded as lowercase hex. Hashes and block IDs are
32 raw bytes encoded the same way. `network_id` is the network's 16 raw UUID
bytes, not its RPC network number. `genesis_hash` is the block ID at height 0.
`parameter_set_hash` is the active EPoSE parameter commitment. The client must
pin `activation_height`, `epoch_length`, `anchor_depth`, `network_id`,
`genesis_hash`, and `parameter_set_hash` from its own network configuration.
Compare every RPC value with the pinned value and reject any mismatch. The
first lockable epoch is `activation_height / epoch_length + 2`: snapshot `e`
needs `e >= activation_epoch + 1`, and qualification `e - 1` needs
`e >= activation_epoch + 2`. Parse all
heights and counts as unsigned 64-bit integers.

For epoch `e`, `epoch_start_height = e × epoch_length` and
`previous_epoch_start_height = (e − 1) × epoch_length`, with checked unsigned
arithmetic. `snapshot_height = epoch_start_height − anchor_depth` and
`qualification_height = snapshot_height − 1`. The snapshot at
`snapshot_height` freezes the service keys admitted by the end of
`snapshot_height − 1`. Qualification for `e − 1` closes at
`qualification_height`. Match every returned `*_block_id` with the followed
chain at its height, including both epoch starts. The `snapshot_block_id` is
the quorum selection anchor and seed. The `snapshot_hash` and
`qualification_hash` identify the node's retained EPoSE state. These hashes
are not standalone Merkle proofs of the listed keys. Independently replay the
admission, membership, and receipt rules through those blocks to authenticate
the key sets; comparing the RPC values only with each other is insufficient.
The `test_override` field is true only for fakechain core tests, which inject
the same keys into both sets; a production client must reject it.

## Derive the ordered quorum

1. Decode every `snapshot_members` entry as a 32-byte service public key and
   every `qualified_members` entry as a 32-byte service public key. Require
   complete pages and counts. Reject duplicate keys within either set. Use
   exact byte equality to form the
   candidate intersection: `snapshot(e) ∩ qualified(e−1)`.
2. For each candidate key `k`, concatenate these bytes without lengths or
   terminators:

   ```text
   ASCII("QWC_CHAINLOCK_QUORUM_V1")
   || network_id[16]
   || genesis_hash[32]
   || epoch_u64_le[8]
   || snapshot_block_id[32]
   || k[32]
   ```

   Hash the concatenation with CryptoNote `cn_fast_hash`: legacy Keccak-256,
   rate 136 bytes, Keccak padding suffix `0x01` and final `0x80`. NIST
   SHA3-256 has different padding and produces the wrong scores.
3. Sort candidates by score as an unsigned lexicographic comparison of the
   32 hash bytes. Break an equal-score tie by unsigned lexicographic
   comparison of the 32 key bytes. Take the first `min(50, candidate_count)`
   keys **in that rank order**. This is the signer bitmap order, including
   when the candidate count is at most 50.
4. Fewer than 10 selected keys means PoW only: there is no valid ChainLock.
   Otherwise the exact required signer count is
   `ceil(68 × selected_count / 100)`. Match the derived keys and threshold to
   `/get_chainlock` only as a consistency check; use the derived values for
   verification.

The parameter set hash does not enter the quorum score. It enters the signed
message hash. For a lock at `height` with `block_hash`, that hash is
`cn_fast_hash(ASCII("QWC_CHAINLOCK_V1") || network_id[16] || genesis_hash[32]
|| parameter_set_hash[32] || epoch_u64_le[8] || height_u64_le[8] ||
block_hash[32])`. `lock_blob` is `height_u64_le || block_hash || epoch_u64_le
|| signer_bitmap[7] || signatures`. Bitmap bit `i % 8` of byte `i / 8`
selects derived quorum key `i`; signatures follow set-bit index order.
Authenticate the locked and carrier blocks on the followed chain, then verify
the signatures with the locally derived quorum.

`tools/chainlock.py --vector tests/data/chainlock_vm_state_proof.json` checks
the bundled offline vector. For a live daemon, pass `--parameters` with a
locally pinned JSON object containing the six network parameters above. The helper fetches every
input page and checks the derived quorum and threshold against the lock reply.
