# ChainLock VM proof vector

The `chainlock_vm_state_proof.json` file is emitted by the `gen_chainlock_rpc`
core test from its fakechain. The locked block writes the
registry-style `re` contract state key. The response objects are serialized
with the same response filling functions used by daemon RPC.

Regenerate after building `core_tests`:

```sh
QWC_CHAINLOCK_VECTOR_PATH=tests/data/chainlock_vm_state_proof.json \
  build/release/tests/core_tests/core_tests --generate_and_play_test_data --filter 'gen_chainlock_rpc'
```

Run the command from the repository root. The vector contains the hex VM key,
a complete `get_chainlock` response and a historical `get_vm_state_proof`
response at the locked height. The vector is written from the playback core's
proof and quorum-input methods after the carrier is accepted.
The extra network and chain context fields supply the ChainLock signature
domain for offline verification. The carrier block blob lets a client check
that coinbase tag `0x09` contains the returned lock blob. The
`get_chainlock_quorum_inputs` object includes the paged RPC fields for the
fakechain test override. It lets the Python helper recompute the quorum from
the snapshot and qualification keys and the anchor block ID.
