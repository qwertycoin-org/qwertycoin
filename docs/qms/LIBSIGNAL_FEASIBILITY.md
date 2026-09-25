# QMS2 libsignal feasibility record

Status: implementation input, not a release approval.

## Immutable upstream baseline

| Item | Pin |
|---|---|
| Repository | `https://github.com/signalapp/libsignal.git` |
| Tag | `v0.103.1` |
| Commit | `e8cc2dddd578859b4a029c9c94670b24ce2b616a` |
| Rust toolchain | `1.98.1` |
| `Cargo.lock` SHA-256 | `1fd9a14f21fb151489000876289508d31d0aeb5550684b17f9b963d0f85c3d6f` |
| License SHA-256 | `0d96a4ff68ad6d4b6f1f30f713b18d5184912ba8dd389f86aa7710db079abcb0` |
| SPQR tag / commit | `v1.6.0` / `06959b4708f9b7b1e94d0f8cc835f3958c077e94` |
| `libcrux-ml-kem` | `0.0.10`, checksum `1d8160f7d64fd2716b4fd05cc886a042f8dcda18d9206c0d506e2c67bdf97daa` |

The upstream code is AGPL-3.0-only and explicitly says that use outside Signal is
unsupported and that bridge APIs may change without notice. Qwertycoin's existing
Core/GUI code is BSD-3-Clause and the Web/WASM repositories are MIT. A release that
links or ships this dependency therefore requires an independent license/compliance
decision. This branch treats that as a release blocker; it is not legal advice.

## Proved code path

The public `libsignal-protocol` API was used, not a reimplementation:

1. `process_prekey_bundle` verifies the signed EC and Kyber prekeys and initializes
   the session through PQXDH.
2. `message_encrypt` delegates to `OutgoingTripleRatchet`.
3. `message_decrypt` delegates to the same Triple Ratchet receive path.
4. `ratchet.rs` initializes SPQR version 1 and `triple_ratchet.rs` combines the
   classical Double Ratchet with the Sparse Post-Quantum Ratchet.
5. Serialized `SignalMessage` values carry a non-empty `pq_ratchet` field after the
   handshake. The serialized `SessionRecord` contains the evolving SPQR state.

The upstream `test_basic_prekey` test passed with the pinned source and toolchain.
It covers PQXDH session creation, first-message decryption, acknowledgement, reply,
and subsequent bidirectional messages.

## Size measurements

The measurement used two fresh in-memory Signal stores, a Kyber1024 prekey bundle,
the real public session API, and 4,096-byte plaintexts. Every ciphertext was parsed
and decrypted by the peer.

| Message | Serialized libsignal ciphertext | SPQR field |
|---|---:|---:|
| first PQXDH/PreKey message | 5,856 bytes | 37 bytes |
| first reply | 4,169 bytes | 4 bytes |
| subsequent alternating messages (64 rounds) | max 4,202 bytes | max 37 bytes |

The QMS2 canonical inner record adds 105 fixed bytes to the user text plus at most
48 bytes when an outer-secret offer and acknowledgement are both present. The final
4,096-byte PQXDH vector serializes to 5,953 bytes before the outer envelope and fits
the canonical 7,200-byte class. The protocol still fails closed if any final
serialized envelope exceeds 9,600 bytes.

## Native and WebAssembly probes

- A Rust `staticlib` using the pinned protocol crate linked into a C++17 executable
  and generated a real libsignal identity (`identity_size=69`).
- The same crate compiled for `wasm32-unknown-unknown` after explicitly enabling
  the JavaScript entropy backend for both `getrandom` dependency lines.
- A wasm-bindgen export executed a complete PQXDH send/decrypt roundtrip in Node.
- The generated WebAssembly module containing the session path was 1,336,518 bytes
  before HTTP compression (SHA-256
  `197c1c592178c4d29cc49a62031f1b7dd15ac1da6c42d0eb3937839e2aebe2ef`).

Signal's published TypeScript package is a native Node add-on, not a browser module.
QMS therefore needs a small Qwertycoin-owned Rust wrapper for both a C ABI and
wasm-bindgen. The wrapper must expose only QMS operations and keep libsignal records
opaque; JavaScript and C++ must never implement ratchet primitives themselves.

## Feasibility verdict

PQXDH plus an ongoing SPQR/ML-KEM ratchet is technically feasible within the QMS
9,600-byte transport limit and on the native C++ and browser-WASM targets. Release is
still blocked on independent security review, AGPL compliance review, complete
cross-platform builds, final envelope-size vectors, and the Tor limitation described
in `NETWORK_POLICY_V2.md`.
