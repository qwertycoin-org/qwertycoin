# CO-00 Baseline Validation

**Date:** 2026-09-06

**Source commit:** `1c4c1bf10c387887a42243dc690a65abb6c6e786`

**Change type:** documentation only

## Environment

```text
Linux x86_64
CMake 3.25.1
GCC/G++ 12.2.0
ccache 4.7.5
Boost 1.74.0
OpenSSL 3.0.20
Unix Makefiles
Release, shared libraries, tests enabled
```

The generated configuration used native architecture flags and the repository's
pinned submodules. Functional RPC tests were not configured because optional
Python modules `requests`, `psutil`, `monotonic`, `zmq`, and `deepdiff` were not
installed in this environment. No packages were installed for this baseline.

## Commands

The build directory was a newly created temporary directory outside the source
tree. Equivalent commands were:

```bash
cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DMANUAL_SUBMODULES=1 \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

cmake --build "$BUILD_DIR" \
  --target epose_unit_tests epose_admission_bench epose_sybil_sim daemon \
  --parallel 1

"$BUILD_DIR/tests/unit_tests/epose_unit_tests" --gtest_color=no
"$BUILD_DIR/bin/qwertycoind" --version
"$BUILD_DIR/tests/unit_tests/epose_sybil_sim" 100 20 64 0 9
"$BUILD_DIR/tests/unit_tests/epose_admission_bench" 8 1 1 5000
```

The runtime library search path pointed only at libraries produced in the same
temporary build directory.

## Results

| Check | Result |
|---|---|
| CMake configure | Passed |
| EPoSE library and focused test targets | Built successfully |
| Daemon target | Built and linked successfully |
| Daemon version | `QWC v2 Mainnet (v2.0.0-1c4c1bf10)` |
| Focused suite | **82/82 passed** in 83.165 seconds |
| EPoSE test case count | 76 |
| Sybil simulation smoke | Completed deterministically |
| Admission benchmark smoke | Solved and verified one 8-bit test identity |
| Markdown parse | Passed with Pandoc 2.17.1.1 |
| `git diff --check` | Passed |
| Secret-pattern scan of new documents | Passed |

The Sybil simulation used 100 identities, 20 controlled identities, 64 epochs,
committee target 9, and dynamic two-thirds quorum. It observed four captured
thresholds across 2,400 honest-subject trials in this deterministic run. This
single run is not a security probability estimate.

The 8-bit admission run is only a harness smoke test. A bounded 16-bit attempt
was stopped after exceeding the intended short validation window. Neither result
is a hardware benchmark or evidence that the current 16-bit/30-epoch admission
economics are safe. CO-03 requires optimized steady-state measurements across
multiple hardware classes and attacker configurations.

## Compiler observations

The successful build emitted inherited warnings, including OpenSSL 3 deprecated
API use, executable-stack notes from assembly objects, low-level C array-bound
warnings, and a constructor member-initialization warning in
`cryptonote_core.cpp`. They did not fail this build and are not resolved by a
documentation-only PR. They remain release-hardening inputs.

## Not run or not established

- full `unit_tests` suite;
- functional RPC tests requiring the missing Python modules;
- EPoSE fuzz corpus in this run;
- local multi-node integration, partition, or deep-reorg harness;
- wallet build and matured reward spend/recipient E2E;
- macOS Apple Silicon, Windows, sanitizer, or reproducible release builds;
- source-to-binary reproduction of the observed deployed `d5e6cf7de` build;
- production mutation, deployment, funds movement, or chain reset.

These gaps are explicit gates for later change orders. Passing this CO-00
baseline validates the inventory process, not economic production readiness.

## HF17 genesis adapter and commitment/replay validation

The consolidated candidate adds an exact-version HF17/EPoSE-v2 block adapter,
bounded recent undo, a same-transaction LMDB commitment index, and a streaming
replay verifier. The verifier rebuilds from canonical blocks and compares every
state root, block hash, parameter-set hash, and schema version with the stored
commitment. It never trusts a serialized semantic-state cache.

Commands reproduced on Linux x86-64 with GCC 12.2, CMake 3.25.1 and Boost
1.74.0:

```text
cmake --build <build-dir> --target epose_unit_tests unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_replay_v2.*:epose_block_transition_v2.*'
<build-dir>/tests/unit_tests/unit_tests \
  --gtest_filter='BlockchainDBTest/0.EPoSEStateCommitmentSharesCanonicalBlockTransaction'
```

