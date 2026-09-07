# ADR-0003: EPoSE v2 Reward Semantics and Open Economic Decision

- **Status:** Candidate implementation; economic decision required
- **Date:** 2026-09-06
- **Scope:** CO-06, non-activating

## Context

HF17 calculates the service share from subsidy plus fees, returns the service
allocation to the miner when no payee exists, and validates outputs with a
private reward view key published in each registration. Retrofitting different
rules onto HF17 would invalidate history. A v2 rule requires separate,
versioned activation.

The security review recommends a subsidy-only service allocation and no miner
windfall when the qualified set is empty. Permanent non-issuance affects
tokenomics, emission accounting, and the PoW security budget, so it cannot be
selected by an implementation PR.

## Candidate implemented by CO-06

`reward_v2.*` provides a non-activating reference implementation with these
properties:

1. The service allocation is `floor(scheduled_subsidy * 1000 / 10000)`.
2. Transaction fees always belong to the miner.
3. A qualified payee receives the service allocation; the miner receives the
   remaining subsidy and all fees.
4. With no qualified payee, the caller must explicitly choose either historical
   miner fallback or permanent non-issuance. `unset` fails closed.
5. Permanent non-issuance advances scheduled emission by the full scheduled
   subsidy while issued supply excludes the withheld amount. Integration must
   prove that inherited emission code cannot reissue it later.
6. The service share remains 1,000 BPS. This PR does not authorize changing it.
7. Payout rotation is relative to the payout epoch start and binds genesis,
   parameter set, closed qualification hash, source/payout epochs, payout seed,
   and service key.

No option is connected to block construction or validation in this PR.

## Scoped payment proof candidate

The candidate uses the inherited transaction-proof primitive. The miner
publishes the recipient-specific derivation `D = 8*r*A` and proves knowledge of
the coinbase secret `r` binding `R = r*G` and `D = r*(8*A)`. The transcript also
commits to network/genesis, parameter set, height, parent, payout epoch, closed
qualification set, service identity, public reward keys, amount, transaction
public key, explicit output indices/amounts/keys, and a canonical coinbase
commitment calculated with the proof record omitted.

Validators derive the prescribed one-time output keys from `D` and the public
spend key. A private view key is neither published nor consumed. Proof bytes are
excluded from the coinbase commitment, avoiding a circular transcript.

This construction is limited to primary standard reward addresses. It remains
a candidate until an independent cryptographic review covers canonical point
encoding, small-order/cofactor behavior, transcript completeness, output
allocation, and inherited proof assumptions. It must not be activated merely
because its tests pass.

## Decision required before activation

Alex must explicitly select one empty-set policy:

- **A — miner fallback:** preserve the scheduled subsidy and PoW revenue, but
  retain the direct incentive to suppress all service qualification.
- **B — permanent non-issuance (recommended by the review):** remove the
  empty-set miner windfall, but reduce miner revenue and require reviewed
  emission-continuity accounting.

The activation ADR must also approve subsidy-only fee treatment and cite the
independent payment-proof review. Until then the v2 manifest remains
`not-activatable` and the policy remains unset.

## Compatibility

HF17 reward calculation, registrations, output validation, existing balances,
and history are unchanged.
