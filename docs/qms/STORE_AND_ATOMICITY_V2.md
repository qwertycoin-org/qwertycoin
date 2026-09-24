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

The encrypted envelope embeds its bounded wallet/network context and authenticates it
with both the key-wrap and data-encryption domains. A wallet-password change generates
a fresh Argon2id salt and wrap nonce and rewraps only the random store key; it does not
decrypt, rewrite, or rewind ratchet state. The wallet cache durably records a temporary
new-password recovery slot before changing the keys-file password. After a crash in the
middle of that transition, opening with the new password can recover from that slot.
Removing the wallet password is rejected while QMS2 state exists.

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

A QWC wallet seed without the separately encrypted messenger store restores no
messenger identity, contacts, sessions, or history. Importing an authenticated current
store preserves its exact ratchet state; silently rewinding or cloning that store can
reuse send state and is unsafe.

The application can detect authentication failure, corruption, missing QMS2 state, and
an interrupted wallet-password migration. It cannot reliably detect a complete
rollback in which an attacker restores every local file or browser-storage record to a
previous mutually consistent snapshot. No local instance counter can solve that
without an external monotonic authority.

After any uncertain, cloned, or stale restore, the operator must use the explicit
Messenger reset action (`qms_reset confirm` in the CLI, or the corresponding GUI/Web
control). Reset deletes the local messenger identity, contacts, ratchets, outer
contexts, prepared plans, replay state, and optional history. Both peers must exchange
fresh contact packages before sending again. Reset cannot revoke or erase historical
blockchain carriers and does not repair a peer that continues using the old session.