Results:

- block-transition plus replay tests: **8/8 passed**;
- LMDB atomic add/restart/pop and invalid-commitment test: **1/1 passed**;
- full broad `unit_tests` target: built and linked successfully;
- exact QWC HF17 is accepted; inherited version 16 and unscheduled version 18
  fail closed in the v2 adapter;
- missing, wrong-state, wrong-parameter and wrong-schema commitments fail;
- LMDB accepts only commitment schema version 1.

This earlier evidence established the storage primitive. The later production
integration below retires legacy-v1 authorization from HF17 block acceptance,
connects the coordinator and canonical Coinbase context, and verifies stored
commitments during startup/deep replay. Public activation remains blocked by
the incomplete compiled parameter set.

## CO-01 design and vector validation

CO-01 adds documentation and an independent Python standard-library reference
model only. It does not add a C++ parser or change current consensus behavior.

Commands:

```text
jq empty docs/epose/PARAMETER_MANIFEST_V2.json
jq -S -c . docs/epose/PARAMETER_MANIFEST_V2.json | sha256sum
python3 -m py_compile tests/epose/reference_model_v2.py tests/epose/test_reference_model_v2.py
python3 tests/epose/test_reference_model_v2.py
python3 tests/epose/reference_model_v2.py tests/epose/vectors/co01_transition_v2.json
```

Results:

- reservation manifest parses as JSON;
- canonical reservation manifest SHA-256 is
  `6377b1eb1eca7b44165cf64f66b1d686ec657e99ca057a0041ba4990c83e8699`;
- reference-model unit tests: **13/13 passed**;
- activation, cutoff, anchor, deadline, payout, envelope, malformed-length,
  unknown-record, noncanonical-varint, and uint64-overflow vectors passed;
- the manifest test proves `fresh-genesis` activation is fixed at height 0
  while required release fields remain unset and status remains
  `not-activatable`.

The vector-only limits in `co01_transition_v2.json` test parser boundaries and
are not proposed mainnet constants. Final limits remain owned by CO-03/CO-05.

## CO-02 C++ membership-pipeline validation

CO-02 adds `membership_pipeline_v2` as an executable but non-activating C++
state-transition primitive. It is linked into the EPoSE library but has no call
site in HF17 block parsing, relay, template construction, reward validation, or
RPC.

Build and focused execution:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_v2.*'
<build-dir>/tests/unit_tests/epose_unit_tests
cmake --build <build-dir> --target daemon -j1
```

Reproduced on 2026-09-06 at this branch head: 10/10 `epose_v2` tests and
92/92 tests in the complete focused binary passed. The complete daemon target
also rebuilt successfully with the new EPoSE library object linked.

The focused cases cover checked timing arithmetic; inclusive cutoff/deadline;
invalid configuration and pre-service-epoch rejection; post-seed admission
rejection; canonical arrival-order-independent snapshots;
idempotent duplicates and conflicting records; snapshot-only subject/verifier
membership; fixed committee/threshold behavior; one-time qualification close;
and anchor replacement plus A -> B -> A replay equivalence.

These tests do not claim authenticated service evidence, calibrated committee
or admission parameters, a valid v2 wire encoding, persistent state, payout
safety, or activation readiness. Those remain assigned to later COs.

## CO-03 security-parameter model validation

Commands:

```text
python3 -m py_compile tests/epose/security_parameter_model.py tests/epose/test_security_parameter_model.py
PYTHONPATH=tests/epose python3 tests/epose/test_security_parameter_model.py
python3 tests/epose/security_parameter_model.py --output docs/epose/review/results/security_parameters_v1.json
jq -e '.status == "no_go_for_economic_activation"' docs/epose/review/results/security_parameters_v1.json
sha256sum docs/epose/review/results/security_parameters_v1.json
```

Results reproduced on 2026-09-06:

- security-model unit tests: **8/8 passed**;
- deterministic result SHA-256:
  `ccc672f70cf3f984331a01be70f69d08af52989168ae441838244f8d09d2835d`;
- exact finite-population capture, exact independent-seat liveness, grinding
  estimate/union bound, admission sensitivity, capacity, and verifier-duty
  matrices generated;
- deterministic correlated-operator simulations ran with 50,000 trials per
  scenario and store their RNG seeds and intervals in the result;
- result status remains `no_go_for_economic_activation` and lists every open
  hardware, risk-budget, capacity, concentration, and PoW-cost gate.

Illustrative hash rates in this model are not hardware measurements. No
optimized x86-64/ARM64 solver matrix, energy/rental-cost study, or owner-approved
numeric risk budget is claimed by this result.

The extended inherited-helper smoke benchmark was also built and run locally:

```text
cmake --build <build-dir> --target epose_admission_bench epose_sybil_sim -j1
<build-dir>/tests/unit_tests/epose_admission_bench 8 1 1 64 32
<build-dir>/tests/unit_tests/epose_sybil_sim 100 20 64 7 9
```

Environment: Debian 12 x86-64, 4 visible CPUs, AMD EPYC-Genoa Processor,
GCC 12.2.0, CMake 3.25.1. The admission smoke result reported a 746,460 us
first hash and 26.5961 H/s over 32 same-seed hashes. It is a short container
smoke test of the inherited helper, not an optimized solver or hardware-class
benchmark. The 64-epoch simulator reported zero controlled 7-of-9 thresholds;
that small zero-event result is not used as a probability claim and is
superseded by the exact finite-population calculation for parameter analysis.

## CO-04 authenticated-receipt validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_receipts_v2.*'
rg -n 'service_ok = true' src
```

