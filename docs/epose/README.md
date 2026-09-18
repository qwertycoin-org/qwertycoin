# EPoSE v2 documentation

EPoSE (Egalitarian Proof of Service) is Qwertycoin's deterministic
service-node reward protocol. RandomX proof of work remains the only block
production and chain-selection mechanism. EPoSE observes canonical chain data,
qualifies service identities, and assigns part of the scheduled subsidy; it is
not proof of stake and it does not create an alternative chain.

This directory documents the implementation on the current `main` branch.
When prose and code disagree, the compiled consensus profile, transition code,
and block-validation tests are authoritative.

## Start here

| Document | Purpose |
| --- | --- |
| [`PROTOCOL.md`](PROTOCOL.md) | Normative protocol flow, state transitions, carrier, lifecycle, admission, receipts, qualification, and reorg behavior. |
| [`SECURITY_PARAMETERS.md`](SECURITY_PARAMETERS.md) | Exact compiled mainnet parameter values and their code locations. |
| [`REWARDS.md`](REWARDS.md) | Subsidy split, payee selection, Coinbase construction, payment proof, and wallet compatibility. |
| [`SERVICE_NODE.md`](SERVICE_NODE.md) | Operator CLI, network exposure, keystore rules, automatic enrollment, and recovery. |
| [`RPC.md`](RPC.md) | EPoSE daemon RPC methods and restricted-RPC exposure. |
| [`CONSENSUS_INVARIANTS.md`](CONSENSUS_INVARIANTS.md) | Properties that every accepted block and reorg must preserve. |
| [`THREAT_MODEL.md`](THREAT_MODEL.md) | Assets, trust boundaries, implemented mitigations, and residual risks. |
| [`COMMUNITY_SETUP.md`](COMMUNITY_SETUP.md) | Public-test wallet, node, mining, and service-node setup. |
| [`BENCHMARKS.md`](BENCHMARKS.md) | Admission-work and capacity measurements; not a consensus parameter source. |
| [`DNS_RECORDS.md`](DNS_RECORDS.md) | QWC-owned DNS records and publication constraints. |

Machine-readable files:

- [`PARAMETER_MANIFEST_V2.json`](PARAMETER_MANIFEST_V2.json) is the reviewed
  activation manifest. `src/epose/coordinator_v2.cpp` contains the compiled
  values used by consensus; `src/epose/compiled_profile_v2.h` binds the mainnet
  genesis and parameter-set hashes.
- [`review/RELEASE_GATE_POLICY_V2.json`](review/RELEASE_GATE_POLICY_V2.json) and
  [`review/RELEASE_GATES_V2.json`](review/RELEASE_GATES_V2.json) are consumed by
  release tooling. They classify stable/audit readiness; they are not runtime
  feature switches.

## Implementation map

| Behavior | Primary implementation |
| --- | --- |
| Compiled profile | `src/epose/compiled_profile_v2.h`, `src/epose/coordinator_v2.cpp` |
| Envelope parser and budgets | `src/epose/envelope_v2.*` |
| Identity lifecycle | `src/epose/lifecycle_v2.*` |
| Admission, membership, committees | `src/epose/membership_v2.*` |
| Canonical service receipts | `src/epose/service_receipt_v2.*`, `src/epose/canonical_service_v2.*` |
| Atomic semantic application | `src/epose/semantic_batch_v2.*`, `src/epose/block_transition_v2.*` |
| Reward plan and payment proof | `src/epose/reward_v2.*`, `src/epose/coordinator_v2.*` |
| Persistent state | `src/epose/state_index_v2.*`, `src/blockchain_db/*` |
| Relay and template selection | `src/epose/relay_pool_v2.*`, `src/cryptonote_protocol/*` |
| Local producer and keystore | `src/epose/service_producer_v2.*`, `src/epose/service_keystore_v2.*` |
| Daemon integration | `src/cryptonote_core/*`, `src/rpc/*`, `src/daemon/*` |

## Protocol summary

```text
normal RandomX miner
  -> produces the canonical chain
  -> carries bounded EPoSE envelopes in transactions/Coinbase

EPoSE service producer
  -> maintains a genesis-bound operator/service keystore
  -> publishes a signed endpoint descriptor
  -> enrolls for a future epoch with RandomX admission work
  -> answers canonical-object challenges
  -> signs receipts as subject or selected verifier

canonical block transition
  -> freezes membership at the epoch boundary
  -> validates authenticated receipts
  -> closes qualification deterministically
  -> pays one qualified identity in the next payout epoch
```

No wallet private key is stored in the EPoSE keystore. The daemon is configured
with a public primary reward address only. No funded registration transaction
is required by EPoSE v2.

## Documentation policy

Normative documentation describes only behavior reachable from the current
public-chain code. Historical hardening decisions remain under `review/` and
are explicitly non-normative. Superseded PR reports, early test-network logs,
and legacy-v1 design analyses were removed from the current tree; Git history
retains them when an archaeological reference is needed.

Do not copy a parameter from an old report into code or operations. Verify it
against `compiled_consensus_parameters_v2`, the manifest tests, and the current
release revision.

## Publication boundary

Public documentation may include consensus values, public RPC schemas, public
ports, example loopback bindings, and DNS names compiled into Core. It must not
contain private or historical host inventories, origin IP addresses, container
or volume names from production, administrative routes, access mappings,
monitoring topology, credentials, or secret-bearing configuration. Store that
material only in the approved private operations system.
