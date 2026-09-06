# QWC HF17 / EPoSE-v2 version mapping audit

**Status:** Fresh-genesis launch mapping; release readiness remains gated  
**Inherited baseline:** Monero consensus rules through block version 16  
**QWC launch block version:** 17  
**EPoSE protocol/format version:** 2  

## Version domains

Block-major version and EPoSE record version are independent domains. The new
Qwertycoin chain begins at exact block major version 17. That version inherits
the retained Monero-HF16 transaction, RingCT, Coinbase, and cryptographic rules
and adds the QWC launch rules. EPoSE records within those blocks use protocol,
envelope, and record version 2.

There is no block-version-16 operating phase, no version-17 legacy EPoSE phase,
and no scheduled version-18 transition. Blocks with major version 16 or 18 do
not select an EPoSE handler on the fresh chain.

## Source mapping

| Entry point | Launch rule |
|---|---|
| `src/cryptonote_config.h` | `HF_VERSION_MONERO_CURRENT_CONSENSUS = 16` names the inherited baseline; `HF_VERSION_QWC_EPOSE = 17` names the only launch block version. |
| `src/hardforks/hardforks.cpp` | Mainnet, testnet, and stagenet schedule exact version 17 at height 0. |
| `src/cryptonote_core/cryptonote_tx_utils.h` | Genesis construction defaults to block major/minor version 17. |
| `src/epose/envelope_v2.cpp` | The dedicated EPoSE-v2 extra field is accepted only for exact block version 17. Versions 16, 18, and other unscheduled values fail closed. |
| `tests/epose/manifest_v2.py` | A launch candidate requires integer activation height 0, block hardfork version 17, and EPoSE protocol version 2. |

## Retired mapping

The prior pair `HF_VERSION_QWC_EPOSE_V1 = 17` and
`HF_VERSION_QWC_EPOSE_V2 = 18` is retired. Aliasing both names to 17 is
forbidden because it permits conflicting legacy and hardened dispatch. Source
and tests use one block-version constant and retain `_v2` names only for the
separate EPoSE protocol/format generation.

The legacy-v1 state machine remains historical source/test material only. The
production `Blockchain` block, reward, Coinbase, LMDB commitment, disconnect
and startup/deep-replay paths select the hardened coordinator at exact HF17.
The inherited service-node CLI, extra-nonce template producer and fixed-size
P2P registration/attestation command fail closed and cannot supply eligibility,
receipts, qualification, or payouts for the fresh-genesis candidate.

## Required launch evidence

This mapping is necessary but not sufficient for release. Candidate-bound
evidence must still prove the integrated transition under multi-node,
process-crash, partition and reorg workloads; bounded typed-v2 network
transport; wallet-funded record construction/scanning and matured spend;
pinned distinct network genesis hashes; and reproducible supported-platform
binaries.