Results reproduced on 2026-09-06:

- authenticated receipt tests: **8/8 passed**;
- complete focused EPoSE binary after integration: **100/100 passed**;
- daemon target rebuilt successfully after disabling the HF17 originator;
- a valid dual-signed receipt converts to a prevalidated CO-02 slot and is
  accepted by the frozen membership pipeline;
- verifier-only fabrication, wrong network/genesis/parameter set, changed
  epoch/round/snapshot/anchor/endpoint/nonce, role swap, self-vote, unsupported
  service kind, null context, wrong object, and signature tampering fail closed;
- no `service_ok = true` assignment remains under `src/`;
- cryptographic receipt validation contains no network or wall-clock call.

At that revision, the live bounded probe transport, v2 serialization/carrier,
endpoint descriptor discovery, and multi-host offline-subject test remained
open dependencies in CO-05/CO-07/CO-10. This evidence does not claim that dual signatures prove
operator independence or prevent a colluding committee/shared proxy.

### Canonical block service exchange

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_canonical_service_v2.*'
<build-dir>/tests/unit_tests/epose_unit_tests
```

Results reproduced on 2026-09-06:

- canonical service exchange tests: **5/5 passed**;
- complete focused EPoSE binary after integration: **159/159 passed**;
- the broad `unit_tests` target, including `wallet2.cpp`, built successfully;
- challenge authorization occurs before object lookup or signing;
- exact-limit canonical block bytes pass, while unknown, oversized, malformed,
  wrong-hash and locally divergent blocks fail with atomic outputs;
- wrong signing keys, missing subject participation and byte tampering fail;
- returned bytes are parsed, canonically reserialized and compared with the
  verifier's canonical source before its signature is produced.

This is an in-process, transport-independent exchange over real serialized
block objects. It is not evidence of DNS, HTTP, socket, timeout or four-host
operation. The public adapter remains intentionally unavailable until its
authorization callback is backed by integrated frozen v2 state; exposing it
earlier would create an unsafe signing boundary.

## CO-05 bounded-envelope validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_envelope_v2.*'
```

Results reproduced on 2026-09-06:

- envelope tests: **10/10 passed**;
- complete focused EPoSE binary after integration: **110/110 passed**;
- daemon target rebuilt successfully with the envelope object linked;
- canonical envelope and outer-field round trips passed;
- noncanonical/overflowing varints, wrong tag, bad magic/version, nonzero flags,
  unknown/disabled records, count/size mismatch, truncation, trailing data,
  empty envelope, oversized payload, and invalid limits fail closed;
- duplicate opaque records consume record and signature budget before later
  semantic handling;
- per-envelope and block-wide byte, record, signature, and admission budgets
  fail without committing a partial block budget;
- miner and fee-funded field fixtures produce identical parse results and cost.

Subsequent corrective-stack validation added the real `tx_extra_epose_v2`
variant, explicit default rejection, exact version-gated opt-in, unrelated-field
preservation, and the fixed 402-byte service-receipt codec. The carrier/receipt
subset now contains 22 passing tests and the complete focused binary contains
154 passing tests. At that historical stage HF17 and unscheduled version 19
rejection, duplicate-field
limits, malformed receipt size/signature and atomic output reuse are covered.

