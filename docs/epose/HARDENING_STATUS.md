# Qwertycoin EPoSE Hardening Status

Status values: `DONE`, `PARTIAL`, `OPEN`, `ACCEPTED RISK`.

This file describes the consolidated PR #184 candidate. Public launch remains
blocked by the evidence ledger and incomplete parameter manifest.

| Area | Status | Current state |
| --- | --- | --- |
| HF17 from genesis | `DONE` | QWC mainnet, testnet, and stagenet start at `HF_VERSION_QWC_EPOSE = 17` from height 0; launch EPoSE records use format version 2. |
| QWC baseline | `DONE` | 8 decimals, 120s v2 target, RandomX, QWC address prefix `0x14820c`, ports `8196/8197/8198/8199`, supply `184,467,440.73709551 QWC`, final subsidy `0.3 QWC/min`. |
| EPoSE chain-state derivation | `PARTIAL` | HF17 block acceptance uses the hardened v2 coordinator. State commitments are written/removed atomically with LMDB block connect/disconnect; restart and deep replay reconstruct every transition from canonical blocks and compare each commitment. Final pruning and multi-node crash evidence remain open. |
| Registration | `PARTIAL` | Hardened v2 lifecycle records bind a stable identity, separate offline operator authority, online service key, public reward address, endpoint descriptor, sequence and effective/expiry epochs. Register, renew, update, deregister and recovery transitions are implemented; daemon/wallet production of those records remains open. |
| Reward privacy | `PARTIAL` | Hardened v2 descriptors contain only public reward keys and scoped Coinbase proofs; no private reward view key is required. The legacy CLI path that requested a private reward view key now fails closed, but replacement v2 configuration/RPC surfaces remain open. |
| Admission proof | `PARTIAL` | V2 admission is real RandomX work bound to network, genesis, parameter set, stable identity, complete authorized descriptor, exact target epoch and the canonical genesis/previous-epoch context. Wrong contexts and lifecycle bindings fail before RandomX. Difficulty and lease duration remain unset launch parameters. |
| Committee selection | `PARTIAL` | V2 selection excludes the subject and deterministically ranks only the immutable frozen snapshot, binding network, genesis, parameter set, snapshot, epoch, round and anchor. Committee size is fixed and never shrinks for a small network; the final size remains unset. |
| Qualification threshold | `PARTIAL` | V2 qualification counts unique authenticated receipt slots over the configured rounds and a fixed threshold. It does not dynamically weaken the economic quorum; final threshold, rounds and rounds-required remain unset. |
| Attestation | `PARTIAL` | V2 receipts bind a canonical service request/response to subject and selected-verifier signatures, immutable snapshot/endpoint data, nonce, epoch, round and anchor. Consensus performs no network I/O; the live probe producer and canonical service transport remain launch work. |
| Attestation relay | `OPEN` | The inherited v1 `NOTIFY_NEW_EPOSE_PAYLOADS` command and extra-nonce producer are rejected/disabled under HF17. Bounded v2 queue primitives exist, but typed-envelope P2P ingestion, storage and fee-funded/template production are not yet connected. |
| Mining / service-node decoupling | `PARTIAL` | Consensus and Coinbase construction require no service-node secret at the miner. Practical decoupling still requires the v2 relay/wallet producer so independently funded lifecycle, admission and receipt records reach arbitrary miners. |
| Reward source epoch | `DONE` | Blocks in epoch `E + 1` pay from the finalized qualified set of epoch `E`, preventing current-block/current-epoch payload ordering from changing the current payee. |
| Service reward output | `PARTIAL` | The v2 miner constructor derives normal one-time outputs from public reward keys and emits a scoped proof using the actual Coinbase transaction secret. Wallet maturity/spend/rescan evidence remains open. |
| Coinbase validation | `PARTIAL` | The coordinator derives the settled payee and allocation, requires the exact Coinbase total, reconstructs service outputs from the actual transaction, verifies one scoped proof, and commits state only afterward. Integrated malformed-block and multi-node evidence remains open. |
| Wallet detection/spendability | `OPEN` | Standard one-time v2 reward outputs and tolerant wallet metadata parsing are implemented. Final-binary maturity, spend-to-recipient, rescan, repeated-output uniqueness and payout-fork replacement have not been demonstrated. |
| Service-node identity storage | `OPEN` | The v2 protocol separates offline operator and online service authorities, but the production keystore, backup and recovery workflow has not been implemented or reviewed. Legacy key storage is not accepted as v2 evidence. |
| Duplicate registrations | `DONE` | Lifecycle, admission and freeze independently enforce at most one stable identity and one service key per target epoch; authorized higher-sequence replacement and deregistration semantics are regression-tested. |
| Docker deployment | `PARTIAL` | Mainnet deployment now has one shared compose file plus per-host env files and a Docker-only helper. Compose plugin availability on hosts is not guaranteed, so the helper remains operationally important. |
| RPC / explorer semantics | `OPEN` | Existing endpoints expose legacy-v1 shapes and return no hardened state after legacy dispatch retirement. Pagination/resource primitives exist; v2 status, lifecycle construction, queue metrics and observer schemas still need production integration. |
| Tests | `PARTIAL` | The consolidated candidate has focused codec, cryptography, resource, coordinator, parser, LMDB commitment and replay regression coverage. Full multi-node, process-crash, sustained fuzz/sanitizer, partition/reorg, native macOS and wallet-spend evidence remains open. |
| Tokenomics | `OPEN` | Hardened v2 reserves 1,000 BPS of scheduled subsidy only and leaves fees with miners. The empty-qualified-set/emission policy remains deliberately unset and prevents public initialization. |
| Stronger reward privacy | `PARTIAL` | New v2 descriptors never disclose a private reward view key and scoped proofs reveal only the prescribed Coinbase payment. Independent proof review and wallet end-to-end evidence remain required. |

## Next Hardening Work

1. Connect typed v2 P2P/RPC ingestion, fee-funded record construction, queue storage and canonical service probing; keep every legacy-v1 producer fail-closed.
2. Add isolated multi-node genesis, restart, process-crash, partition/heal, deep-reorg and wallet maturity/spend/rescan evidence.
3. Complete the optimized multi-hardware admission, committee, throughput and worst-case verification study before filling manifest limits.
4. Obtain explicit empty-set/emission policy approval and independent review of the scoped payment proof.
5. Prove reproducible Linux/Docker and native macOS arm64 binaries, then bind final genesis, source revision and artifact hashes into the launch manifest.
