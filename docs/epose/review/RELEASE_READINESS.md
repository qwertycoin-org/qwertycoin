# EPoSE v2 Release Readiness

**Assessment date:** 2026-09-06

**Status:** **NO-GO for economic activation**

**Scope:** CO-10 and CO-11 gate assessment

## Decision

The stacked CO-00 through CO-09 branches provide reviewed design boundaries and
non-activating C++ primitives. They do not yet form an integrated QWC-HF17 /
EPoSE-v2 launch protocol.
The current mainnet manifest intentionally contains unset consensus parameters
and has status `not-activatable`.

It would therefore be invalid to reset a network from genesis and describe the
result as an EPoSE v2 funds-safety test. The existing Docker integration harness
exercises the disposable legacy EPoSE-v1 prototype, including its reward
view-key interface. It cannot establish the hardened EPoSE-v2 properties
required by CO-10.

No production, public-testnet, or four-host deployment of this branch is
authorized by this assessment.

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
- 30 required manifest values remain unset.
- 13 gates remain blocked or not run.
- The security-parameter study independently reports
  `no_go_for_economic_activation`.

The source/invariant baseline and normative transition reservation remain useful
review material, but their earlier filename-only evidence is not a verified
candidate-bound gate result. All implementation evidence through CO-09 remains
valuable, but a component unit test is not promoted to a release-gate pass until
the component is connected to the canonical block transition and tested there.

## Blocking implementation work

1. Integrate the version-aware envelope and record codecs into the QWC-HF17
   fresh-genesis parsing, block connect/disconnect, relay, miner templates, and
   fee-funded wallet construction. Retire legacy-v1 production dispatch.
2. Complete canonical handler ownership of the v2 state. LMDB now stores a
   schema-bound state/parameter commitment in the same write transaction as
   each canonical block, and the streaming replay verifier rejects missing or
   mismatched commitments. The handler still must supply those commitments,
   retire the legacy state, and prove crash recovery, pruning and deep replay
   through the production coordinator.
3. Connect the bounded probe/descriptor policy to live P2P and RPC handlers and
   measure worst-valid-block plus sustained invalid-load behavior.
4. Select committee, round, admission, capacity, and resource constants from
   optimized measurements on the supported hardware classes and an approved
   adversarial risk budget.
5. Approve one empty-set and emission policy. The implementation must then prove
   exact inherited emission continuity and the PoW-security consequence.
6. Obtain independent review of the scoped payment proof and the integrated
   consensus state transition.

## CO-10 execution preconditions

The four-host test may start only when all six items above are closed and the
candidate manifest contains the final fresh-genesis identity, parameters, and
source revision. Activation height is already fixed at zero; there is no future
activation-block observation or migration boundary. Before any reset:

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

The eventual run must cover enrollment, authenticated service, both carriers,
qualification, payout, maturity, spend, recipient receipt, rescans, restarts,
partition/heal, payout-fork replacement, deep reorg, fresh replay, and identical
state roots on all honest nodes.

## CO-11 release conditions

Activation requires a complete signed manifest, reproducible source-to-binary
evidence, Linux/Docker/macOS build results, checksums, operator migration and
rollback guidance, dependency inventory, public protocol/vectors, and linked
independent review findings. No administrator RPC, DNS record, privileged key,
or local relay decision may change consensus qualification or payout rules.

Until those conditions are met, the only correct release result is **NO-GO**.