This historical result is not launch-handler evidence. The launch mapping now
assigns hardened EPoSE v2 to exact QWC HF17; no canonical block-state adapter
consumed these records at the recorded stage. Remaining codecs, actual template batching,
wallet funding, queue fairness, worst-valid-block timing, synchronization cost,
and approved manifest limits remain open.

## CO-06 reward and scoped-proof validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_reward_v2.*'
```

Results reproduced on 2026-09-06:

- reward/payment tests: **9/9 passed**;
- complete focused EPoSE binary after integration: **119/119 passed**;
- daemon target rebuilt successfully with the v2 reward object linked;
- subsidy-only arithmetic keeps all fees with the miner;
- 1,000-BPS rounding, overflow, invalid BPS, and fail-atomic rejection passed;
- both empty-set policies are modeled and an unset policy fails closed;
- payout selection rejects a wrong source epoch, duplicate member, missing
  context, empty set, and pre-epoch height;
- scoped payment proof generation/verification passed without a private view
  key;
- height, parent, payee, coinbase commitment, derivation, signature, output
  index/key, and transaction-secret substitutions fail closed;
- repeated rewards to the same public reward address use distinct derivations
  and one-time output keys.

This is not funds-spendability or activation evidence. The implementation is
not connected to coinbase construction/validation, its envelope record codec is
not finalized, the empty-set policy is unresolved, inherited emission
integration is unproved, and independent cryptographic review remains required.

## CO-07 lifecycle validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_lifecycle_v2.*'
```

Results reproduced on 2026-09-06:

- lifecycle tests: **8/8 passed**;
- complete focused EPoSE binary after integration: **127/127 passed**;
- daemon target rebuilt successfully with the lifecycle object linked;
- future-effective registration and exact idempotent duplicate passed;
- invalid-signature duplicates, post-cutoff/retroactive changes, wrong
  predecessor, wrong sequence, and illegal key changes fail closed;
- reward/endpoint updates require both operator and service signatures;
- online service-key recovery succeeds with offline operator authority plus the
  replacement key and leaves earlier epoch views unchanged;
- renewal extends expiry without mutating prior descriptors;
- delayed deregistration preserves history and takes effect at its declared
  epoch;
- state hash is independent of identity arrival order.

This is not activation evidence. Record serialization, persistent storage,
key-file atomicity/backup tests, migration, and operator-authority rotation are
not implemented.

## CO-08 state-index validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_state_index_v2.*'
```

Results reproduced on 2026-09-06:

- state-index tests: **8/8 passed**;
- complete focused EPoSE binary after integration: **135/135 passed**;
- daemon target rebuilt successfully with the state-index object linked;
- contiguous connect, exact idempotency, bounded retention, and fail-atomic
  invalid-connect behavior passed;
- same-height conflict, gap, wrong parent, wrong tip, schema, parameter set,
  checksum, and trailing data fail closed;
- canonical image roundtrip preserves tip and index root;
- disconnect beyond undo returns `rebuild_required` and canonical rebuild
  succeeds;
- A → B → A produces the same root as fresh A replay;
- corrupt restore leaves the committed state unchanged.

This is not durable-LMDB or crash-injection evidence. The same-transaction LMDB
adapter, reconstruction of full state objects, schema migration, process-kill
testing, and pruned/full-node compatibility remain activation gates.

## CO-09 resource-policy validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests --gtest_filter='epose_resource_policy_v2.*'
```

Results reproduced on 2026-09-06:

- resource-policy tests: **8/8 passed**;
- complete focused EPoSE binary after integration: **143/143 passed**;
- daemon target rebuilt successfully with the resource-policy object linked;
- signed descriptors are chain-context-bound and canonical DNS/IP forms pass;
- uppercase/single-label/malformed DNS, invalid transport, and wrong IP family
  fail closed;
- loopback, RFC1918, link-local metadata, ULA, and IPv4-mapped loopback are
  rejected while public IPv4/IPv6 fixtures pass;
- empty/oversized DNS result sets and any prohibited answer fail;
- request/response bytes, timeout, global concurrency, tracked-peer, and
  per-peer limits are enforced and released deterministically;
