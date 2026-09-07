# EPoSE v2 Release Readiness

**Assessment date:** 2026-09-07

**Status:** **NO-GO for economic activation**

**Scope:** CO-10 and CO-11 gate assessment

## Decision

The consolidated branch connects the hardened v2 coordinator to QWC HF17 from
genesis. The checked-in mainnet manifest is now a complete, embedded
`activation-candidate` for the explicitly authorized four-host pre-launch
rehearsal. Configuration completeness permits that bounded rehearsal; it does
not satisfy the evidence-bound launch gates or authorize economic launch.

The historical PR-#183 Docker run exercised legacy production dispatch and is
not v2 evidence. A new seed-00 through seed-03 run must use the regular MAINNET
path and the exact embedded candidate, while being publicly identified as a
resettable rehearsal. The planned chain-only reset follows the run and must pin
a distinct final-launch genesis before any launch claim.

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

- 0 of 13 top-level release gates are satisfied by candidate-bound evidence.
- 0 required manifest values remain unset in the rehearsal candidate.
- 13 gates remain blocked or not run.
- The security-parameter study independently reports
  `no_go_for_economic_activation`.

The source/invariant baseline and normative transition reservation remain useful
review material, but their earlier filename-only evidence is not a verified
candidate-bound gate result. All implementation evidence through CO-09 remains
valuable, but a component unit test is not promoted to a release-gate pass until
the component is connected to the canonical block transition and tested there.

## Blocking implementation work

1. Complete live lifecycle/admission/receipt construction and the bounded
   canonical service probe. The canonical HF17 parser, block transition,
   fee-funded wallet carrier, typed P2P semantic ingress, bounded relay pool,
   miner-template carrier and legacy-v1 retirement are implemented.
2. Prove the production coordinator's same-transaction LMDB commitments,
   bounded disconnect, startup verification and deep replay under process-crash,
   pruning and full canonical reorg scenarios.
3. Exercise the connected P2P/RPC/descriptor protections across the four
   operated rehearsal nodes
   and measure worst-valid-block, sustained invalid-load, backlog and inclusion
   behavior before selecting the six local queue/template reservations.
4. Measure the embedded rehearsal committee, round, admission, capacity and
   resource constants on supported hardware, then select final values against
   an approved adversarial risk budget.
5. Prove the selected miner-fallback, actual-issued-subsidy policy against exact
   inherited emission continuity and document its qualification-suppression
   incentive in the final risk decision.
6. Obtain independent review of the scoped payment proof and the integrated
   consensus state transition.

## CO-10 rehearsal boundary

The four-host test is authorized to generate the evidence needed by the open
gates. It may start when the candidate manifest is complete, the compiled
profile matches its commitment, local build/regression/startup checks pass, and
the exact destructive inventory has been recorded. Activation height is fixed
at zero; there is no future activation-block observation or migration boundary.
Before any reset:

- inventory every container, volume, wallet, chain directory, and persistent
  service identity on the target host;
- preserve wallets, seeds, and service identity keys separately from disposable
  chain state;
- pin one source commit, submodule set, compiler/toolchain, image digest, and
  parameter-manifest hash;
- use newly generated worthless test wallets;
- never perform host administration on shared seed-02 or seed-03; QWC actions on
  those machines remain confined to Docker;
- treat an unconfirmed SSH host-key replacement as unavailable infrastructure.

The rehearsal must cover enrollment, authenticated service, both carriers,
qualification, payout, maturity, spend, recipient receipt, rescans, restarts,
partition/heal, payout-fork replacement, deep reorg, fresh replay, and identical
state roots on all honest nodes.

## CO-11 release conditions

Activation requires a complete signed manifest, reproducible source-to-binary
evidence, Linux/Docker/macOS build results, checksums, operator migration and
rollback guidance, dependency inventory, public protocol/vectors, and linked
independent review findings. No administrator RPC, DNS record, privileged key,
or local relay decision may change consensus qualification or payout rules.

Until those conditions are met, the only correct final-launch result is
**NO-GO**. This does not conflict with the bounded pre-launch rehearsal.
