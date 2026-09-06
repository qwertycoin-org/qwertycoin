# EPoSE Security Review

This directory records the source-audited baseline and decisions that must
precede a hardened EPoSE protocol revision. It is deliberately separate from
the current protocol description: finding a weakness in EPoSE v1 does not
authorize a node to reinterpret an already accepted block.

## Documents

- [`SOURCE_BASELINE.md`](SOURCE_BASELINE.md) identifies the reviewed source,
  upstream ancestry, active parameters, deployment observations, and reproduced
  test surface.
- [`FINDINGS.md`](FINDINGS.md) maps security-review findings F01 through F21 to
  concrete code and tests. `Open` means work is required; it does not by itself
  claim an exploitable vulnerability.
- [`ADR-0001-ACTIVATION-STATUS.md`](ADR-0001-ACTIVATION-STATUS.md) records the
  chain-preserving activation decision and the current no-go status for a new
  economic protocol.
- [`ADR-0002-HARDENED-PROTOCOL-RESERVATION.md`](ADR-0002-HARDENED-PROTOCOL-RESERVATION.md)
  reserves HF18, EPoSE v2, transaction-extra tag `0x05`, epoch ordering, and
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
benchmarked, specified, tested, reviewed, and approved. In particular, this
baseline does not change the 1,000-BPS service allocation, admission target,
committee size, quorum, reward fallback, or activation height.

CO-01 adds a normative transition, reservation manifest, and independent
boundary/envelope vectors without making reserved records valid. Runtime
implementation remains owned by CO-02 onward.
