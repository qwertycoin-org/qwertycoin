# QMS2 threat model

Status: implementation threat model. It is not an independent cryptographic audit.

## Security objectives

QMS2 aims to authenticate contacts and message context, provide classical forward
secrecy and post-compromise recovery through the Double Ratchet, continuously mix
ML-KEM material through SPQR, hide contact/session/message metadata inside a padded
outer envelope, prevent accidental direct network access in strict native mode, and
keep wallet funds deterministic under retries and failures.

It does not hide ordinary transaction timing, fees, ring members, or the fact that a
size class of QMS carrier records exists. It does not provide cover traffic, automatic
read receipts, invisible fees, or deletion from the blockchain.

## Blockchain observer and malicious wallet node

An observer can correlate carrier timing, padded class, fragment linkage, and normal
transaction metadata. Exact message length, static contact identifier, wallet address,
session counter, and plaintext are not present in QMS fields. A node/miner may omit,
delay, reorder, duplicate, or reorg carriers. Canonical fragment bounds, hash, HMAC,
message replay state, and atomic decrypt state make these denial-of-service actions,
not accepted plaintext changes.

Strict native mode routes every wallet-node request through user-supplied SOCKS5 to a
pinned Tor v3 onion endpoint with no direct fallback or clearnet DNS path. A malicious
onion node can still see and manipulate the requested chain data. The Web Wallet cannot
prove this transport property and is therefore fail-closed for online messenger actions
without a separately reviewed attestable transport.

## Tampered invitation or prekeys

The complete contact package is bound to network genesis, profile, invitation ID,
identity, EC prekeys, ML-KEM prekey, and outer secret by an identity signature. The
libsignal PQXDH path also verifies the signed EC and ML-KEM prekeys. A changed identity
requires explicit out-of-band fingerprint approval. Duplicate imports are idempotent;
conflicting reuse of an invitation or consumed one-time prekey is rejected.

Possession of the confidential contact package grants discovery and outer-envelope MAC
capability for that context. It does not grant the peer's private libsignal identity or
allow forging an inner Triple-Ratchet ciphertext. Conversely, compromising only an
outer key must not authenticate a forged inner message.

## Session compromise

Loss of current session state may expose messages protected by current chains. Future
security depends on a successful uncompromised Double-Ratchet/SPQR update; QMS makes no
stronger claim than the pinned libsignal algorithms. Retained plaintext history is a
separate exposure and is disabled by default. A contact-package compromise also leaks
its outer metadata context until rotation is confirmed.

QMS does not claim post-quantum authentication: PQXDH and SPQR add post-quantum key
agreement/ratcheting while identity authentication still has classical components.
The composite use and its context bindings require independent review.

## Stolen device or unlocked process

At rest, messenger state is encrypted under a random store key wrapped with an
Argon2id-derived key. Wallet lock erases live messenger keys and plaintext. This does
not protect a currently unlocked, compromised process, malicious operating system,
keylogger, swap/crash dump outside application control, or a weak user password.
Secrets are excluded from logs, CLI arguments, URLs, temporary files, and crash
metadata under application control.

Messenger identity is intentionally independent from wallet spend/view keys and EPoSE
keys. Compromise of one domain is not treated as authority in another.

## Old backups and rollback

Restoring an old messenger backup can clone or rewind ratchet state and risk key reuse.
Authentication failure, corruption, missing state, and an interrupted password change
are detected and fail closed. A complete rollback of all mutually consistent local
files or browser-storage records is not reliably detectable without an external
monotonic authority. After any uncertain, cloned, or stale restore, the operator must
explicitly reset Messenger state and exchange fresh contact packages with every peer.
A QWC wallet seed alone does not restore messenger keys. Reorg and application retry
never roll back cryptographic state.

The reset action deletes local identity, contact, session, rotation, prepared-plan,
replay, and history state. It does not erase old blockchain carriers or remotely revoke
a peer's stored copy. Continuing an old session after restoring stale state is outside
the supported recovery model.

## Active protocol manipulation and resource exhaustion

Unsupported versions/profiles/flags, wrong genesis/direction/session/message IDs,
non-canonical sizes, bad padding, invalid AEAD, bad fragment HMAC, changed hashes,
duplicate conflicts, and malformed libsignal messages are rejected before display.
Receivers cap incomplete messages globally at 64 and 8 MiB, with smaller per-contact
limits. Authentication is performed before optional plaintext persistence.

Unknown profile 2 records are never parsed as profile 1. EPoSE nonce subtypes `0x70`
and `0x71` remain separate from QMS `0x72`.

## Funds and transaction failures

Carrier preparation reserves distinct mature inputs for the whole batch and never
depends on unconfirmed change or automatic splitting. The review shows every carrier,
fee, and total. Ratchet state, ciphertext, and exact signed transaction bytes are
committed before broadcast. Restart and retry reuse those bytes; partial broadcast
resumes only unsent entries. No repair path spends another fee without renewed user
authorization.

Hardware-wallet and multisig support are excluded until their own atomicity model is
specified and tested.
