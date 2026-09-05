# Service Reward Output Analysis

## Current Status

Current `main` no longer uses the transparent spend-public-key reward output
described below. PR #8 replaced it with governance-style one-time outputs,
validated through the disclosed reward private view key and decomposed into
standard amount digits for spendability. This document preserves the root-cause
analysis of the pre-fix bug and the design rationale for the replacement.

## Current Bug

PR #7 created EPoSE service rewards with a transparent output:

```text
output_key = reward_address.public_spend_key
```

That is consensus-visible, but it is not a normal CryptoNote one-time output.
`wallet2` scans coinbase outputs by deriving expected output keys from the
transaction public key, the wallet private view key, the output index, and the
wallet public spend key. A raw spend public key therefore does not match the
normal scan path and is not detected as an incoming wallet output.

## Collision Risk

Because the transparent output used the static reward spend public key, repeated
rewards to the same reward address reused the same output key. That breaks the
one-time-output invariant and can lead to downstream spend/key-image issues.

## Fixed Design

The miner transaction now appends the service reward through the same derivation
path as miner outputs:

```text
derivation = generate_key_derivation(reward_view_public_key, coinbase_tx_secret_key)
output_key = derive_public_key(derivation, service_output_index, reward_spend_public_key)
```

Validation uses the disclosed reward private view key from the registration:

```text
derivation = generate_key_derivation(coinbase_tx_public_key, reward_view_secret_key)
expected = derive_public_key(derivation, service_output_index, reward_spend_public_key)
```

The matching output amounts must equal the expected decomposed service reward
amount set, and no output public key may be repeated.

## Known Heights

The pre-fix internal mainnet-test chain contained rewards at heights such as
`90`, `93`, `96`, `99`, through `126`. Those outputs were created before this
fix and remain malformed. They should not be used as release evidence; a clean
chain reset is required after the fix passes end-to-end validation.
