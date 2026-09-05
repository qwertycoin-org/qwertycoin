# Service Reward Privacy

## Current Status

Current `main` implements the governance-style reward design: EPoSE rewards are
normal CryptoNote one-time outputs derived from the coinbase transaction public
key and the selected service node's registered reward address.

This fixed the previous transparent-output blocker where the coinbase output
key was the reward wallet's long-term public spend key. Wallets can now detect
EPoSE rewards through normal scanning, and rewards are decomposed into standard
amount denominations so they can be spent after coinbase maturity with usable
decoy classes.

Status: `PARTIAL` for privacy.

The output mechanics are now wallet-compatible and avoid reusable long-term
output keys. However, the reward address and matching private view key are
intentionally disclosed in the service-node registration. Validators and
observers who track the registry can derive expected incoming reward outputs for
that dedicated reward wallet. Operators should therefore use a dedicated EPoSE
reward wallet and understand that incoming reward activity is observable.

## Current Design

The service node registers:

```text
service_public_key
reward_address = (reward_public_view_key A, reward_public_spend_key B)
reward_view_secret_key = a
endpoint_commitment
registration_epoch / expiry_epoch
admission proof
service-node signature
```

Registration parsing derives `A` from `a` and rejects the registration if the
derived public view key does not match the reward address. The reward spend key
is never disclosed and is not needed for validation.

For each service reward output:

```text
R = coinbase transaction public key = rG
derivation = rA
P = derive_public_key(derivation, output_index, B)
```

Validators recompute the same one-time output key with:

```text
derivation = aR
P_expected = derive_public_key(derivation, output_index, B)
```

The sorted set of matching output amounts must equal the standard denomination
decomposition of the expected 10% service reward.

## Security Properties

- The private spend key remains secret and is never part of consensus data.
- The disclosed private view key cannot spend funds.
- The reward output keys are normal one-time keys, not reusable long-term spend
  keys.
- Repeated rewards to the same reward wallet produce distinct output keys.
- `qwertycoin-wallet-cli` and compatible wallets can detect rewards through the
  normal output scan path.
- Coinbase validation rejects missing, wrong-recipient, wrong-amount,
  duplicate-output-key, overpay, and underpay rewards.

## Privacy Properties

- Public observers no longer see raw long-term spend keys as output keys.
- The registered reward address and private view key make incoming activity for
  the dedicated reward wallet observable to anyone who follows the registry.
- This is an explicit trade-off for deterministic validator verification.
- Operators should not reuse a personal wallet as the EPoSE reward wallet.

## Open Hardening

Further privacy work would require a separately reviewed design that lets
validators verify reward correctness without publishing enough information to
watch the reward wallet. That is out of scope for the current implementation and
should not be added as unaudited cryptography.

Current guidance:

- keep governance-style one-time outputs,
- require a dedicated reward wallet,
- never disclose the private spend key,
- document the observable-view-key trade-off clearly,
- treat stronger reward privacy as a future protocol research item.
