# QMS2 offline test report

Date: 2026-09-25

Status: working-branch evidence for review. This report is not a release, deployment,
network-compatibility, or security-audit approval.

## Source and dependency baseline

- Core merge base: `361b1fa003c749275a19ea7b889687c5a90379c9`
- libsignal: tag `v0.103.1`, commit
  `e8cc2dddd578859b4a029c9c94670b24ce2b616a`
- Rust toolchain: `1.98.1`
- native/browser crypto ABI: `3`
- outer carrier subtype: `0x72`; EPoSE retains `0x70` and `0x71`

The reviewed pull request and its CI record the immutable implementation commit and
generated-artifact hashes. The libsignal dependency is AGPL-3.0-only and unsupported
outside Signal; independent license/compliance approval remains a release blocker.

## Core and crypto tests

The pinned Rust toolchain executed:

```sh
cargo fmt --check
cargo test --locked
```

Result: 6/6 tests passed. They cover FFI pointer/buffer ownership, contact import,
4,096/4,097-byte boundaries, tampered packages, real PQXDH, ongoing Triple Ratchet,
out-of-order delivery, replay/tamper rejection, and the outer-secret
offer/acknowledge/grace/deletion sequence.

The focused C++ target executed:

```sh
cmake --build /tmp/qms2-core-build --target qms_unit_tests -j1
/tmp/qms2-core-build/tests/unit_tests/qms_unit_tests
```

Result: 17/17 tests passed. Coverage includes strict SOCKS5/Tor-v3 configuration,
invitation binding, the shared Core/browser vector, XChaCha20-Poly1305 framing,
canonical padding classes through 9,600 bytes, fragmentation and nonce varint limits,
unchanged `MAX_TX_EXTRA_SIZE == 1060`, the Argon2id/XChaCha encrypted store, native
PQXDH/Triple-Ratchet ABI, repeated/lost outer-secret rotation offers, one retiring
context, restart/idempotency, history default-off/opt-in/delete, password migration,
corruption rejection, and 4,096 deterministic parser mutations. The dedicated QMS fuzz
harness also exercised fragment, transaction-extra carrier, and invitation decoders on
empty, unit-test-source, and fuzz-source inputs.

The same 17 tests and fuzz inputs passed in a separate AddressSanitizer build. Leak
detection was disabled because the inherited executable/signal-handler stack crashes
during LeakSanitizer startup on this runner; this is a runner/toolchain limitation, not
a passed leak check. No ASan memory error was reported by the completed runs.

The source size vector used exact 4,096-byte UTF-8 text. The first real PQXDH message
serialized to 5,953 bytes before outer framing and fit the mandatory 7,200-byte class.
A full 9,600-byte envelope produces 16 fragments and exactly 726 bytes of serialized
transaction extra for each full carrier.

## CLI build evidence

The modified `simplewallet.cpp` compiled successfully with the production
defines/includes from CMake's generated compile command and `-O0 -g0` in place of the
release optimizer. This proves the new `qms_history` and `qms_reset` command surface and
its native controller compile.

An initial standalone release build exhausted this runner while compiling the
pre-existing, very large `wallet2.cpp` translation unit, even at `-j1`; that failed run
was not counted as evidence. The same exact Core source was then rebuilt by the native
GUI integration build with its populated compiler cache. That complete Release build
successfully compiled and linked `qwertycoin-wallet-cli`, `qwertycoin-wallet-rpc`, and
the GUI. The QMS2 CLI compile/link gate is therefore satisfied on Linux, while the new
pull-request workflow independently rebuilds `simplewallet` from a clean checkout.

## Browser/WASM evidence

The same Rust wrapper compiled for `wasm32-unknown-unknown`; wasm-bindgen executed the
real PQXDH/Triple-Ratchet path in Node. The local Web Wallet offline suite passed 15/15
tests with ABI 3, including Core-vector equivalence, 4,096-byte first-message framing,
out-of-order reassembly, duplicate/reorg behavior, outer-secret rotation, encrypted
store corruption, history policy, explicit restore reset, and fail-closed ordinary
browser transport.

The distributable proof was rebuilt by the TypeScript push workflow from the complete
pinned Core → C++ bridge → TypeScript/WASM graph. Artifact
`qms-qwertycoin-ts-dist-69161c3af8e536dae7d7f1e740dcde2782d89d99`
(GitHub artifact ID `10836732235`) recorded the expected source graph and SHA-256
manifest. A separate download independently compared every recorded hash before the
Web Wallet vendored the artifact; its full offline suite then passed. The Web commit is
kept local because pushing that repository would create a public preview deployment,
which this assignment explicitly forbids.

## Explicitly not exercised

- no daemon, wallet RPC, P2P peer, public gateway, or Tor process;
- no transaction construction against live outputs, relay, mempool, mining, or reorg;
- no funded wallet or real fee;
- no release, deployment, merge, or public test;
- no macOS, Windows, hardware-wallet, multisig, or light-wallet acceptance;
- no claim that a complete rollback of all consistent local files is automatically
  detectable.

Those gaps require the separate reviewed manual procedure and independent security and
license review. They are not permission to weaken the protocol or silently fall back
to profile 1 or direct network transport.

**No node was started or created, no live test was performed, and no real transaction
was sent.**
