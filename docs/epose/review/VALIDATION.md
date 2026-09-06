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
  `3cc33c916af6cd377485e163dd41006a9ea6ff8899ae4e31205cd01f104fe5f1`;
- reference-model unit tests: **13/13 passed**;
- activation, cutoff, anchor, deadline, payout, envelope, malformed-length,
  unknown-record, noncanonical-varint, and uint64-overflow vectors passed;
- the manifest test proves required activation fields remain unset and status
  remains `not-activatable`.

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

The live bounded probe transport, v2 serialization/carrier, endpoint descriptor
discovery, and multi-host offline-subject test remain open dependencies in
CO-05/CO-07/CO-10. This evidence does not claim that dual signatures prove
operator independence or prevent a colluding committee/shared proxy.

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

This is not HF18 activation evidence. The historical generic `tx_extra` parser
does not yet recognize tag `0x05`; payload codecs, actual template batching,
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