- unknown admission contexts allocate no cache entry;
- RPC page/scan arithmetic rejects oversize and wraparound boundaries.

This is not handler or load-test evidence. P2P/RPC/probe integration,
streaming/no-redirect socket enforcement, DNS race fixtures, RandomX VM hooks,
fuzzing, and sustained malicious-load benchmarks remain open.

## CO-10/11 release-gate validation

Commands:

```text
python3 -m py_compile tests/epose/release_gate_v2.py tests/epose/test_release_gate_v2.py
python3 tests/epose/test_release_gate_v2.py
python3 tests/epose/release_gate_v2.py \
  --manifest docs/epose/PARAMETER_MANIFEST_V2.json \
  --policy docs/epose/review/RELEASE_GATE_POLICY_V2.json \
  --gates docs/epose/review/RELEASE_GATES_V2.json \
  --evidence-root . \
  --expect no-go
```

Results reproduced on 2026-09-06:

- release-gate evaluator tests: **6/6 passed**;
- all corrected EPoSE Python model/gate/manifest tests: **34/34 passed**;
- complete focused C++ EPoSE binary: **143/143 passed**;
- EPoSE fuzz corpus: **11/11 files passed**;
- daemon rebuilt and linked at stacked base `e90f64c60`;
- malformed schema, duplicate gate, unsupported state, evidence-free satisfied
  gate, and false ready declaration fail closed;
- the current result is `no-go`: 0/13 top-level gates satisfied by candidate-bound evidence, 13 unresolved,
  and 24 required manifest fields unset;
- the corrected reservation-manifest canonical SHA-256 is recorded by the
  evaluator output; it changes whenever the typed manifest changes.

No genesis reset or four-host execution was performed. The existing integration
harness exercises the disposable legacy EPoSE-v1 path and cannot establish
QWC-HF17/EPoSE-v2 funds safety while the hardened handler, LMDB, final
parameters, reward policy, and payment-proof review remain open. Running it and
relabeling the result would be false launch evidence.

## Consolidated review corrections

The corrective stack added negative regressions for exact scheduled versions,
typed manifest semantics, mandatory release-gate IDs, candidate-bound evidence,
rare-event numerical stability, Wilson confidence intervals, full authenticated
receipt ingestion, distinct round windows/anchors, immutable lifecycle history,
authority-key separation, payout-epoch upper/lower bounds, atomic envelope
outputs, strict journal images, admission-cache rollover and reviewed address
prefix boundaries.

Reproduced on 2026-09-06:

- corrected Python model/manifest/gate suites: **34/34 passed**;
- complete focused EPoSE C++ binary: **151/151 passed**;
- affected lifecycle/receipt/envelope/reward/resource/journal subsets:
  **69/69 passed**;
- current release result: **NO-GO**, 0/13 mandatory gates satisfied by
  candidate-bound evidence and 24 manifest fields unset.

These corrections do not connect hardened EPoSE v2 to QWC-HF17 block
validation, LMDB, live probing,
wallet construction or release publication. They close concrete primitive
defects while leaving their integration gates explicit.

## Canonical RandomX admission validation

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_v2.admission_*:epose_v2.snapshot_is_canonical_across_admission_arrival_order:epose_v2.duplicate_admission_is_idempotent_but_conflict_is_rejected:epose_receipts_v2.authenticated_receipt_is_accepted_by_frozen_membership_pipeline'
```

The corrective branch replaces the caller-trusted lease hash with a typed
322-byte lease whose RandomX work and canonical lease hash are recomputed before
membership state can change. The proof binds network, genesis, parameter set,
separate service/operator keys, derived identity, descriptor/reward/endpoint
commitments, sequence, target epoch, algorithm, target, canonical context and
nonce. Output decoding is atomic.

The one permitted context for target epoch `E` is fixed at `start(E-1)`. A
fully recomputed valid RandomX proof using height `start(E-1)-1` is rejected by
the pipeline, as are wrong-network proofs, same-height contexts, claimed work
hashes, caller-selected lease hashes, redirected endpoints and identical
online/offline authority keys.

Results reproduced on 2026-09-06:

- admission/snapshot/receipt integration subset: **6/6 passed**;
- complete focused EPoSE C++ binary: **161/161 passed**;
- corrected Python model/manifest/gate suites: **35/35 passed**;
- broad `unit_tests` target and daemon target rebuilt successfully.

This is still not handler, persistent-state, optimized-solver or parameter
approval evidence. The mainnet manifest remains non-activatable while its
security and resource parameters are unset.

## Remaining typed-record codecs

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_envelope_v2.*:epose_lifecycle_v2.*:epose_reward_v2.*'
```

