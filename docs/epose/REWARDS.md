# EPoSE Rewards

## Current Formula

```text
total_reward = base_reward + transaction_fees
service_reward = floor(total_reward * EPOSE_SERVICE_REWARD_BPS / 10000)
miner_reward = total_reward - service_reward
```

Current `main` value:

```text
EPOSE_SERVICE_REWARD_BPS = 1000
```

This means 10% service reward and 90% miner reward in the current controlled
mainnet validation chain.

## Epoch Source

Service rewards in epoch `E + 1` are paid only from the finalized qualified set of epoch `E`.

```text
registrations / challenges / attestations in E
=> qualification snapshot E
=> rewards in E + 1
```

This means current-epoch attestation ordering, inclusion, or omission cannot change the current block's payee. Reorgs before finalization recompute the source epoch from the canonical chain; after convergence, the same canonical chain must produce the same qualified set and reward view.

## Fee Split Status

The current implementation computes the service share from the amount passed
into coinbase validation/template generation. In the current code path this is:

```text
service_reward = floor((base_reward + transaction_fees) * bps / 10000)
```

This is Model B below. It is acceptable for controlled mainnet validation
because it stresses coinbase validation, reward rounding, and service-node
payment paths under one simple rule. It is not a final public mainnet tokenomics
decision.

For mainnet, two candidates remain open and must be selected explicitly before
activation:

| Model | Subsidy | Fees | Assessment |
| --- | --- | --- | --- |
| A | 90% miner / 10% service node | 100% miner | Better miner incentive during fee spikes and simpler fee-market reasoning. Service-node income tracks tail/subsidy only. |
| B | 90% miner / 10% service node | 90% miner / 10% service node | Better service-node upside during high usage, but takes part of the fee security budget from miners and can make high-fee blocks more contentious. |

### Model A: Split Subsidy Only

```text
service_reward = floor(base_reward * bps / 10000)
miner_reward = base_reward - service_reward + transaction_fees
```

Security properties:

- transaction inclusion incentives remain fully miner-owned,
- high-fee blocks do not increase service-node reward capture,
- fee-sniping incentives stay closest to the upstream miner-only fee model,
- service-node budget becomes predictable from emission alone.

Risks:

- service-node income may be too low if tail/subsidy is low and operating costs
  are high,
- fee-heavy future usage would not automatically improve service-node rewards.

### Model B: Split Subsidy And Fees

```text
service_reward = floor((base_reward + transaction_fees) * bps / 10000)
miner_reward = base_reward + transaction_fees - service_reward
```

Security properties:

- service-node rewards grow with actual chain usage,
- the rule is simple and already implemented in the current validation code
  path.

Risks:

- a portion of the fee security budget is redirected away from miners,
- high-fee blocks become more valuable to the selected service-node payee,
- fee-market analysis has to account for two recipient classes,
- reward manipulation tests must cover high-fee blocks and rounding near
  boundary values.

### Technical Recommendation

For mainnet review, prefer Model A unless measured service-node operating costs
show that subsidy-only rewards are insufficient. Model A keeps miner fee
incentives simpler and makes service-node rewards independent of short-term fee
spikes.

Keep Model B as the current validation behavior because it exercises the
broadest coinbase-validation path and makes reward bugs easier to observe
during high-turnover testing.

Before final public mainnet activation, add a consensus switch or hardfork-gated
rule if the chosen mainnet model differs from the current implementation.

## Tokenomics Status

This is not a final public mainnet tokenomics decision. Final values require
explicit approval because they affect emission, incentives, and possibly
legacy-token expectations.

## Bounded Coinbase

EPoSE does not pay every service node in every block. One qualified node is selected per block using deterministic rotation over an epoch-seeded ranking. This prevents Coinbase output count from growing linearly with the service-node set.

## Governance-Style Coinbase Outputs

EPoSE service rewards are normal one-time CryptoNote outputs. The miner
transaction creates its usual transaction key pair:

```text
r = coinbase tx secret key
R = rG = coinbase tx public key
```

For the selected service-node reward address:

```text
A = reward public view key
B = reward public spend key
```

the service output key is:

```text
derivation = rA
P = derive_public_key(derivation, output_index, B)
```

The service reward amount is decomposed into standard amount digits before
outputs are added to the coinbase transaction. This is deliberate: paying the
whole 10% as one large non-RCT amount creates amount classes with too few
decoys and can leave wallet coin selection unable to spend an otherwise
unlocked reward. Denominated outputs repeat across blocks and are the spendable
CryptoNote-compatible shape.

Every denominated output uses the real output index in the coinbase
transaction. Repeated rewards to the same reward wallet therefore receive
different output keys because every coinbase transaction has a fresh transaction
key and each output position is part of the derivation.

## Coinbase Validation

When a qualified payee exists, block validation reconstructs the expected payee
and service amount from canonical chain state. The registration discloses the
reward wallet private view key `a`, while the private spend key remains secret.
Validators read `R` from the coinbase transaction extra, calculate:

```text
derivation = aR
P_expected = derive_public_key(derivation, output_index, B)
```

and collect all outputs that match the derived keys for that payee. The sorted
matching amounts must exactly equal the denomination decomposition of the
expected service reward. Missing, wrong-recipient, wrong-amount, wrong-view-key,
duplicate-output-key, overpay, or underpay service rewards are invalid.

For presentation, explorers may group these denominated outputs as one logical
EPoSE service reward. That grouping is UI-only; on-chain there are multiple
standard outputs when the service amount has multiple non-zero digits.
