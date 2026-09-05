# Historical Governance Reward Analysis

## Old Mechanism

The historical Qwertycoin governance reward used a static governance wallet
address and a static governance private view key from `CryptoNoteConfig.h`.
Coinbase construction generated the normal miner transaction key pair and wrote
the transaction public key into `tx.extra`.

For the governance output, the miner transaction used the standard CryptoNote
recipient derivation:

```text
derivation = generate_key_derivation(governance_view_public_key, tx_secret_key)
output_key = derive_public_key(derivation, output_index, governance_spend_public_key)
```

Validation inverted the same relationship with the disclosed governance private
view key:

```text
derivation = generate_key_derivation(tx_public_key, governance_view_secret_key)
expected = derive_public_key(derivation, output_index, governance_spend_public_key)
```

The validator summed outputs whose key matched the derived governance output key
and required the sum to match the configured governance percentage.

## Security Properties

The private spend key was not embedded in consensus data, so the governance
wallet remained spend-protected. The private view key was public to validators,
which made incoming governance outputs observable but allowed deterministic
recipient validation.

## Privacy Properties

The model intentionally sacrificed incoming-payment privacy for the governance
wallet because every validator could derive and recognize governance outputs.
This was acceptable for a protocol-level public funding address, but it is not
appropriate for a mixed-use personal wallet.

## Wallet Compatibility

The output was wallet-compatible because it was a normal one-time output derived
from the transaction public key, output index, recipient public spend key, and
recipient public/private view key pair. No wallet-side special case was needed.

## What Can Be Reused Conceptually

EPoSE can reuse the validation idea:

```text
normal coinbase tx key
normal one-time output
disclosed view secret key
validator derives the expected output key
```

The dynamic selected EPoSE service node replaces the old static governance
address.

## What Must Change For Modern Monero/QWC

The implementation must use current Monero v0.18 transaction helpers, tagged
outputs when view tags are active, and the current coinbase transaction extra
parser. EPoSE registrations are bounded by `TX_EXTRA_NONCE_MAX_COUNT`, so the
wire format stores the reward spend public key and disclosed reward view secret
key; the reward view public key is derived while parsing.
