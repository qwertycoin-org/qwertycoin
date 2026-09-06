# Qwertycoin EPoSE v2

This directory documents the EPoSE protocol implemented on the current
`main` branch. Source of truth for protocol behavior is the code in
`src/epose`, `src/cryptonote_core`, `src/p2p`, `src/wallet`, deployment files
under `deploy/mainnet`, and the current unit/integration tests.

EPoSE keeps RandomX Proof of Work as the chain-security mechanism and adds a
deterministic service-node reward layer:

```text
RandomX PoW
=> block production and chain security

EPoSE
=> service-node registration
=> RandomX-bound admission
=> deterministic verifier committee
=> signed attestations
=> P2P attestation relay
=> bounded relay/template pool
=> inclusion by normal non-service-node miners
=> qualification
=> finalized reward-source epoch
=> deterministic payee rotation
=> 10% service reward / 90% miner reward
```

Service nodes are not miners. Miners do not need service-node keys. A normal
miner daemon can include relayed EPoSE registration or attestation payloads in
block templates.

## Current Protocol

- `PROTOCOL.md` - wire format, state transitions, relay, qualification, payout.
- `REWARDS.md` - reward split, governance-style one-time outputs, validation.
- `SERVICE_NODE.md` - operator model, CLI flags, Docker identity storage.
- `COMMUNITY_SETUP.md` - end-user installation, mining, and service-node
  registration guide.
- `CONSENSUS_INVARIANTS.md` - properties the implementation must preserve.
- `THREAT_MODEL.md` - current risks and mitigations.

## Implementation / Validation

- `IMPLEMENTATION_REPORT.md` - current implementation report for `main`.
- `MAINNET_VALIDATION.md` - controlled mainnet validation status.
- `HARDENING_STATUS.md` - DONE/PARTIAL/OPEN/ACCEPTED RISK matrix.
- `BENCHMARKS.md` - benchmark and simulation notes.
- `MACOS_M1_TESTING.md` - Apple Silicon test notes.
- `review/README.md` - security-review baseline, finding disposition, and
  activation decisions for the coordinated hardening program.

## Specialized Design Notes

- `ATTESTATION_RELAY.md`
- `ATTESTATION_RELAY_ANALYSIS.md`
- `HISTORICAL_GOVERNANCE_REWARD_ANALYSIS.md`
- `SERVICE_REWARD_OUTPUT_ANALYSIS.md`
- `SERVICE_REWARD_PRIVACY.md`
- `DUPLICATE_REGISTRATION_ANALYSIS.md`
- `DNS_RECORDS.md`

## Historical Snapshots

These documents describe earlier PR or testnet states. They remain useful for
audit history, but they are not current protocol documentation unless a section
explicitly says otherwise:

- `PR2_OVERVIEW_DE.md`
- `QWC_V2_CHANGE_REPORT_DE.md`
- `MAINNET_TESTPHASE.md`
- `TESTNET.md`
- `CONSOLIDATION_RFC.md`

## Documentation Inventory

`DOCUMENTATION_INVENTORY.md` records the current/historical classification used
for this synchronization pass.
