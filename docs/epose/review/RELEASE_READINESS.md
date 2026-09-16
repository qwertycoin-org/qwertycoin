# EPoSE v2 Release Readiness

**Assessment date:** 2026-09-13

**Status:** **NO-GO for economic activation**

**Scope:** CO-10 and CO-11 gate assessment

## Decision

The consolidated branch connects the hardened v2 coordinator to QWC HF17 from
genesis. ADR-0009 defines the coordinated Reset-1 identity with genesis
`4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39`
and parameter commitment
`2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6`.

The previous isolated four-node rehearsal remains valid evidence for the
superseded `90662948...` chain only. Because every satisfying gate is
candidate-bound, none of those results is promoted to Reset-1. This is an
evidence status, not a runtime feature switch; the activatable compiled profile
still fails closed unless its exact Reset-1 genesis is present.

## Machine-readable gate

`RELEASE_GATES_V2.json` is the evidence ledger. The standard-library evaluator
fails closed when a required manifest value is absent, a gate is unresolved, a
gate identifier is duplicated, or the declared overall state disagrees with
the computed state.

```bash
python3 tests/epose/release_gate_v2.py \
  --manifest docs/epose/PARAMETER_MANIFEST_V2.json \
  --policy docs/epose/review/RELEASE_GATE_POLICY_V2.json \
  --gates docs/epose/review/RELEASE_GATES_V2.json \
  --evidence-root .
```

The command exits nonzero while activation is blocked. Review and test jobs that
intend to prove the present no-go state must use `--expect no-go`; they must not
ignore the default failure.

## Current result

- 0 of 13 top-level release gates are satisfied by Reset-1-bound evidence.
- 0 required manifest values remain unset in the rehearsal candidate.
- 13 gates remain blocked or not run.
- The parameter values are unchanged. Their earlier study and rehearsal remain
  historical input, but Reset-1 source/artifact binding and operational
  evidence have not yet been recorded.

The source/invariant baseline and normative transition reservation remain useful
review material, but their earlier filename-only evidence is not a verified
candidate-bound gate result. All implementation evidence through CO-09 remains
valuable, but a component unit test is not promoted to a release-gate pass until
the component is connected to the canonical block transition and tested there.

## Blocking implementation work

1. Complete the normative transition transcript and differential-vector
   evidence for the exact candidate.
2. Prove the production coordinator's same-transaction LMDB commitments,
   bounded disconnect and replay under process-crash, pruning, partition/heal
   and full payout-boundary reorg scenarios on the exact candidate.
3. Complete worst-valid-block, sustained invalid-load, DNS-race, backlog and
   inclusion measurements for the connected P2P/RPC/descriptor protections.
4. Prove miner fallback and actual-issued-subsidy continuity together with the
   final-binary wallet maturity, spend, receipt, rescan, uniqueness and
   reward-fork replacement matrix.
5. Obtain independent review of the scoped payment proof and integrated
   consensus state transition.
6. Complete macOS Apple Silicon and remaining release/dependency artifacts,
   then publish a separately signed activation manifest only when every gate is
   satisfied.

## CO-10 rehearsal boundary

The isolated four-node final-genesis test completed on the exact candidate and
its immutable evidence is recorded in
`results/final_genesis_rehearsal_v1.json`. Activation height is fixed at zero;
there is no future activation-block observation or migration boundary. The
following safety boundary remains authoritative for any later rehearsal:

- inventory every container, volume, wallet, chain directory, and persistent
  service identity on the target host;
- preserve wallets, seeds, and service identity keys separately from disposable
  chain state;
- pin one source commit, submodule set, compiler/toolchain, image digest, and
  parameter-manifest hash;
- use newly generated worthless test wallets;
- run only in an explicitly authorized isolated test environment and do not
  administer unrelated host workloads;
- treat any endpoint whose host identity cannot be confirmed out of band as
  unavailable.

Independent exact-candidate partition/deep-reorg, wallet maturity/spend/receive/
rescan, release-matrix and review obligations remain in the release ledger even
though the bounded final-genesis rehearsal itself passed.

## CO-11 release conditions

Activation requires a complete signed manifest, reproducible source-to-binary
evidence, Linux/Docker/macOS build results, checksums, operator migration and
rollback guidance, dependency inventory, public protocol/vectors, and linked
independent review findings. No administrator RPC, DNS record, privileged key,
or local relay decision may change consensus qualification or payout rules.

Until those conditions are met, the only correct final-launch result is
**NO-GO**. This does not conflict with the bounded pre-launch rehearsal.
