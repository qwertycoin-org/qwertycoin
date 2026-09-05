# EPoSE Documentation Inventory

Date: 2026-09-04
Branch source: `main` at `60ccc9d08`

## CURRENT

These files are intended to describe the protocol and deployment behavior of
the current `main` branch:

- `README.md`
- `PROTOCOL.md`
- `REWARDS.md`
- `SERVICE_NODE.md`
- `CONSENSUS_INVARIANTS.md`
- `THREAT_MODEL.md`
- `HARDENING_STATUS.md`
- `IMPLEMENTATION_REPORT.md`
- `MAINNET_VALIDATION.md`
- `ATTESTATION_RELAY.md`
- `HISTORICAL_GOVERNANCE_REWARD_ANALYSIS.md`
- `SERVICE_REWARD_OUTPUT_ANALYSIS.md`
- `SERVICE_REWARD_PRIVACY.md`
- `DUPLICATE_REGISTRATION_ANALYSIS.md`
- `DNS_RECORDS.md`
- `BENCHMARKS.md`
- `MACOS_M1_TESTING.md`

## HISTORICAL SNAPSHOT

These files are retained for audit history and older PR context. They should
not be used as current protocol reference without checking `README.md`,
`PROTOCOL.md`, and the current source code first:

- `PR2_OVERVIEW_DE.md`
- `QWC_V2_CHANGE_REPORT_DE.md`
- `MAINNET_TESTPHASE.md`
- `TESTNET.md`
- `CONSOLIDATION_RFC.md`

## CURRENT OUTSIDE `docs/epose`

- `../architecture/CURRENT_STATE.md`
- `../../README.md`
- `../../deploy/mainnet/README.md`

## Current Code Source Of Truth Checked

- `src/epose/service_node.h`
- `src/epose/service_node.cpp`
- `src/epose/service_epoch.h`
- `src/epose/service_epoch.cpp`
- `src/epose/attestation_pool.h`
- `src/epose/attestation_pool.cpp`
- `src/epose/service_registry.*`
- `src/cryptonote_core/cryptonote_core.*`
- `src/cryptonote_core/blockchain.*`
- `src/cryptonote_core/cryptonote_tx_utils.cpp`
- `src/cryptonote_config.h`
- `tests/unit_tests/epose.cpp`
- `tests/epose/integration/local_epose_network.sh`
- `deploy/mainnet/*`

## Current Parameter Snapshot

| Parameter | Current `main` value |
| --- | --- |
| EPoSE protocol version | `1` |
| EPoSE hardfork | `HF_VERSION_QWC_EPOSE_V1 = 17` from genesis |
| Epoch length | `720` blocks |
| Finality depth | `60` blocks |
| Registration TTL | `30` epochs |
| State retention | `2` epochs |
| Verifier committee target | `5` |
| Qualification threshold | `EPOSE_MIN_ATTESTATIONS = 2` |
| Service reward | `1000 bps` / 10% |
| Admission difficulty | `8` leading zero bits |
| Identity blob | `249` bytes |
| Attestation blob | `234` bytes |
| Relay batch limit | `32` payloads |
| Relay pool limit | `4096` entries |
| QWC display decimals | `8` |
| P2P / daemon RPC / restricted RPC / ZMQ | `8196` / `8197` / `8198` / `8199` |
| Mainnet standard address prefix | `0x14820c` |

## Known Pending Hardening

- Replace fixed `EPOSE_MIN_ATTESTATIONS = 2` with a dynamic 2/3 quorum after
  simulation and tests.
- Evaluate committee target `5/7/9/11`; `9` is the leading candidate but is not
  implemented on `main`.
- Benchmark and raise RandomX admission difficulty; `8` bits is the current
  bootstrap value, not a final long-term anti-Sybil setting.
- Decide final tokenomics for fee sharing versus subsidy-only service rewards.
- Add longer fuzzing, larger multi-node runs, deeper reorg/partition coverage,
  and clean broader sanitizer coverage.
