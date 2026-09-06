# EPoSE Source and Deployment Baseline

**Baseline date:** 2026-09-06

**Reviewed branch:** `main`

**Reviewed commit:** `1c4c1bf10c387887a42243dc690a65abb6c6e786`

**External change-order SHA-256:**
`98a846ad9539510e4146334be82b0da317483e500366936a7df979be626133cb`

## 1. Scope and evidence standard

This document establishes CO-00: the source, deployment, parameter, and
invariant baseline that later EPoSE hardening work must preserve or change
explicitly. Source statements below were checked against the reviewed commit.
Deployment statements are dated observations and are not consensus input.

Evidence labels used in the finding matrix:

- **Confirmed:** the behavior is directly present in source.
- **Partially covered:** a related safeguard exists, but it does not establish
  the full requested property.
- **Unspecified:** no normative rule or regression test was located.
- **Decision required:** a protocol/economic choice requires an ADR and owner
  approval before implementation.

This is not a complete security audit. It is an auditable inventory for the
ordered hardening program.

## 2. Repository and upstream provenance

The public source baseline is the Qwertycoin v2 import merged through PR #164
and the subsequent macOS metadata cleanup. The original import was based on
Monero `v0.18.5.1`, whose signed tag resolves to upstream commit
`4f92268d7c16741cfb41e5bbe2aa46cc260a9ea5`. The QWC import was squashed when
merged, so the current QWC tree is not byte-identical to the Monero release;
QWC-specific differences must be reviewed from the public QWC history.

Pinned submodules at the baseline:

| Component | Commit |
|---|---|
| RandomX | `6c4340ba4561aec9a3611c1aedf9931239777fb3` |
| RapidJSON | `129d19ba7f496df5e33658527a7158c79b99c21c` |
| supercop | `633500ad8c8759995049ccd022107d1fa8a1bbc9` |
| trezor-common | `bff7fdfe436c727982cc553bdfb29a9021b423b0` |

`src/version.cpp.in` identifies the QWC build as `2.0.0` / `QWC v2 Mainnet`.
The nearest inherited Git tag remains the legacy `v6.0.9` tag and is not a QWC
v2 release manifest. A signed release manifest tying source, submodules,
compiler, binaries, genesis, and activation schedule does not yet exist.

GoogleTest is vendored source under `tests/gtest`, not a Gitlink.  There is no
`external/miniupnp` Gitlink or path at this baseline.  The table above is the
complete recursive Gitlink set and is checked against the manifest by the
CO-01 regression test.

The additional DNS hardening revision
`845efabd2251c837c0dce5a2e9c142bc963a559b` is the head of open PR #166. It is
a direct child of this baseline, not part of `main`, and changes DNS-checkpoint
availability behavior without changing EPoSE consensus parameters.

## 3. Build and dependency baseline

The baseline was inspected with:

```text
CMake 3.25.1
GCC/G++ 12.2.0
ccache 4.7.5
RandomX 1.2.2 source revision as pinned above
```

Reproducibility requires recording the generator, complete CMake cache, build
type, compiler identity, target architecture, and submodule revisions. Current
project documentation covers building, but does not yet provide one signed,
machine-readable EPoSE release manifest.

## 4. Active network and protocol manifest

All values in this section describe the reviewed code. They are not approval
to change a deployed network.

| Parameter | Mainnet value | Source |
|---|---:|---|
| Active hardfork | HF17 from height 0 | `src/hardforks/hardforks.cpp` |
| Block target | 120 seconds | `src/cryptonote_config.h` |
| Display decimals | 8 | `src/cryptonote_config.h` |
| Money supply (atomic units) | `18446744073709551` | `src/cryptonote_config.h` |
| Final subsidy | 30,000,000 atomic units/minute | `src/cryptonote_config.h` |
| Address prefix | `0x14820c` | `src/cryptonote_config.h` |
| P2P / daemon RPC / ZMQ | 8196 / 8197 / 8199 | `src/cryptonote_config.h` |
| Genesis nonce | 10000 | `src/cryptonote_config.h` |
| EPoSE protocol | 1 | `src/epose/service_node.h` |
| Epoch length | 720 blocks | `src/epose/service_node.h` |
| Anchor delay (currently named finality depth) | 60 blocks | `src/epose/service_node.h` |
| Registration TTL | 30 epochs | `src/epose/service_node.h` |
| State retention | 2 epochs | `src/epose/service_node.h` |
| Service allocation | 1,000 BPS | `src/epose/service_node.h` |
| Admission target | 16 leading zero bits | `src/epose/service_node.h` |
| Committee target | 9 | `src/epose/service_epoch.h` |
| Quorum | `ceil(actual committee size * 2 / 3)` | `src/epose/service_node.cpp` |
| Registration / attestation blob | 249 / 234 bytes | `src/epose/service_node.h` |

