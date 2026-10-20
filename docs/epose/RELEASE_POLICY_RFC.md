# Service release admission — research and isolated prototype

Owner: Sw0rdfish. Status: **Draft; not a production activation proposal**.

## Scope and first findings

Baseline: `qwertycoin-org/qwertycoin` default branch `main`,
`5a834c2e95ced1b6285605ea9282776fcbe0d382` (2026-10-20). No open PRs or
issues were returned during preflight. Existing secure-updater, release-DNS,
telemetry and governance branches are not authorization to change consensus.
This work does not merge those branches or change production configuration.

Published stable source references inspected separately:

| Tag | Commit |
| --- | --- |
| v2.0.4 | `1e479cc9feda48e07dd07ba6abcb89ca3934b5e2` |
| v2.0.5 | `f1db1c0df16b180c608d609a6e5966c701d1c292` |

Publication is not evidence of deployed Mainnet software. No production-node
inventory or operator telemetry is available to this investigation.

The current endpoint descriptor's `uint8_t service_version` is a protocol
version, not a release ordinal. `service_producer_v2.cpp` sets it to
`EPOSE_PROTOCOL_VERSION_V2`. It must not be repurposed. Existing receipt
subject/verifier signatures bind the challenge and response hash, but do not
contain software release IDs (`service_receipt_v2.{h,cpp}`).

`membership_pipeline_v2::apply_authenticated_receipt` checks frozen membership,
endpoint commitment, round/anchor/inclusion window, selected verifier and both
signatures. `close_qualification` requires distinct selected verifiers in enough
rounds. Therefore positive qualification is the appropriate extension point;
introducing a negative ban registry solely for updates is unnecessary.

The compiled profile uses epoch length 720, anchor depth 60, committee cap 9,
nominal threshold 6, three rounds at offsets 0/200/400 and two required rounds.
The actual threshold is `n - n/3` for `n=min(9,population-1)`; one identity alone
cannot qualify. Admission is RandomX with 18 leading zero bits and is not staking.

## Options and working decision

| Option | Trade-off |
| --- | --- |
| A: compiled minimums | Smallest authority surface; every new minimum needs validator rollout. Suitable fallback. |
| B: negative quorum certificates | Reuses Oxen's architecture there, but Qwertycoin would need new state changes and trust assumptions. Not a drop-in 7/10 solution. |
| C: bounded authorized chain records | Explicit developer authority; deterministic replay and later release changes without another generic-policy binary upgrade. More initial consensus work. |
| D: DNS/HTTP/GitHub decisions | Reject as a consensus source: validators can see inconsistent or unavailable data. Mirrors/notifications only. |

Prototype C while retaining existing positive service qualification. This is
centralized release authorization, **not decentralized release governance**.
It cannot install unknown service functions, record formats or consensus rules
into an old validator. Bootstrap and later protocol upgrades still need rollout.

## Safety boundary

| Goal/attack | Guarantee |
| --- | --- |
| Unmodified obsolete official service continues qualifying | New-format signed evidence and target-epoch admission will exclude it after bootstrap. |
| Unauthorized official release or invented high ID | Registry and independent authority signatures; not `id >= minimum` alone. |
| Local minimum changed | Does not change independent validators. |
| Old code claims a real new ID | **Not prevented** by a signed self-claim or build hash. |
| Missing externally verifiable function | Test the actual service behavior; no cosmetic constant challenge. |
| Conforming modified fork | Remains permitted in an open protocol. |
| Exact unmodified binary execution | Not established. No universal secret, DRM or remote-attestation claim. |

Canonical block fetching provides a real verifier-side byte/hash check. The
chain verifies the signed transcript, not a historical network transfer.
Colluding subject/verifier keys can sign claims without doing the transfer.
Memory-leak and internal-scheduler fixes generally cannot be proven remotely.

## Delivery plan and review boundaries

1. Pin the full Qwertycoin and historical/current Oxen paths and activation math.
2. Specify bounded release records, target-epoch binding, authority rotation and
   recovery constraints, cancellation and pre-activation newer releases.
3. Implement an **isolated**, test-only policy/receipt prototype using existing
   crypto and test infrastructure; preserve the production wire protocol.
4. Record actual tests and remaining integration gates. Do not call a standalone
   state-machine test a live multi-daemon rollout.

No Mainnet H0, production release IDs, private release keys, releases,
deployments, emission changes or automatic binary updates are authorized here.
The critical-update single-release exception needs a separate explicit review
decision and is not silently enabled. Review owner: Sw0rdfish.

Production completion requires a versioned carrier/receipt integration, shared
validation/diagnostics derivation, state commitment migration and restart/reorg
proof, a reproducible isolated multi-daemon rollout and reviewed authority/H0
choices. The PR remains Draft until those validation gates are satisfied.
