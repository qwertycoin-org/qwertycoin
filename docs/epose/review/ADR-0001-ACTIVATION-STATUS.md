# ADR-0001: EPoSE Hardened Activation Status

- **Status:** Accepted baseline decision
- **Date:** 2026-09-06
- **Scope:** Planning and compatibility; no consensus change
- **Baseline:** `1c4c1bf10c387887a42243dc690a65abb6c6e786`

## Context

QWC code schedules HF17 and EPoSE v1 from height 0. Read-only deployment
inspection found a live HF17 chain with nonzero height and EPoSE registrations,
but the running source suffix is not reproducible from the public baseline.
The security review also identified P0 gaps, most importantly automatic
verifier-generated positive attestations without subject participation.

Changing current constants or interpreting old records under new rules would
risk rejecting accepted history, splitting nodes, or invalidating balances.
Resetting genesis is irreversible and is neither required nor authorized by
this review.

## Decision

1. Treat all existing mainnet blocks as potentially carrying real balances.
2. Preserve the historical HF17/EPoSE-v1 interpretation exactly while it is
   active. Do not retrofit hardened receipt, membership, reward, or encoding
   rules onto historical blocks.
3. The hardened economic protocol is **not approved for activation**. Existing
   EPoSE behavior may be exercised only as explicitly controlled pre-release
   infrastructure until the release gates are met.
4. Define hardened behavior in CO-01 as a new protocol/hardfork version with a
   future, chain-preserving activation height and an epoch-aligned warm-up.
   The version number and activation height are intentionally unassigned.
5. Before activation, reconcile the deployed source and public source, publish
   a signed release manifest, and prove that old history validates identically.
6. Preserve RandomX as the only chain-selection and block-production mechanism.
   EPoSE must never add finality votes, DNS dependencies, live probes, external
   APIs, or privileged operator switches to block validation.
7. Keep the service allocation at 1,000 BPS unless Alex separately approves a
   tokenomics ADR. Fee splitting, empty-set fallback, and emission accounting
   require explicit economic approval and golden vectors.
8. Do not introduce new cryptography for payment verification without an
   independent design review and compatibility proof against the pinned code.

## Activation gates

A future activation proposal must include all of the following:

- a complete normative transition and parameter manifest;
- frozen pre-seed membership and a settled pre-payout qualification set;
- subject-authenticated, temporally bound, independently checkable service
  receipts;
- a versioned bounded envelope and measured throughput at the supported
  population;
- approved admission, committee, quorum, verifier-duty, and inclusion-risk
  models;
- approved reward/fallback/emission semantics and independently reviewed scoped
  payment verification;
- deterministic lifecycle, persistent state, restart, pruning, crash, and deep
  reorg behavior equal to fresh replay;
- enrollment-to-matured-spend-to-recipient E2E evidence using worthless wallets;
- reproducible binaries tied to source/submodules/toolchain and independent
  review with no unresolved P0 findings.

## Consequences

- CO-00 and documentation improvements can merge without changing consensus.
- Implementation proceeds as reviewable packages rather than one monolithic PR.
- A passing unit suite alone cannot authorize activation or deployment.
- Current terminology using “finality” should be migrated to “anchor” or
  “settlement” in the future version; historical symbols may remain for binary
  compatibility until a separately reviewed cleanup.
- If no parameter set meets both security and affordable-operator constraints,
  the correct outcome is an explicit no-go, not silent quorum reduction.

## Rejected alternatives

### Reinterpret HF17 from height 0

Rejected because nodes with old and new binaries could disagree about accepted
history, qualification, and coinbase validity.

### Reset genesis and balances

Rejected because a live-chain reset is irreversible, unnecessary for CO-00,
and not authorized.

### Change committee, admission, or reward constants immediately

Rejected because the review requires capacity, adversarial, economic, and
funds-safety evidence before selecting constants.

### Add only a subject signature to the legacy nonce

Rejected because it exceeds the current envelope budget and still would not by
itself prove continuous service, operator independence, or data correctness.
