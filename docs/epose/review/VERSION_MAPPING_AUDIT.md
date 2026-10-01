# QWC HF17/HF18 / EPoSE-v2 version mapping audit

**Status:** Fresh-genesis launch mapping; release readiness remains gated  
**Inherited baseline:** Monero consensus rules through block version 16  
**QWC launch block version:** 17  
**Relay-hardening block version:** 18 from height 20,000
**EPoSE protocol/format version:** 2  

## Version domains

Block-major version and EPoSE record version are independent domains. The new
Qwertycoin chain begins at exact block major version 17. That version inherits
the retained Monero-HF16 transaction, RingCT, Coinbase, and cryptographic rules
and adds the QWC launch rules. EPoSE records within those blocks use protocol,
envelope, and record version 2.

There is no block-version-16 operating phase and no version-17 legacy EPoSE
phase. HF18 is a forward continuation of the same EPoSE-v2 state machine at
height 20,000; it does not select a new EPoSE wire format or reset state.

## Source mapping

| Entry point | Launch rule |
|---|---|
| `src/cryptonote_config.h` | Version 17 names the launch rules; version 18 names relay hardening and keeps EPoSE-v2 active. |
| `src/hardforks/hardforks.cpp` | Mainnet, testnet, and stagenet schedule version 17 at height 0 and version 18 at height 20,000. |
| `src/cryptonote_core/cryptonote_tx_utils.h` | Genesis construction defaults to block major/minor version 17. |
| `src/epose/envelope_v2.cpp` | The dedicated EPoSE-v2 field is accepted for scheduled QWC versions 17 and later; chain hardfork validation rejects unscheduled block versions. |
| `src/epose/relay_pool_v2.cpp` | Receipt-slot deduplication and semantic variant purging activate exactly at height 20,000. |
| `tests/epose/manifest_v2.py` | A launch candidate requires integer activation height 0, block hardfork version 17, and EPoSE protocol version 2. |

## Retired mapping

The prior pair `HF_VERSION_QWC_EPOSE_V1 = 17` and
`HF_VERSION_QWC_EPOSE_V2 = 18` is retired. Aliasing both names to 17 is
forbidden because it permits conflicting legacy and hardened dispatch. Source
and tests use one block-version constant and retain `_v2` names only for the
separate EPoSE protocol/format generation.

The legacy-v1 state machine remains historical source/test material only. The
production `Blockchain` block, reward, Coinbase, LMDB commitment, disconnect
and startup/deep-replay paths select the hardened coordinator from HF17 onward.
The inherited service-node CLI, extra-nonce template producer and fixed-size
P2P registration/attestation command fail closed and cannot supply eligibility,
receipts, qualification, or payouts for the fresh-genesis candidate.
Daemon observer RPCs read only the canonical v2 coordinator state. The retained
v1 registration-payload and reward-preview endpoint names do not call their
legacy builders: they report the corresponding typed-v2 capability unavailable
until the producer and shared reward-plan RPC contracts are implemented.

## Required launch evidence

This mapping is necessary but not sufficient for release. Candidate-bound
evidence must still prove the integrated transition under multi-node,
process-crash, partition and reorg workloads; bounded typed-v2 network
transport; wallet-funded record construction/scanning and matured spend;
pinned distinct network genesis hashes; and reproducible supported-platform
binaries.
