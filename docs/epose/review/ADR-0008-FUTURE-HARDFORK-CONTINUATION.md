# ADR-0008: EPoSE-v2 state continuation across future QWC hardforks

- **Status:** Accepted compatibility rule; no future fork scheduled
- **Date:** 2026-09-11
- **Scope:** Post-launch QWC block versions and existing EPoSE-v2 state

## Context

QWC launches EPoSE protocol version 2 with block major version 17.  Block and
EPoSE protocol versions are separate domains.  Treating block version 17 as an
exact feature flag would silently disable EPoSE record parsing, state replay,
block-template rewards, and coinbase validation at the first later QWC
hardfork.

## Decision

1. QWC block versions below 17 never activate EPoSE v2.
2. Version 17 activates the parameter- and genesis-bound EPoSE-v2 state
   machine at genesis.
3. A later block version continues the same EPoSE-v2 state, parameter-set hash,
   commitment schema, registrations, snapshots, receipts, qualifications, and
   reward history unless that hardfork explicitly specifies a replacement
   migration.
4. Merely scheduling a new QWC block version does not change EPoSE economics or
   reinterpret existing records.
5. A future parameter or schema change requires a separate ADR, a new committed
   parameter-set/schema identifier, deterministic activation-height migration,
   rollback rules, replay tests, and release evidence.
6. No block version 18 or other future version is scheduled by this ADR.
   Normal hardfork validation continues to reject unscheduled versions.

## Required invariants

- Replaying a canonical sequence that crosses 17 to a future scheduled version
  reconstructs every pre- and post-boundary state commitment without reset.
- Disconnecting and reconnecting the first future-version block restores and
  reproduces the exact parent and child state hashes.
- The future-version block uses the same bounded envelope parser, coordinator,
  reward plan, payment proof, and resource limits until an explicit migration
  changes them.
- A block whose major version differs from the hardfork schedule is rejected;
  accepting EPoSE v2 in a primitive does not bypass the chain's version rule.

## Consequences

This provides a safe no-op upgrade path for operational or unrelated consensus
hardforks.  It does not authorize post-launch parameter changes and cannot undo
historical payouts or state commitments.  Such changes remain forward-only,
explicit migrations.

