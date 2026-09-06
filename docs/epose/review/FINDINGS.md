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
| F01 | **CO-04 receipt core implemented; transport open / P0** | The local HF17 fabricator is disabled. V2 requires subject and verifier signatures over a temporally/context-bound canonical-object receipt before CO-02 accepts a slot. No live bounded probe transport is connected yet. | CO-05/07: add bounded carrier and frozen endpoint transport; CO-10: prove an offline subject cannot qualify with honest verifiers across real hosts. |
| F02 | **CO-02 primitive implemented; integration open / P0** | `membership_pipeline_v2` freezes canonically ordered opaque descriptor commitments at the committee anchor and all receipt/qualification membership reads that snapshot. It remains unreachable from HF17 and awaits CO-03/04/05 record validation and CO-08 persistence. | CO-04/05: connect authenticated records and bounded carriers; CO-08: persist/rebuild; activation remains blocked. |
| F03 | **CO-02 cutoff implemented; economics open / P0** | A v2 lease names a future target epoch and is rejected after `enrollment_cutoff(E)`, before the anchor is known. Selection binds network, genesis, parameter set, snapshot, epoch, round, anchor, subject, and verifier. Admission proof economics/context remain CO-03. | CO-03: proof transcript, calibrated recurring leases, optimized grinding simulation and approved parameters. |
| F04 | **CO-03 quantified; decision open / P0** | The exact finite-population model compares fixed `k/q`, capture and honest liveness; CO-02 refuses small-network shrinkage. No parameter meets an approved risk budget yet. | Approve a numerical risk budget and combine CO-03 results with measured CO-04/05 cost/capacity before selecting constants. |
| F05 | **CO-03 model implemented; hardware gate open / P0** | Admission sensitivity now exposes solver rate and lease amortization without presenting illustrative rates as measurements. The required optimized x86-64/ARM64 and attacker matrix is incomplete. | Measure the required hardware classes, model recurring leases, and obtain owner approval before changing the target. |
| F06 | **Confirmed / P0** | `build_epose_miner_extra_nonce()` returns after one local/relayed registration or one attestation. Registration has priority. | CO-05: bounded batches and fee-funded carrier; prove capacity, fairness, inclusion delay, and starvation resistance. |
| F07 | **Confirmed / P0** | Current attestation is 234 bytes in the 255-byte legacy nonce. A 64-byte subject signature cannot fit without redesign. | CO-01/05: dedicated versioned, length-delimited EPoSE envelope; do not enlarge legacy nonce ad hoc. |
| F08 | **CO-02 settlement primitive implemented; payout integration open / P0** | V1 epoch 0 remains unchanged. V2 timing enforces the CO-01 two-epoch warm-up, inclusive enrollment/evidence cutoffs, immutable snapshots, and one-time qualification close. No v2 payout path exists yet. | CO-06: consume only closed v2 qualification sets; CO-08: persistent rollback/replay; activation remains blocked. |
| F09 | **Partially quantified / P0** | Selection is deterministic and seed-bound, but a delayed block hash remains replaceable. CO-03 reports independent-attempt estimates and conservative union bounds without claiming an unbiased beacon. | Quantify QWC-specific withholding/reorg cost against reward value; rebuild all dependent state in CO-08. |
| F10 | **Confirmed decision / P0** | With no selected payee, `get_epose_service_reward_for_block()` returns no service allocation; miner may claim the full permitted reward. | CO-06 ADR: compare miner fallback, permanent non-issuance, emission continuity, and PoW-security effects. No change without approval. |
| F11 | **Confirmed / P0** | `service_node_identity` serializes `reward_view_secret_key`; validation derives outputs using it. | CO-06/07: isolate legacy reward wallets and independently review scoped payment verification for new registrations. Never publish spend keys. |
| F12 | **Partially covered / P0** | Unit tests validate output construction, uniqueness, amounts, recipient, and view key. No exact-release matured spend/receipt/rescan/reorg proof exists. | CO-10: worthless-wallet enrollment-to-recipient E2E on final protocol and binaries. |
| F13 | **Transition specified; storage implementation open / P0** | Transaction batches are atomic and state hash sorting is deterministic. CO-01 now defines canonical block/transaction/field/record order, atomic transaction semantics, duplicate/conflict behavior, reward read-state, checked arithmetic, and a reference model. | CO-02/05/08: implement and cross-check the transition, persistent index, crash recovery, and replay equivalence. |
| F14 | **Confirmed / P0** | Relay pool is bounded, but no dedicated per-block byte, signature, RandomX, object-count, or population/work budget was located. Selection can scale quadratically. | CO-05/09: consensus budgets, worst-valid-block benchmarks, cached selection, supported-population contract. |
| F15 | **Confirmed / P1** | Registration stores only an endpoint commitment. No authenticated descriptor discovery or bounded SSRF-safe probing implementation was located. | CO-04/07/09: signed canonical descriptors, connection-time address checks, no redirects, bounded concurrency/bytes/time. |
| F16 | **Confirmed / P1** | Registry supports registration/expiry and rejects overlapping key/endpoint entries; renewal, update, deregistration, rotation, and recovery semantics are absent. | CO-07: sequence-bound future-effective lifecycle with independent payout authorization. |
| F17 | **Confirmed / P1** | EPoSE state is in-memory; recent full snapshots are retained and deeper rollback replays from height 0. No dedicated LMDB schema exists. | CO-08: atomic rebuildable versioned index, bounded undo, crash/migration/deep-reorg tests, replay oracle. |
| F18 | **CO-04 service kind defined; transport open / P1** | V2 defines retrieval of a requested canonical object and a dual-signed digest. The chain does not see returned bytes, so this remains a signed service claim and cannot prove unique archival storage or defeat a shared proxy. | Implement and measure the bounded live retrieval path; document proxy/shared-backend results without overclaiming. |
| F19 | **CO-03 correlation model added; network test open / P1** | Deterministic simulations expose withholding and shared-operator outages, but no adaptive targeted-DoS or payout-after-outage network experiment exists. | CO-04/10: adversarial outage/partition tests; never add live probing to block validation. |
| F20 | **Activation contract specified; deployment reconciliation open / P0** | Public `main`, the running `d5e6cf7de` binary, observed live HF17 chain, and release identity remain unreconciled. CO-01 reserves HF18/v2/tag `0x05`, requires an epoch-aligned future height, and makes incomplete manifests non-activatable. | CO-11: source-to-binary proof, final signed activation manifest, public specification, and exact deployment reconciliation before activation. |
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
| CO-01 | F08, F13, F20 | Normative transition and future activation specification. |
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