Current `split_block_reward()` applies the 1,000-BPS share to subsidy **plus
fees**. If no qualified payee is selected, current coinbase validation requires
no service output, leaving the full permitted reward with the miner. Both are
consensus and tokenomics rules, not implementation details.

The current payout source is epoch 0 for heights 0 through 1439 as expressed by
`reward_source_epoch_for_height()`. Epoch 0 can therefore affect its own reward
source. Later hardening must not rewrite historical interpretation.

## 5. Current EPoSE state model

Registrations and attestations are carried in legacy transaction-extra nonce
subtypes `0x70` and `0x71`. A registration includes the service public key,
reward address, reward private view key, endpoint commitment, epoch interval,
admission proof, and service-key signature. An attestation includes verifier and
subject keys, epoch, challenge/response/tip hashes, `service_ok`, and only the
verifier signature.

The automatic verifier path in `cryptonote_core.cpp` constructs the challenge
and response locally, sets `service_ok = true`, signs the record as verifier,
and relays it. No authenticated exchange with the subject is performed. This
confirms F01 and is the primary reason economic hardening is a no-go.

Committee membership is computed from `registry.active_nodes(epoch)` when an
attestation is applied. There is no separately persisted immutable pre-seed
snapshot. Qualification later recomputes the committee size and counts unique
valid verifier signatures. The state transition processes registrations before
attestations within each transaction and restores an in-memory snapshot on
failure.

The miner template chooses at most one EPoSE nonce payload, prioritizing a local
registration, then a relayed registration, then a relayed attestation. The
relay pool is bounded, but there is no explicit block-wide EPoSE byte and
cryptographic-operation budget independent of the inherited transaction limits.

EPoSE state is presently held in memory. Recent per-block copies are retained;
a rollback beyond them invokes replay from block height 0. There is no dedicated
versioned LMDB EPoSE index or crash-recovery transaction boundary.

## 6. Deployment reconciliation

The network must be treated as carrying real history and balances. Read-only
inspection produced the following dated evidence:

| Target | Observation |
|---|---|
| `seed-00.qwertycoin.org` | Running container reports `QWC v2 Mainnet (v2.0.0-d5e6cf7de)`, height 323, HF17 active from height 0, and EPoSE epoch 0 with three registered and three qualified nodes. Genesis hash is `e791e506200ba3a221b87b6c78359c2bbb13c3ef622e1c90b3b3fbb52f4943f5`. |
| `seed-01.qwertycoin.org` | Not inspected: SSH host-key revalidation remains pending after reinstall. |
| `seed-02.qwertycoin.org` | Docker-only inspection found no QWC node container. |
| `seed-03.qwertycoin.org` | Docker-only inspection found no QWC node container. |

The deployed source suffix `d5e6cf7de` is not resolvable in the reviewed local
or public repository history. Therefore the running binary is not presently
reproducible from the public baseline. This is a release blocker, not proof that
the binary is malicious or invalid.

Operational review also found a sensitive reward-view credential supplied in a
container command line, where host-level inspection can reveal it. The value is
intentionally omitted. Existing on-chain disclosure cannot be revoked; affected
operators need a dedicated-wallet migration plan. Production credentials and
containers were not changed.

## 7. Test inventory and status

The source contains 76 focused `TEST(epose, ...)` unit tests in
`tests/unit_tests/epose.cpp`, an EPoSE fuzz target and corpus, an admission
benchmark, a Sybil simulation, and a local multi-node integration harness.
Current coverage includes bounded parsing, signatures, admission binding,
dynamic quorum, reward arithmetic and one-time outputs, deterministic
selection, atomic transaction batches, relay duplicates, state hashing,
rollback, and A -> B -> A restoration.

The current suite does **not** prove subject-authenticated service, frozen
pre-seed membership, a dedicated versioned envelope, bounded supported
population, lifecycle/recovery, scoped payment proofs without a private view
key, persistent-index crash recovery, or matured reward spend/receipt on the
exact release binary.

Commands and fresh results for this branch are recorded in `VALIDATION.md` and
the pull request. Existing known failures outside EPoSE are not to be hidden or
relabeled as EPoSE successes.

## 8. Baseline conclusion

The public code and the observed deployment are not yet one reproducible release
identity. The observed HF17 chain is disposable and is not a migration source
for the intended public genesis. EPoSE v1 has valuable deterministic and
funds-separation foundations,
but its automatic positive attestation does not prove service. The hardened
economic protocol is therefore **not approved for activation or public
production claims**. Work proceeds in the CO-00 through CO-11 order, with a
fresh-genesis version-18 target and separately reviewed consensus changes.
