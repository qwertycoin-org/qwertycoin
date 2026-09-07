# EPoSE Security Finding Disposition

**Source baseline:** `1c4c1bf10c387887a42243dc690a65abb6c6e786`

**Review input SHA-256:**
`98a846ad9539510e4146334be82b0da317483e500366936a7df979be626133cb`

This matrix maps F01 through F21 to the current public source. “Open” records a
missing guarantee or unresolved decision; it is not a claim that exploitation
has occurred. A finding closes only with code evidence and an executable
regression test, or with an approved ADR where the issue is a declared limit.

| ID | Status | Current evidence | Required disposition |
|---|---|---|---|
| F01 | **Authenticated data exchange implemented; network adapter open / P0** | The local HF17 fabricator is disabled. V2 now requires an authorized subject to retrieve, bound, parse and sign exact canonical block bytes; the verifier independently compares them with its canonical chain before signing. The public network adapter remains disabled until canonical v2 lifecycle/membership state can authorize it. | Connect the bounded endpoint transport only through canonical snapshot/lifecycle authorization; CO-10 must prove an offline subject cannot qualify with honest verifiers across real hosts. |
| F02 | **Corrected primitive; handler integration open / P0** | `membership_pipeline_v2` freezes canonically ordered descriptor commitments before selection and now ingests the complete authenticated receipt plus canonical context; no public Boolean/aggregate bypass remains. Exact QWC HF17 permits its v2 carrier, but the canonical block handler and persistence integration remain incomplete. | CO-04/05: connect authenticated records and bounded carriers; CO-08: persist/rebuild; launch remains blocked. |
| F03 | **Admission proof/context corrected; economics open / P0** | A typed v2 lease is accepted only after recomputing its RandomX work and canonical lease hash. It binds separate service/operator keys, identity, endpoint/reward/descriptor commitments and target epoch to the one permitted context `start(E-1)`. A fully recomputed proof on any other earlier context is rejected before state mutation; cutoff and snapshot rules remain enforced. | CO-03: calibrated recurring lease target, optimized grinding measurements and approved parameters; handler/persistence integration remains required. |
| F04 | **CO-03 quantified; decision open / P0** | The exact finite-population model compares fixed `k/q`, capture and honest liveness; CO-02 refuses small-network shrinkage. No parameter meets an approved risk budget yet. | Approve a numerical risk budget and combine CO-03 results with measured CO-04/05 cost/capacity before selecting constants. |
| F05 | **RandomX verifier implemented; hardware gate open / P0** | C++ no longer trusts a claimed hash prefix: it recomputes RandomX against the canonical context and exact bound lease fields. Admission sensitivity exposes solver rate and lease amortization without presenting illustrative rates as measurements. The required optimized x86-64/ARM64 and attacker matrix is incomplete. | Measure the required hardware classes, model recurring leases, and obtain owner approval before changing the target. |
| F06 | **Bounded carriers and connected local delivery implemented; load evidence open / P0** | Miner and fee-funded transactions use the same version-gated `tx_extra` adapter and envelope budgets. The typed P2P handler performs atomic semantic admission into a class-reserved bounded pool, and miners select deterministic deadline-ordered records under separate Enrollment/Evidence reservations. Live record construction and measured inclusion delay remain open. | Implement lifecycle/admission/receipt producers; prove capacity, inclusion delay, propagation and starvation resistance under sustained load. |
| F07 | **HF17/v2 carrier, codecs and canonical handler implemented; integrated adversarial evidence open / P0** | Dedicated canonical `0x05` / `QEP2` framing is a length-delimited `tx_extra` variant. Exact QWC block version 17 permits the hardened EPoSE-v2 envelope; inherited version 16, unscheduled version 18, and legacy-v1 objects do not. The canonical coordinator applies bounded records atomically, validates the scoped Coinbase proof and commits the resulting state with the block. | Complete differential/fuzz, malformed-block, multi-node and independent consensus evidence; launch remains blocked until every gate closes. |
| F08 | **CO-02 settlement primitive implemented; payout integration open / P0** | V1 epoch 0 remains unchanged. V2 timing enforces the CO-01 two-epoch warm-up, inclusive enrollment/evidence cutoffs, immutable snapshots, and one-time qualification close. No v2 payout path exists yet. | CO-06: consume only closed v2 qualification sets; CO-08: persistent rollback/replay; activation remains blocked. |
| F09 | **Partially quantified / P0** | Selection is deterministic and seed-bound, but a delayed block hash remains replaceable. CO-03 reports independent-attempt estimates and conservative union bounds without claiming an unbiased beacon. | Quantify QWC-specific withholding/reorg cost against reward value; rebuild all dependent state in CO-08. |
| F10 | **CO-06 alternatives implemented; decision open / P0** | V2 subsidy-only arithmetic models miner fallback and permanent non-issuance, but `unset` fails closed and no option is active. | Alex must approve the empty-set policy and emission/PoW-security consequences before activation. |
| F11 | **CO-06 scoped proof and codec implemented; review open / P0** | V2 binds prescribed one-time outputs using public reward keys, a recipient-specific derivation, and the inherited transaction-proof primitive without a private view key. Its 96-byte record exposes only the derivation/proof and verifies against the reconstructed Coinbase context. The disposable HF17 chain is not migrated. | Independent cryptographic review plus CO-07 fresh dedicated-wallet lifecycle and custody guidance are required before activation. |
| F12 | **CO-06 proof unit coverage; E2E open / P0** | Scoped proof tests cover exact outputs, binding, tampering, and repeated-wallet uniqueness. No exact-release matured spend/receipt/rescan/reorg proof exists. | CO-10: worthless-wallet enrollment-to-recipient E2E on final protocol and binaries. |
| F13 | **Atomic semantic and persistent production path implemented; crash evidence open / P0** | Envelope parsing, transaction aggregation and the production coordinator commit only wholly valid ordered blocks. Schema- and parameter-bound state commitments share the LMDB write transaction with canonical block connect/disconnect; startup and deep replay verify every commitment before replacing live state. | CO-08/10: prove process-crash recovery, pruning, canonical deep reorg and independent fresh-replay equivalence. |
| F14 | **Structural and local queue bounds implemented; measured limits open / P0** | Envelope/block accumulators bound bytes, records, signatures and admission operations with fail-atomic charging. Relay queues now have total and per-class reserved item/byte bounds with deterministic expiry and selection. Manifest values remain unset; worst-case selection/probe costs are not yet measured. | CO-05/09: worst-valid-block and sustained-stream benchmarks, final manifest values, cached selection, and supported-population contract. |
| F15 | **CO-09 policy primitives implemented; handler gate open / P1** | Signed typed descriptors, canonical hosts, public-address/DNS rechecks, and probe byte/time/concurrency limits now exist as non-activating primitives. | Integrate descriptor discovery/socket checks with no redirects, streaming limits, credential isolation, and SSRF/amplification fixtures. |
| F16 | **Corrected state machine and codec; integration open / P1** | Historical lookup applies the selected historical record rather than the latest record. Deregistration is terminal; later ordinary renewal cannot rewrite E5. Online service and offline operator keys must be distinct. Registration and later actions have a shared 378-byte dual-authorized codec with type/action separation. | Derive effective epochs from canonical inclusion height; finalize custody UX, semantic batch integration, and durable rollback. |
| F17 | **LMDB commitment/replay integration implemented; operational gate open / P1** | Each canonical HF17 block stores a schema- and parameter-bound EPoSE state commitment in the same LMDB transaction. Disconnect removes it atomically, bounded undo is explicit, and startup/deep replay streams canonical blocks and rejects missing or mismatched commitments. | Add process-kill injection, pruning-mode proof, multi-node restart/deep-reorg convergence and final schema review before launch. |
| F18 | **Canonical block exchange implemented; transport measurement open / P1** | Subject and verifier now validate bounded canonical block bytes rather than accepting an echoed hash. The on-chain receipt still carries only the signed digest, so it remains a service claim and cannot prove unique storage or defeat a shared proxy. | Connect and measure the bounded live transport; document proxy/shared-backend results without overclaiming. |
| F19 | **CO-09 local bounds implemented; network test open / P1** | Deterministic simulations expose outages; probe resources are now bounded and local failures cannot enter block validation. Predictable schedules still permit targeted DoS. | CO-10: adaptive outage/partition and payout-after-disappearance tests. |
| F20 | **Fresh-genesis profile fixed; release identity open / P0** | The typed schema fixes `fresh-genesis` activation at height 0 and rejects Boolean integers, invalid relationships, and test fixtures for release use. A separate immutable 13-ID policy requires candidate-bound artifact hashes. The disposable HF17 deployment is not a public-chain migration source. | CO-11: finalize and bind the new genesis, source revision, parameter digest, reproducible binaries, public specification, and independent review before activation. |
| F21 | **CO-03 duty cost modeled; incentive unresolved / P0** | Mean outgoing checks and withholding liveness are now explicit. Missing evidence still cannot identify a lazy verifier versus subject/network failure, and no safe incentive mechanism is established. | CO-04: measure final probe cost and behavior; do not add per-vote rewards or penalties without a separate equilibrium analysis. |

