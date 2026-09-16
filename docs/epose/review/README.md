# EPoSE Security Review

> **Historical and evidentiary material.** This directory preserves the
> source-audited baselines, decisions, vectors, and release evidence produced
> during EPoSE v2 hardening. Its narrative files describe the source snapshot
> named in each document; they are not the current protocol specification.
> Use [`../PROTOCOL.md`](../PROTOCOL.md) and current source for implemented
> behavior.

The machine-readable release policy, evidence ledger, vectors, and results are
still consumed by repository tooling and therefore remain operational inputs.
Their retained verdicts apply to the release class and evidence snapshot they
name; they do not silently change consensus or invalidate later explicitly
approved public-test releases.

## Documents

- [`FINDINGS.md`](FINDINGS.md) maps security-review findings F01 through F21 to
  concrete code and tests. `Open` means work is required; it does not by itself
  claim an exploitable vulnerability.
- [`ADR-0001-ACTIVATION-STATUS.md`](ADR-0001-ACTIVATION-STATUS.md) preserves the
  superseded chain-preserving decision for audit history.
- [`ADR-0007-FRESH-GENESIS-ACTIVATION.md`](ADR-0007-FRESH-GENESIS-ACTIVATION.md)
  records the accepted public-mainnet target: QWC block version 17 and EPoSE v2 from a new
  genesis, with runtime activation still blocked by the release gates.
- [`ADR-0002-HARDENED-PROTOCOL-RESERVATION.md`](ADR-0002-HARDENED-PROTOCOL-RESERVATION.md)
  reserves QWC HF17, EPoSE v2, transaction-extra tag `0x05`, epoch ordering, and
  fail-closed activation rules.
- [`VALIDATION.md`](VALIDATION.md) records the baseline build and test commands,
  results, and explicit gaps.
- [`RELEASE_READINESS.md`](RELEASE_READINESS.md) records the CO-10/11 no-go
  assessment and the preconditions for a valid four-node funds-safety run.
- [`RELEASE_GATES_V2.json`](RELEASE_GATES_V2.json) is the machine-readable,
  fail-closed release evidence ledger.
- [`ADVERSARIAL_ACCEPTANCE_MATRIX.md`](ADVERSARIAL_ACCEPTANCE_MATRIX.md) maps
  the required attacks and failure modes to current and final evidence.

## Governing rule

RandomX remains responsible for block production, PoW security, and chain
selection. EPoSE is consensus-critical because its state affects coinbase
validity, but it is not a finality or chain-selection protocol.

Numerical proposals from a review are research candidates until separately
benchmarked, specified, tested, reviewed, and approved. The fresh-genesis
activation height is fixed at zero, but this baseline does not activate the
runtime path or choose the admission target, committee size, quorum, reward
fallback, or final resource limits.

CO-01 adds a normative transition, reservation manifest, and independent
boundary/envelope vectors without making reserved records valid. Runtime
implementation remains owned by CO-02 onward.

Historical operational host inventories and access notes are deliberately not
kept in the current public tree. Machine-readable evidence uses sanitized test
labels and contains no credentials or private operations mapping.
