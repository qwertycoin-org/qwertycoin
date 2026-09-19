# QMS MVP offline test report

Date: 2026-09-19

## Baselines

- Paper Core: `54308d8473dc5606d054c0ba428cfb2d64e758c1`
- Implementation Core base: `361b1fa003c749275a19ea7b889687c5a90379c9`
- Paper/current GUI base: `ecd1844f1b2e3416dec16c07a21e6850e11b630c`
- GUI submodule after implementation: feature commit containing the QMS Core commits

The current Core base adds work after the paper baseline but retains outer nonce tag
`0x02`, 255-byte nonce payloads and the 1,060-byte normal relay extra limit. It also
revealed that inner subtype `0x71` is occupied by EPoSE attestation, so QMS uses `0x72`.

## Executed offline commands

```sh
cmake -S . -B /workspace/build-qms-core -G Ninja \
  -DBUILD_TESTS=ON -DBUILD_GUI_DEPS=ON -DMANUAL_SUBMODULES=ON \
  -DUSE_DEVICE_TREZOR=OFF -DUSE_READLINE=OFF -DSTACK_TRACE=OFF \
  -DCMAKE_CXX_FLAGS_RELEASE='-O1 -DNDEBUG' \
  -DCMAKE_C_FLAGS_RELEASE='-O1 -DNDEBUG'
cmake --build /workspace/build-qms-core --target qms_unit_tests -j1
/workspace/build-qms-core/tests/unit_tests/qms_unit_tests --gtest_color=no
```

Result: 8/8 focused QMS tests passed. Coverage includes invitation signature and
tampering, A→B encryption/decryption, wrong recipient, wrong pinned sender, wrong
genesis, ciphertext tampering, UTF-8 and 4,096/4,097-byte boundaries, canonical
fragment/segment reorder and reassembly, missing/conflicting data, fragment MAC,
existing nonce serialization at 127/128/255 bytes, rejection at 256, and unchanged
`MAX_TX_EXTRA_SIZE == 1060`.

```sh
cmake -S . -B /workspace/build-qms-gui -G Ninja \
  -DMANUAL_SUBMODULES=ON -DUSE_DEVICE_TREZOR=OFF -DBUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/workspace/qwc-gui-qt-prefix.8255N7/root/usr \
  -DCMAKE_CXX_FLAGS_RELEASE='-O1 -DNDEBUG' \
  -DCMAKE_C_FLAGS_RELEASE='-O1 -DNDEBUG'
cmake --build /workspace/build-qms-gui --target qwertycoin-gui -j1
```

Result: Linux Qt wallet compiled and linked successfully. It was not launched because
runtime GUI startup could select or contact a configured node.

The initial broad unit-test target attempt used parallel compilation and was killed by
the container memory limit. The focused QMS target was then separated from broad tests
and built serially. No test failure was hidden by this resource issue.

## Compatibility evidence and limits

The focused carrier tests call the unchanged Core `add_extra_nonce_to_tx_extra`,
`sort_tx_extra` and `parse_tx_extra` implementations. Wallet construction re-parses the
final signed transaction and compares its QMS fragment byte-for-byte before review.
The Linux Qt binary and wallet API compile against the current Core.

Not empirically covered offline:

- daemon mempool acceptance with real chain/ring context;
- dynamic fee/output selection against a real wallet balance;
- a complete transaction-builder fixture with realistic decoy outputs;
- macOS, Windows and hardware-wallet builds;
- real reorg delivery from a daemon.

Those gaps do not justify a live test under this assignment. Mainnet acceptance remains
Alex's manual two-wallet step. Hardware wallets, multisig, watch-only, integrated
addresses, user payment IDs and light-wallet services are rejected by the MVP path.

**No node was started, no QWC network connection was opened, and no real transaction
was sent.**

