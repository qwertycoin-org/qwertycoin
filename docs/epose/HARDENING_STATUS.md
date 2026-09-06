# Qwertycoin EPoSE Hardening Status

Status values: `DONE`, `PARTIAL`, `OPEN`, `ACCEPTED RISK`.

This file describes current `main` at `60ccc9d08`.

| Area | Status | Current state |
| --- | --- | --- |
| HF17 from genesis | `DONE` | QWC mainnet, testnet, and stagenet start at `HF_VERSION_QWC_EPOSE = 17` from height 0; launch EPoSE records use format version 2. |
| QWC baseline | `DONE` | 8 decimals, 120s v2 target, RandomX, QWC address prefix `0x14820c`, ports `8196/8197/8198/8199`, supply `184,467,440.73709551 QWC`, final subsidy `0.3 QWC/min`. |
| EPoSE chain-state derivation | `PARTIAL` | State is deterministically rebuilt from canonical chain registrations and attestations. Dedicated LMDB EPoSE indexes are not implemented. |
| Registration | `PARTIAL` | Signed tx-extra registration includes service key, reward spend key, disclosed reward view secret key, endpoint commitment, epoch, expiry, nonce, admission hash, and signature. Lifecycle is currently register/active/expire with duplicate guards; explicit renewal/update/deregister semantics remain open. |
| Reward view key validation | `DONE` | Registration parsing derives the public view key from the disclosed private view key and rejects mismatches. The spend key remains secret. |
| Admission proof | `PARTIAL` | Admission is RandomX-bound and tied to network, service key, reward address/view key, endpoint, epoch, previous finalized epoch hash, and nonce. The current hardened target is `EPOSE_ADMISSION_LEADING_ZERO_BITS = 16`, selected from bounded seed host A benchmarks because 18/20/22/24 bits are not practical with the current RandomX light-mode admission solver. |
| Committee selection | `PARTIAL` | Deterministic verifier selection excludes the subject, sorts by epoch-seeded score, and targets committee size 9. Corrected Sybil simulations show this substantially reduces controlled-threshold exposure compared with the old 5/2 rule, while still preserving small-network bootstrap behavior through actual-committee sizing. |
| Qualification threshold | `PARTIAL` | Unique valid attestations qualify nodes. The fixed `EPOSE_MIN_ATTESTATIONS = 2` rule was removed and replaced with `ceil(actual_committee_size * 2 / 3)`, where the actual committee is selected per subject from the active node set. |
| Attestation | `PARTIAL` | Signed, network-bound, subject/verifier-bound, epoch-bound, challenge-bound, observed-tip-bound attestations exist. Self-attestations, duplicate votes, wrong challenge/response, wrong epoch, and unselected verifiers are rejected. Automatic miner attestations use the same consensus response-hash helper as validation. Subject-authenticated live service proof remains a future hardening item. |
| Attestation relay | `PARTIAL` | `NOTIFY_NEW_EPOSE_PAYLOADS` relays bounded registration/attestation blobs. Normal nodes validate, deduplicate, cache, and may include accepted payloads in block templates. The pool is not consensus state. |
| Mining / service-node decoupling | `DONE` | A service node does not need local mining to register or qualify, and a miner does not need a service-node key to include relayed EPoSE payloads. |
| Reward source epoch | `DONE` | Blocks in epoch `E + 1` pay from the finalized qualified set of epoch `E`, preventing current-block/current-epoch payload ordering from changing the current payee. |
| Service reward output | `DONE` | Service rewards are normal one-time CryptoNote outputs derived from the coinbase tx public key and registered reward address/view key. The service amount is decomposed into standard denominations for spendability. |
| Coinbase validation | `PARTIAL` | Validation recomputes expected derived output keys and requires the sorted matching amounts to equal the service reward denomination set. Missing, wrong, duplicate-output-key, overpay, and underpay cases have focused coverage; broader block-level malformed-output fuzzing should continue. |
| Wallet detection/spendability | `PARTIAL` | Live controlled testing showed reward detection in the web wallet. The denominated-output fix addresses the observed coin-selection failure. A final spend-to-recipient gate should remain part of release validation. |
| Service-node identity storage | `DONE` | Mainnet deployment uses `/service-node/service-node.key` and a separate identity volume so chain-volume resets do not change service identity when the identity volume is preserved. |
| Duplicate registrations | `PARTIAL` | Active overlapping registrations are rejected for duplicate service public keys and duplicate endpoint commitments. Endpoint uniqueness is an operational duplicate guard, not a complete Sybil model. Renewal/update semantics remain open. |
| Docker deployment | `PARTIAL` | Mainnet deployment now has one shared compose file plus per-host env files and a Docker-only helper. Compose plugin availability on hosts is not guaranteed, so the helper remains operationally important. |
| RPC / explorer semantics | `PARTIAL` | EPoSE status, node list, reward, and registration payload RPCs exist. Explorer integration is external and observer-only; it should group denominated service reward outputs only as UI presentation. |
| Tests | `PARTIAL` | Current EPoSE unit tests and fuzz corpus passed during PR #8 validation. This hardening branch adds focused dynamic quorum, bounded admission benchmark, and committee Sybil simulation coverage. Larger multi-node, long fuzz, sanitizer breadth, reorg/partition depth, and final reward spend gates remain open. |
| Tokenomics | `OPEN` | Current code splits `base_reward + transaction_fees` 90/10. Final public mainnet must explicitly choose fee-sharing versus subsidy-only rewards. |
| Stronger reward privacy | `OPEN` | Governance-style one-time outputs are wallet-compatible, but the disclosed view key makes the dedicated reward wallet observable. Stronger privacy needs a separate reviewed design. |

## Next Hardening Work

1. Run a fresh post-reset live validation from genesis because the admission target and quorum rule are consensus-relevant.
2. Continue long-running fuzz, sanitizer breadth, and deeper reorg/partition validation.
3. Benchmark future optimized admission solvers and recalibrate the target if identities become too cheap.
4. Keep `EPOSE_SERVICE_REWARD_BPS = 1000` unchanged in that hardening PR.
5. Keep final tokenomics, production deployment automation, and stronger reward privacy as explicit mainnet gates.