Results reproduced on 2026-09-06:

- envelope/lifecycle/reward subsets: **35/35 passed**;
- complete focused EPoSE C++ binary: **163/163 passed**;
- corrected Python model/manifest/gate suites: **35/35 passed**;
- broad `unit_tests` target and daemon target rebuilt successfully;
- registration and later lifecycle actions share a fixed 378-byte codec while
  the outer record type must agree with the signed action;
- malformed, relabelled and signature-tampered lifecycle records fail without
  exposing a decoded value;
- the fixed 96-byte scoped-payment-proof codec accepts only a proof valid for
  the reconstructed payment context and clears reused outputs on failure;
- envelope accounting charges the two lifecycle signatures and the payment
  proof operation before semantic duplicate handling.

This does not yet prove the generic semantic batch adapter, producer queue
integration, wallet submission, sustained fuzzing or worst-valid-block cost.

## Deadline relay queue and class reservations

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_resource_policy_v2.*'
```

Results reproduced on 2026-09-06:

- resource-policy subset: **12/12 passed**;
- complete focused EPoSE C++ suite: **166/166 passed**;
- complete EPoSE Python model/manifest/gate suite: **36/36 passed**;
- broad `unit_tests` target and `daemon` target rebuilt and linked;
- enrollment flooding cannot consume the configured evidence item/byte reserve,
  and the symmetric rule protects enrollment;
- duplicate IDs are idempotent only for identical metadata; conflicting reuse
  fails and expired records release capacity before admission;
- deterministic template selection fills nonzero shares for both classes before
  deadline-ordered surplus capacity;
- invalid share sums fail atomically in both the C++ policy and typed manifest.

This is local relay/template policy. It does not change complete-block validity
and is not yet evidence for producer integration or measured inclusion latency.

## Atomic semantic transaction batch

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_semantic_batch_v2.*'
```

Results reproduced on 2026-09-06:

- semantic-batch subset: **7/7 passed**;
- complete focused EPoSE C++ suite: **174/174 passed**;
- broad `unit_tests` and `daemon` targets rebuilt and linked;
- lifecycle followed by its bound admission applies in serialized order;
- admission before lifecycle fails without retaining either operation;
- an invalid suffix rolls back a valid lifecycle prefix and clears the summary;
- a separately valid RandomX lease with a redirected reward binding fails
  against the authorized historical descriptor;
- canonical round anchors are re-resolved before receipt state mutation;
- post-cutoff lifecycle timing is derived from inclusion height, and scoped
  payment proof records are rejected outside Coinbase;
- Coinbase accepts one context-bound scoped proof, while duplicate proof
  records fail atomically.

The adapter is not yet an activated block handler. Exact required-payment
enforcement, block-level LMDB atomicity, disconnect/replay and sustained carrier
fuzzing remain open gates.

## Fresh-genesis target and cumulative PR validation

The intended public-chain topology was changed from a later chain-preserving
transition to a fresh QWC-HF17 genesis over the inherited Monero-HF16 baseline.
EPoSE protocol/format version 2 is a separate version domain. This changes the
target specification, manifest, and boundary vectors; it does not by itself
complete the v2 runtime path.

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j2
<build-dir>/tests/unit_tests/epose_unit_tests
python3 -m unittest discover -s tests/epose -p 'test_*.py'
python3 tests/epose/reference_model_v2.py \
  tests/epose/vectors/co01_transition_v2.json
python3 tests/epose/release_gate_v2.py \
  --manifest docs/epose/PARAMETER_MANIFEST_V2.json \
  --gates docs/epose/review/RELEASE_GATES_V2.json \
  --policy docs/epose/review/RELEASE_GATE_POLICY_V2.json \
  --evidence-root . --expect no-go