## Existing safeguards to preserve

- RandomX remains the only chain-selection and block-production mechanism.
- EPoSE validation uses chain data and performs no live network I/O.
- Service identity keys are separate from wallet spend keys.
- Admission and attestation signatures are network-bound.
- Parser sizes and relay-pool entries are bounded at their current interfaces.
- Duplicate verifier votes do not add qualification weight.
- Transaction-batch application is atomic in memory.
- Current-epoch attestations do not alter the documented prior reward source.
- Reorg snapshot and A -> B -> A tests exist.
- Service reward validation checks exact decomposed one-time outputs.

These properties are necessary but do not close the P0 findings above.

## Ordered implementation ownership

| Change order | Findings | Gate |
|---|---|---|
| CO-00 | F20 and mapping of all findings | This document set and reproduced baseline evidence. |
| CO-01 | F08, F13, F20 | Normative fresh-genesis activation and transition specification. |
| CO-02 | F02, F03, F08, F09 | Frozen future-epoch admission/selection pipeline. |
| CO-03 | F04, F05, F09, F19, F21 | Measured security/economic parameter report; owner risk approval. |
| CO-04 | F01, F07, F18, F19 | Authenticated subject service receipts. |
| CO-05 | F06, F07, F14 | Versioned bounded envelope and viable carriers. |
| CO-06 | F10, F11, F12 | Separately approved reward economics and reviewed payment proof. |
| CO-07 | F11, F15, F16 | Lifecycle, rotation, and credential recovery. |
| CO-08 | F13, F14, F17 | Atomic persistent index and replay equivalence. |
| CO-09 | F14, F15, F19 | Probe/RPC/relay/cryptographic resource bounds. |
| CO-10 | F02-F19 | Funds-safety and convergence E2E on final parameters. |
| CO-11 | All P0 | Reproducible release and independent adversarial review. |

No later package may silently relax an earlier invariant to make a test pass.
Consensus, cryptography, wallet, storage, and operational changes should remain
separate reviewable commits and, where practical, separate pull requests.

CO-10/11 assessment is recorded in `RELEASE_READINESS.md`,
`ADVERSARIAL_ACCEPTANCE_MATRIX.md`, and the machine-readable
`RELEASE_GATES_V2.json`. The current result is a deliberate no-go: component
evidence from CO-02 through CO-09 does not close a finding until the final HF17/v2
handler, persistent state, network path, reward/wallet path, and release build
traverse it end to end.
