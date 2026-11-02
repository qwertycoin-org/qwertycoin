# Release admission: source audit

Research date: 2026-10-20. Facts below are source observations, not claims about
software actually deployed by operators. Owner: Sw0rdfish.

## Qwertycoin reference set

All default-branch links are pinned to
[`5a834c2e95ced1b6285605ea9282776fcbe0d382`](https://github.com/qwertycoin-org/qwertycoin/tree/5a834c2e95ced1b6285605ea9282776fcbe0d382).
The same SHA was the supplied preliminary baseline and was still `origin/main`
at clone/fetch. No open PRs/issues were returned. Explicit ancestry checks showed
`feature/secure-updater`, `feature/automate-release-dns`,
`feature/epose-node-telemetry-v204`, `feature/p1-governance-ci`,
`hardening/epose-v2-semantic-batch` and `perf/epose-undo-journal` already contained
in that baseline; these are not independent pending PRs to duplicate or adopt.
Published source tags are not the default branch:

- [`v2.0.4`](https://github.com/qwertycoin-org/qwertycoin/tree/1e479cc9feda48e07dd07ba6abcb89ca3934b5e2)
  resolves to `1e479cc9feda48e07dd07ba6abcb89ca3934b5e2`.
- [`v2.0.5`](https://github.com/qwertycoin-org/qwertycoin/tree/f1db1c0df16b180c608d609a6e5966c701d1c292)
  resolves to `f1db1c0df16b180c608d609a6e5966c701d1c292`.
- Both tags' `service_receipt_v2.h` have the old challenge and two signatures,
  **no subject/verifier software release ID**. Neither supports this prototype's
  policy or claim wrapper. Existing signed protocol version 2 cannot establish
  whether the peer runs one of those two releases. The numeric IDs/labels used in
  tests are explicitly fictional registry fixtures, not retroactive attestations.

Both published tags also expose `software_version=QWERTYCOIN_VERSION_FULL` in
`on_get_epose_service_endpoint_v2` for the local descriptor, alongside
`telemetry_available` and `telemetry_version`. This is **unsigned RPC telemetry**,
not a field in the descriptor signing message or authenticated receipt. See
[`core_rpc_server.cpp`](https://github.com/qwertycoin-org/qwertycoin/blob/f1db1c0df16b180c608d609a6e5966c701d1c292/src/rpc/core_rpc_server.cpp#L993).
It is useful for operator display, not a historical consensus release claim.

### Validation and state paths

| Source / functions | Observed behavior |
| --- | --- |
| [resource_policy_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/resource_policy_v2.cpp), `hash_endpoint_descriptor_v2`, `validate_endpoint_descriptor_v2` | Domain/context-bound signed endpoint descriptor, including `service_version`; cache is non-consensus, bounded at 4096 entries and four future epochs. Descriptor bytes are not part of existing receipts. |
| [service_producer_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/service_producer_v2.cpp#L127), enrollment producer | Assigns protocol constant to endpoint `service_version`, not software version. |
| [service_receipt_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/service_receipt_v2.cpp), `hash_service_challenge_v2`, `hash_subject_response_v2`, `hash_authenticated_service_receipt_v2` | Context includes network UUID, genesis, parameter hash. Challenge binds epoch, round, snapshot, anchor, both roles, endpoint hash, nonce and requested object hash. Subject signs response hash/challenge; verifier signs response message plus subject signature. |
| [canonical_service_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/canonical_service_v2.cpp), answer/verify functions | Real block bytes are obtained/parsed/compared locally by service participants. Chain validation checks transcript hashes/signatures, not a historical download; verifier statements remain a trust assumption. |
| [lifecycle_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/lifecycle_v2.cpp), registry `apply` | Operator/service authorization and sequence/effective-epoch descriptor history bind identity, reward address and endpoint commitment. An upgrade need not replace service identity. |
| [membership_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/membership_v2.cpp), `apply_admission`, `freeze_membership`, `committee`, `apply_authenticated_receipt`, `close_qualification` | Canonical context-bound RandomX lease, frozen membership, deterministic per-subject/round verifier selection excluding subject, unique receipt slots and sufficient distinct verifier/round coverage; immutable closed qualification. |
| [semantic_batch_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/semantic_batch_v2.cpp), `apply_transaction` | Lifecycle/admission consistency checked against canonical descriptor history, not discovery cache. Payment proof only in valid carrier. Fail-atomic journal across typed records. |
| [block_transition_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/block_transition_v2.cpp), `apply_block`, `disconnect_tip` | Structural/cost pass before expensive crypto; freeze BEFORE anchor block records, close AFTER inclusive deadline records; strict sequential heights, bounded undo and explicit deep replay requirement. |
| [coordinator_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/coordinator_v2.cpp), compiled profile and `plan_reward` | Mainnet profile `{activation=0,length=720,anchor=60}`. Committee 9, actual `ceil(2n/3)`, three rounds 0/200/400, two required, cap 100 active identities. Admission 18-bit RandomX. Reward epoch consumes PREVIOUS service epoch's closed qualification. Service share 10%; no-qualified policy is miner fallback, not a burn. |
| [blockchain.cpp](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/cryptonote_core/blockchain.cpp), `init`, `apply_epose_block`, `rollback_epose_block`, `rebuild_epose_state` | Native compiled Mainnet profile is genesis/parameter-bound; test injection restricted to FAKECHAIN. Canonical records/commitments are applied during block acceptance; restart rebuilds from chain, deep undo triggers rebuild. |
| [replay_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/replay_v2.cpp), `replay_and_verify_v2`; [state_index_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/state_index_v2.h) | Streaming canonical replay verifies commitments, schema/parameter hashes, rather than trusting a serialized mutable cache. Index schema 1 and explicit corrupt/wrong-schema handling. |
| [envelope_v2](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/epose/envelope_v2.cpp), codec/budget functions | Types 1–5, exact supported versions, no generic release-policy carrier. Limits: 64KiB envelope, 256 records/envelope, four envelopes/transaction; 256KiB/1024 records/2048 signature checks/eight RandomX per block. Actual cost must not exceed charged cost. |
| [cryptonote_config.h](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/cryptonote_config.h), `is_qwc_epose_v2_hardfork` | `is_qwc_epose_v2_hardfork` is true for major versions >= HF17, preserving the state machine across later forks. This is NOT support for unknown new record formats/functions; supported record versions and the broader hardfork machinery still matter. Relay hardening at 21600 is distinct and must not be reused for H0. |
| [core_rpc_server_commands_defs.h](https://github.com/qwertycoin-org/qwertycoin/blob/5a834c2e95ced1b6285605ea9282776fcbe0d382/src/rpc/core_rpc_server_commands_defs.h) | Existing service-node/status/rewards and service-challenge/endpoint RPCs, with diagnostics separating admission/round failures. No software release-policy RPC contract yet. |

Relay queue prioritization, producer retries, DNS endpoint resolution and endpoint
caches are not consensus authorization. Filtering only in producer/relay would
not stop a miner from including old evidence; the future canonical transition
must reject old receipt formats for affected target epochs, not just hide them.

Existing resource-bounded parsing and undo do not imply all canonical history is
pruned. Lifecycle/lease/receipt/snapshot/qualification collections retain history;
full-history memory growth is an existing issue. Do not silently prune evidence
or add an unbounded second index as part of this feature. A policy archive must
remain reconstructible, even if hot lookups are bounded.

## Oxen chain of enforcement

Pinned historical tag `v9.2.0` =
[`75d35a62e1d9b8eebaa8d42645b1b3237f9ae797`](https://github.com/oxen-io/oxen-core/tree/75d35a62e1d9b8eebaa8d42645b1b3237f9ae797).

1. [`hardfork.h`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_basic/hardfork.h#L36)
   explicitly distinguishes network major version and service-node revision.
2. [`hardfork.cpp`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_basic/hardfork.cpp#L50)
   schedules revision **18/1 at block 839009**. Minimums are compiled, not DNS votes.
3. [`service_node_rules.h`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/service_node_rules.h#L216)
   `MIN_UPTIME_PROOF_VERSIONS` requires oxend 9.2.0, Lokinet 0.9.5 and Storage
   Server 2.2.0 for 18/1. Standard obligations quorum is ten with seven votes;
   conditional integration-test constants are not Mainnet values or Pulse quorum sizes.
4. [`uptime_proof.cpp`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/uptime_proof.cpp#L16)
   `Proof` includes `v`, `lv`, `sv` in the serialized/hash-signed proof. This
   authenticates self-reported versions, not actual binary execution.
5. [`service_node_list.cpp`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/service_node_list.cpp#L3006)
   gets `get_network_version_revision` at current chain height and rejects
   insufficient component versions; accepted proof timestamps no longer refresh.
6. [`service_node_quorum_cop.cpp`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/service_node_quorum_cop.cpp#L109)
   `check_service_node` observes expired proofs; credit/state governs whether to
   vote decommission or deregister. This is an off-chain obligation observation.
7. [`handle_obligations_vote`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/service_node_quorum_cop.cpp#L532)
   aggregates sufficient votes into a state-change transaction with ordered voter
   indices/signatures, state, target index and historical quorum height.
8. [`verify_tx_state_change`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/service_node_voting.cpp#L131)
   checks state type, lifetime, target/index bounds, seven-to-ten votes, distinct
   ordered voters and signatures from the historical obligations quorum.
9. [`state_t::process_state_change_tx`](https://github.com/oxen-io/oxen-core/blob/v9.2.0/src/cryptonote_core/service_node_list.cpp#L591)
   locates canonical/history quorum state, verifies the certificate and valid
   state transition, then updates service membership/reward eligibility.
10. Ordinary validators can accept this **already understood state-change
    format and transition** without knowing the new service minimum or replaying
    proof-age observations. This works only while the major protocol and state
    transition remain compatible. Updated honest observers must form a quorum.

### Later mandatory update and current code are different snapshots

The August 2022 [official announcement](https://oxen.io/blog/service-node-update-august-22)
(2022-08-25) announced release on August 31 and a September 14 service deadline,
with ordinary nodes not needing that service-only update. It is not about v9.2.0.
At [`v10.2.0`](https://github.com/oxen-io/oxen-core/tree/2a70f32f5743bd3554e9ed2ec4879a0d701e85ca)
(`2a70f32f5743bd3554e9ed2ec4879a0d701e85ca`),
[`hardfork.cpp`](https://github.com/oxen-io/oxen-core/blob/v10.2.0/src/cryptonote_basic/hardfork.cpp#L53)
schedules 19/2 at **1146479**;
[`rules`](https://github.com/oxen-io/oxen-core/blob/v10.2.0/src/cryptonote_core/service_node_rules.h#L207)
requires 10.2.0 / 0.9.9 / 2.4.0. The timestamp/comment in that tag differ;
activation is the block table, not the human timestamp. Contemporary deregistration
[documentation](https://docs.oxen.io/oxen-docs/using-the-oxen-blockchain/oxen-service-node-guides/service-node-deregistration)
also describes the quorum certificate and credit model; code takes precedence.

Current upstream default branch is **dev**, pinned at
[`8f87286eb7aba5e9747dffeb5321bb7dbc06c455`](https://github.com/oxen-io/oxen-core/tree/8f87286eb7aba5e9747dffeb5321bb7dbc06c455)
(2026-10-15 commit). Current
[`rules`](https://github.com/oxen-io/oxen-core/blob/8f87286eb7aba5e9747dffeb5321bb7dbc06c455/src/cryptonote_core/service_node_rules.h)
have network-specific minimum tables and a Session Router component. Current
[`hardforks`](https://github.com/oxen-io/oxen-core/blob/8f87286eb7aba5e9747dffeb5321bb7dbc06c455/src/cryptonote_basic/hardfork.cpp)
include Ethereum transition HF20/21/22 and newer Pulse changes on test networks.
Do not project those later architectures back onto historical service policing.
A further concrete code example is
[`33e70fd4d7f12b978b206512f386ee6ebebc8fa8`](https://github.com/oxen-io/oxen-core/commit/33e70fd4d7f12b978b206512f386ee6ebebc8fa8),
which schedules the reward-calculation maintenance update using the same table
pattern (Mainnet revision 22/1 at 1901759 in the current table).

## Inference, alternatives and unknowns

**Inference:** C fits Qwertycoin better than introducing B: positive receipts
already establish eligibility, while negative quorum certificates would add a new
persistent authority/state machine. A remains a simpler fallback if developer
key governance is rejected. Existing project crypto and GTest are reused; no
new crypto package, threshold scheme or external dependency is needed. Existing
Oxen implementations are useful references but are not a maintained drop-in
library for Qwertycoin's different receipt protocol.

**Trust:** RandomX admission gives work-based Sybil costs, not Oxen's locked stake.
A committee of a few cheap or colluding identities can falsify observations;
missing updated verifiers stalls positive qualification. Minimum policy does not
improve committee honesty or guarantee liveness. Filtering membership by release
would change snapshots/quorums and could amplify outages: do not do this casually.

**Unknown:** actual Mainnet deployment distribution, geographic/key independence,
operator upgrade time, censorship risk, production authority custodians and
production artifact coverage. These need operational evidence/review, not guessed
values. No production-node probing or changes were performed.
