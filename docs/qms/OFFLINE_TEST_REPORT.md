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

Result: 16/16 tests passed. Coverage includes strict SOCKS5/Tor-v3 configuration,
invitation binding, the shared Core/browser vector, XChaCha20-Poly1305 framing,
canonical padding classes through 9,600 bytes, fragmentation and nonce varint limits,
unchanged `MAX_TX_EXTRA_SIZE == 1060`, the Argon2id/XChaCha encrypted store, native
PQXDH/Triple-Ratchet ABI, repeated/lost outer-secret rotation offers, one retiring
context, restart/idempotency, history default-off/opt-in/delete, password migration,
and corruption rejection.

The source size vector used exact 4,096-byte UTF-8 text. The first real PQXDH message
serialized to 5,953 bytes before outer framing and fit the mandatory 7,200-byte class.
A full 9,600-byte envelope produces 16 fragments and exactly 726 bytes of serialized
transaction extra for each full carrier.

## CLI build evidence and runner limit

The modified `simplewallet.cpp` compiled successfully with the production
defines/includes from CMake's generated compile command and `-O0 -g0` in place of the
release optimizer. This proves the new `qms_history` and `qms_reset` command surface and
its native controller compile.

A complete `simplewallet` link could not be produced on this runner: GCC's compilation
of the pre-existing, very large `wallet2.cpp` translation unit was killed by the host
memory limit both with the normal release optimizer and with `-O0 -g0`, even at `-j1`.
There was no compiler diagnostic in the changed QMS2 or CLI sources. A clean full CLI
build on the repository's normal CI runner remains mandatory and must not be reported
as passed until that job succeeds.

## Browser/WASM evidence

The same Rust wrapper compiled for `wasm32-unknown-unknown`; wasm-bindgen executed the
real PQXDH/Triple-Ratchet path in Node. The local Web Wallet offline suite passed 15/15
tests with ABI 3, including Core-vector equivalence, 4,096-byte first-message framing,
out-of-order reassembly, duplicate/reorg behavior, outer-secret rotation, encrypted
store corruption, history policy, explicit restore reset, and fail-closed ordinary
browser transport.

This local artifact is not the distributable proof. The TypeScript CI must rebuild the
complete pinned Core → C++ bridge → TypeScript/WASM graph, upload it, and an independent
job/operator must compare every recorded SHA-256 before the Web Wallet vendors it.

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
