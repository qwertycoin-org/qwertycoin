# Release policy prototype: validation record

Owner: Sw0rdfish. Status: **Draft; not production-ready**.

## Scope and reproducibility

Only `tests/epose/prototype/release_policy.{h,cpp}`, its GTest cases and two source
entries in `tests/unit_tests/CMakeLists.txt` are new executable code. The ordinary
daemon and consensus libraries do not include the prototype. No RPC, Mainnet
parameters, emission, RandomX rules, service reward share or production data
were changed. Test identities/authority keys are ephemeral test-only keys.

Native Linux/GCC build uses the repository's existing CMake and bundled GTest:

```sh
git submodule update --init --recursive
cmake -S . -B build/release-policy -G Ninja \
  -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release \
  -DUSE_DEVICE_TREZOR=OFF -DBUILD_GUI_DEPS=OFF
# Required on this native Ninja build because bundled GTest does not declare
# its archive outputs to Ninja. No repository build rules were changed for it.
cmake --build build/release-policy --target googletest -j2
cmake --build build/release-policy --target epose_unit_tests -j2
build/release-policy/tests/unit_tests/epose_unit_tests \
  --gtest_filter='epose_release_policy_prototype.*'
build/release-policy/tests/unit_tests/epose_unit_tests \
  --gtest_filter='-epose_release_policy_prototype.*'
```

At configuration, optional Python functional RPC tests were skipped because the
Python environment lacks requests/psutil/monotonic/zmq/deepdiff. This is not a
successful RPC test. No dependencies were upgraded or added to mask that fact.
Native compiler warnings in existing OpenSSL/device/core code were observed;
new-code indentation warnings found during the first compile were corrected.

## Results

- Native C++ Release build: succeeded. Incremental rebuild of new sources has no
  new compiler warnings.
- New prototype suite: **22/22 passed**, zero failures/errors, 14.364 seconds;
  GTest XML timestamp `2026-10-20T22:44:00` (host clock).
- Existing EPoSE regression suite: **282/282 passed**,
  zero failures/errors, 132.228 seconds; GTest XML timestamp
  `2026-10-20T22:44:21` (host clock). Together the two runs cover all 304
  cases in the augmented EPoSE test target.
- `git diff --check`: passed.
- GitHub CI was still running when this record was written. The PR's
  commit-bound checks (including Linux/Nix, diagnostics ASan and Windows ACL)
  are the source of final CI results; earlier cancelled documentation-head runs
  are not passing evidence for this implementation.

## New behavioral coverage

| Area | What the prototype cases exercise |
| --- | --- |
| Stable order | 2.0.4 → 2.0.5 → 2.1.0 with explicit IDs, skipped ordinals, known early updater, unknown/zero/high IDs, duplicate and bad predecessor, prerelease/nightly rejection |
| Epoch rule | Publication independent of schedule; multiple pending records, immutable epoch view before admission, exact notice boundary and overflowing epoch rejection |
| Authority | Two distinct existing-crypto signatures, missing/wrong/duplicate/unused slots, wrong network/genesis/parameter hash, old+new rotation quorum and old-generation rejection |
| Canonical sequence | First continuation wins, identical-sequence alternative rejected, failures leave state hash unchanged |
| Pending recovery | Latest pending cancellation before full-notice boundary; no late cancellation or removal of initial bootstrap window |
| Parsing/resources | Canonical byte round-trip, every truncated prefix, trailing bytes, unknown schema, overlong labels/records, one-record block cap, archive cap, UINT64 ID exhaustion |
| Bootstrap isolation | Unset H0 disabled, zero/max H0 invalid, Mainnet rejected, H0−1/H0/H0+1 records |
| Positive service path | Actual RandomX membership admission, selected verifier committees, signed existing receipts plus subject/verifier release claims, closed qualifications and actual payee selection |
| Entitlements and updates | Previously qualified ID1 still payable in reward epoch6 after new service floor2 at H=120; no new qualification in stale E6; same service identity can qualify normally in E7 |
| Role/signature binding | IDs, target epoch, policy hash, subject/verifier role and original transcript cannot be swapped; a verifier signature does not replace subject participation |
| Legacy/evasion limits | Old receipt payload has no wrapper/IDs; invented ID rejected; changing to a real allowed ID with identical service implementation succeeds and is labelled a known bypass |
| Actual service function | Existing canonical block service refuses missing object bytes despite a forged version claim; this is verifier observation, not binary attestation or proof of an additional new service capability |
| Liveness | Stale verifier evidence excluded, singleton network has no qualification; no threshold reset or emission change |
| Boundaries | H−1/H/H+1 and round anchor/first inclusion/deadline/close checks |
| Reproduction/undo | Independent in-memory policy replicas replay identical canonical bytes; policy reorg across schedule/activation; membership qualification/receipt undo; explicit deep replay beyond bounded undo |

## Unexecuted acceptance gates and limitations

- No live multi-daemon policy Regtest: the new carrier/transport is deliberately
  not implemented. Existing daemon network tests cannot demonstrate this feature
  by running unchanged. The rollout document specifies the concrete harness work.
- No production LMDB schema/commitment migration, shutdown/restart/rescan or
  alternate-chain replay with integrated new records. Prototype “restart” means
  constructing a fresh object and replaying a canonical byte log in memory.
- Independent model replicas are not independent running historical binaries;
  compatibility with a later generic policy is shown at the shared vocabulary
  level only. No released Qwertycoin version supports this bootstrap.
- No new externally observable service requirement is invented. Existing canonical
  object tests demonstrate availability checks; internal bug-fix execution cannot
  be remotely proven. Colluding verifier/subject keys remain able to lie.
- Cache/DNS/HTTP independence is structural in the test model (there is no such
  input), not a measured adversarial multi-daemon network partition experiment.
- Production lifecycle/descriptor/lease persistence remains the existing code;
  direct membership tests do not prove new chain-record integration through it.
- No macOS/Windows execution, new-policy fuzz campaign or live authority-key
  ceremony was performed. Research archive cap/lookup cost is not a final
  production resource-policy decision.

The full requested acceptance matrix is **not complete**. These gaps are not
hidden behind unit-test success. Keep the PR Draft, Mainnet H0 absent and all
production activation/release actions disabled pending the focused follow-ups.
