# EPoSE v2 rewards

This document describes the reward path implemented by `reward_v2.cpp`,
`coordinator_v2.cpp`, `cryptonote_tx_utils.cpp`, and block validation.

## Allocation

The compiled service share is `1000` basis points (10%). It applies to the
scheduled subsidy only. Transaction fees remain entirely with the miner.

When a qualified payee exists:

```text
service_subsidy = floor(scheduled_subsidy * 1000 / 10000)
miner_subsidy   = scheduled_subsidy - service_subsidy
miner_fees      = transaction_fees
coinbase_total  = scheduled_subsidy + transaction_fees
```

When the source qualification set is empty, the compiled policy is
`miner_fallback`: the service allocation is zero and the miner receives the
full scheduled subsidy and all fees. No extra coins are created.

All arithmetic is checked. Overflow, an invalid basis-point value, an unset
empty-set policy, or a disagreement between the reward plan and Coinbase fails
block validation.

## Reward-source epoch

Epoch `0` is not reward eligible. Epoch `1` is the first service epoch. The
first payout height is the start of epoch `2`, height `1440` for the compiled
720-block epoch.

A block in payout epoch `E` reads only the qualification set closed for epoch
`E - 1`. Records in the payout block or its current epoch cannot alter that
source set.

## Payee selection

The qualified service public keys are canonically ordered. Selection is bound
to the qualification commitment, network, genesis, parameter set, payout
epoch, and canonical payout seed. Within the payout epoch, block position
rotates deterministically over the qualified set. Every honest node with the
same chain therefore derives the same payee.

The selected service public key is resolved back to the frozen source-epoch
member and its exact lifecycle descriptor. Core verifies the identity,
descriptor sequence, descriptor hash, reward binding, and service key before
constructing a payment expectation.

## Coinbase outputs

The reward address is a normal primary QWC address. Core creates normal
CryptoNote one-time outputs with the Coinbase transaction secret key and the
address's public view and spend keys. No wallet private view or spend key is
part of consensus, registration, validation, or node configuration.

The service amount is decomposed using the normal denomination rules. Each
output uses its real Coinbase output index. Repeated payments to the same
address therefore obtain distinct one-time output keys and can be detected and
spent by an ordinary Qwertycoin wallet after normal Coinbase maturity.

## Scoped payment proof

When a service payment is required, Coinbase contains exactly one version-1
service-payment-proof record. The proof commits to:

- network, genesis, and parameter set;
- block height and parent hash;
- payout epoch and qualification hash;
- selected service public key and reward address;
- exact service amount;
- Coinbase transaction public key;
- every claimed service output index, amount, and public key;
- a hash of canonical Coinbase bytes with only the payment-proof record
  removed.

Removing only the proof avoids a circular commitment while preserving all
unrelated Coinbase and EPoSE data. The proof cannot be transplanted to another
height, parent, payee, output allocation, or Coinbase transaction.

Block validation reconstructs the expected plan from the pre-block canonical
state, verifies the actual Coinbase total, validates the scoped proof, derives
the expected one-time outputs, and requires the exact service allocation. A
missing proof, extra proof, wrong recipient, wrong amount, duplicate output,
underpayment, overpayment, or modified commitment invalidates the block.

## Emission accounting

For the compiled `miner_fallback` policy, issued subsidy and emission advance
both equal scheduled subsidy. The code also models a versioned
`permanent_nonissuance` policy, but it is not the compiled mainnet policy.

Transaction fees never enter service subsidy or emission accounting.

## RPC views

`get_service_rewards` previews the reward-source epoch and expected payee for a
height. Its `qualified_service_public_keys` field is the closed source set; it
must not be replaced with the evolving current-epoch `qualified` flags.

`get_epose_block_reward` accepts a canonical block hash. Core replays the
historical reward plan and uses the production Coinbase verifier before it
returns a mapping. Unknown, alternative, inconsistent, or unverifiable blocks
fail closed. Consumers must not infer service attribution from output position
alone.

See [`RPC.md`](RPC.md) for request and exposure details.