git diff --check
```

Results reproduced on 2026-09-06:

- complete focused EPoSE C++ suite: **186/186 passed**;
- complete EPoSE Python model/manifest/gate suite: **38/38 passed**;
- fresh-genesis reference vectors passed;
- QWC block major version 17 is the only accepted launch version at model
  height 0; EPoSE protocol/format version remains 2;
- epoch 0 is enrollment-only, epoch 1 is the first service epoch, and the first
  possible v2 payout is height 1440;
- release evaluation remains **NO-GO**, with **0/13** gates satisfied and
  **29** unresolved manifest values;
- canonical reservation manifest SHA-256 is
  `6377b1eb1eca7b44165cf64f66b1d686ec657e99ca057a0041ba4990c83e8699`;
- documentation parsing, Python byte-compilation, whitespace, and credential
  pattern checks passed.

The cumulative PR targets `main` and retains the signed commit history from the
superseded stacked review PRs. This evidence does not authorize merge,
deployment, genesis generation, or economic activation.

## Canonical Coinbase payment context

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests -j1
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_envelope_v2.*:epose_reward_v2.*'
```

Results reproduced on 2026-09-07:

- envelope and reward subset: **28/28 passed**;
- noncanonical generic outer `tx_extra` lengths fail closed;
- payment-proof stripping is atomic and preserves unrelated record grouping
  and wallet metadata;
- proof-excluded commitments are identical before and after adding the proof,
  but change when any other Coinbase field changes;
- validation derives the paid output indices, amounts, and one-time keys from
  the actual Coinbase plus scoped derivation;
- wrong amounts and duplicate proof records fail with no returned context.

The final payout schedule, empty-set/emission policy, and manifest resource
limits remain unset. These tests prove the non-circular construction and
validator boundary, not an activated producer or economic policy.

## Fail-closed consensus coordinator

Command:

```text
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_coordinator_v2.*'
```

Results reproduced on 2026-09-07:

- coordinator subset: **4/4 passed**;
- incomplete parameters and block versions 16/18 fail before state mutation;
- genesis through height 1439 has no service allocation or payment proof;
- height 1440 consumes the closed epoch-1 qualification set;
- both modeled empty-set policies produce deterministic allocations;
- a real lifecycle/admission set freezes at height 660, two authenticated
  receipts qualify one subject in epoch 1, and the resulting frozen descriptor
  controls the actual proof-bearing Coinbase at height 1440;
- an invalid Coinbase amount leaves the pre-block state unchanged and the
  corrected candidate can be retried at the same height;
- the returned state root and parameter hash are ready for the same-transaction
  LMDB commitment written by the production `Blockchain` owner.

These tests use explicit private fixture parameters. The checked-in production
manifest remains incomplete and cannot construct this coordinator. The
coordinator is now wired into `Blockchain::add_new_block`, miner templates,
same-transaction LMDB commitments, bounded disconnect, and full canonical
startup/deep replay; therefore an incomplete public manifest stops daemon
initialization before genesis rather than selecting legacy behavior.

## Production HF17 coordinator, template, and replay integration

Commands:

```text
cmake --build <build-dir> --target epose_unit_tests unit_tests daemon \
  qwertycoin-wallet-cli qwertycoin-wallet-rpc -j1
<build-dir>/tests/unit_tests/epose_unit_tests
<build-dir>/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_reward_v2.*:epose_coordinator_v2.*:epose_v2.later_rounds_have_distinct_windows_and_fresh_canonical_anchors'
python3 -m unittest discover -s tests/epose -p 'test_*.py'
```

Candidate behavior:

- exact HF17 block acceptance invokes only the hardened v2 coordinator;
- public mainnet/testnet/stagenet initialization fails before genesis while the
  compiled manifest factory is incomplete; fixture injection is FAKECHAIN-only;
- miner construction consumes the same reward plan used by validation and
  emits a scoped proof from the actual Coinbase transaction secret;
- the proof validator performs one real transaction-proof verification and
  passes a complete-record-bound capability to semantic application;
- later receipt rounds start one height after their in-epoch anchor;
- connect stores the state commitment atomically with the block; disconnect
  removes it atomically; restart/deep replay recomputes reward inputs and every
  state root from canonical chain data before installing reconstructed state;
- an aborted multi-block LMDB batch rebuilds the coordinator from the reverted
  canonical chain, and alternative-parent templates fail closed until
  branch-specific state replay exists;
- fee and miner/service output sums are overflow-checked;
- legacy post-proof Coinbase padding is forbidden because it would change the
  committed proofless bytes.

Final numeric limits, empty-set/emission policy, genesis hash, native macOS
arm64 evidence, and network/wallet end-to-end evidence remain launch gates.
