# EPoSE documentation inventory

This inventory separates current operator/protocol documentation from retained
security-review evidence. Source code and consensus tests remain authoritative
if prose and implementation ever disagree.

## Current reference

These files describe the EPoSE v2 implementation on the current branch:

| Document | Purpose |
| --- | --- |
| [`README.md`](README.md) | Entry point, implementation map, and documentation policy |
| [`PROTOCOL.md`](PROTOCOL.md) | Consensus lifecycle, membership, receipts, rewards, and reorg behavior |
| [`SECURITY_PARAMETERS.md`](SECURITY_PARAMETERS.md) | Exact compiled consensus and resource values |
| [`CONSENSUS_INVARIANTS.md`](CONSENSUS_INVARIANTS.md) | Fail-closed invariants enforced by code and tests |
| [`THREAT_MODEL.md`](THREAT_MODEL.md) | Assets, trust boundaries, mitigations, and residual risks |
| [`REWARDS.md`](REWARDS.md) | Subsidy split, payee selection, Coinbase proof, and RPC interpretation |
| [`RPC.md`](RPC.md) | EPoSE daemon RPC methods and exposure rules |
| [`DIAGNOSTICS.md`](DIAGNOSTICS.md) | Stable receipt failure reasons and operator troubleshooting |
| [`SERVICE_NODE.md`](SERVICE_NODE.md) | Service-node operation and keystore handling |
| [`COMMUNITY_SETUP.md`](COMMUNITY_SETUP.md) | Community-node installation and validation checklist |
| [`DNS_RECORDS.md`](DNS_RECORDS.md) | QWC-owned DNS names and publication constraints |
| [`BENCHMARKS.md`](BENCHMARKS.md) | Historical measurement evidence; not a parameter source |

The broader implementation overview is
[`../architecture/CURRENT_STATE.md`](../architecture/CURRENT_STATE.md).
Operator examples must remain portable: private deployment topology and access
metadata are not documentation inputs.

## Machine-readable activation and release artifacts

The following files are intentionally retained because tests or release tools
consume them:

- [`PARAMETER_MANIFEST_V2.json`](PARAMETER_MANIFEST_V2.json) records the signed
  activation-candidate parameter commitment and its source dependency snapshot.
  Its historical `dependencies` and `release.source_revision` fields are not a
  claim that the repository is still at those commits.
- [`review/RELEASE_GATE_POLICY_V2.json`](review/RELEASE_GATE_POLICY_V2.json)
  defines release-gate policy.
- [`review/RELEASE_GATES_V2.json`](review/RELEASE_GATES_V2.json) is the retained
  evidence ledger consumed by release checks.
- Files under [`../../tests/epose/vectors/`](../../tests/epose/vectors/) and
  [`review/results/`](review/results/) are deterministic review evidence.

Changing a JSON artifact can invalidate hashes or release checks. Do not edit
one merely to make narrative documentation look current.

## Historical security review

Markdown files under [`review/`](review/) preserve design decisions, threat
findings, acceptance matrices, and validation snapshots from the v2 hardening
work. They are audit history, not a second current protocol specification.
Their dates, commit IDs, open findings, or release verdicts apply to the
snapshot named in each file unless explicitly updated.

Operational host inventories and access notes are intentionally not retained in
the current public tree. Git history preserves prior snapshots for authorized
archaeology without presenting them as current runbooks.

## Removed development notes

The documentation set previously contained German change reports, PR
summaries, test-phase diaries, duplicate design descriptions, and analyses that
mixed retired EPoSE v1 behavior with v2 plans. They were removed from the
current tree because later implementation made their instructions misleading.
Git history remains the audit source for those development snapshots.

## Code source of truth

When reviewing or updating these documents, start with:

- `src/epose/coordinator_v2.cpp` and `src/epose/compiled_profile_v2.h`;
- `src/epose/block_transition_v2.cpp` and `src/epose/semantic_batch_v2.cpp`;
- `src/epose/membership_v2.cpp`, `lifecycle_v2.cpp`, and `reward_v2.cpp`;
- `src/epose/envelope_v2.cpp`, `record_codec_v2.cpp`, and
  `resource_policy_v2.cpp`;
- `src/cryptonote_core/blockchain.cpp` and
  `src/cryptonote_core/cryptonote_tx_utils.cpp`;
- `src/rpc/core_rpc_server.{h,cpp}`;
- `tests/unit_tests/epose_v2.cpp`, related EPoSE tests, and
  `tests/epose/integration/`.

The legacy files without a `_v2` suffix remain in the tree for compatibility
and test coverage. They do not define the active public-chain EPoSE profile.
