# QMS2 encrypted store and atomicity contract

Messenger state is independent of wallet spend/view keys and EPoSE identities. The
QWC seed does not restore it.

The encrypted store contains messenger identity/prekeys, contact packages, pinned
fingerprints, libsignal session records including SPQR state, outer-root contexts,
rotation state, replay identifiers, prepared send plans, and optionally plaintext
history. History is disabled by default and can be deleted separately.

At first setup the wallet creates a random 256-bit store key. It is wrapped by a key
derived from the user's messenger password with libsodium Argon2id using a random salt
and recorded parameters. Store records use XChaCha20-Poly1305-IETF with unique nonces
and domain-separated associated data. Wallet lock erases decrypted messenger keys and
plaintext from live application state. Secrets must not enter logs, CLI arguments,
temporary files, crash annotations, URLs, or analytics.

## Atomic send invariant

The cryptographic engine receives a snapshot and returns a candidate post-operation
snapshot. The caller commits that snapshot and the exact ciphertext/transaction plan in
one store transaction before broadcast is possible. If persistence fails, neither is
visible. If later construction fails, the produced ratchet step is retained as an
aborted local plan and never reused for another plaintext.

Concurrent sends for one contact are serialized by a per-contact lock. Different
contacts may proceed independently. Broadcast status is idempotent per stored
transaction hash. Restart, retry, partial broadcast, confirmation, and reorg only alter
delivery state; they never rewind a session.

## Restore invariant

An imported backup receives a new local instance identifier. If it contains messenger
state older than the most recent known epoch, sending is disabled until the contact
performs a fresh package/session rotation. Old send keys are never reused. A wallet
seed without the separately encrypted messenger backup restores no messenger data.
