# Qwertycoin Messenger security profile 2 (QMS2)

Status: normative implementation draft. QMS2 is an application protocol carried by
ordinary transactions. It changes no consensus, relay, fee, ring-size, or daemon rule.

## Immutable carrier rules

- outer transaction-extra tag: `TX_EXTRA_NONCE` (`0x02`)
- QMS nonce subtype: `0x72` (`0x70` and `0x71` remain EPoSE-only)
- nonce payload limit: 255 bytes
- unchanged transaction-extra relay limit: 1,060 bytes
- fragment data: at most 600 bytes
- fragment header: exactly 96 bytes
- nonce segment data: at most 248 bytes
- exactly one QMS fragment per carrier transaction
- final transaction-extra is parsed and checked after wallet construction

One full fragment is 696 bytes. Its canonical three nonce payloads are 255, 255,
and 207 bytes. Including each outer tag and canonical two-byte length varint, the
three fields consume 726 serialized transaction-extra bytes before other normal wallet
fields. Integrated addresses and user-supplied payment IDs are rejected for carriers.

## Version and profile

| Field | Value |
|---|---:|
| wire version | `2` |
| cryptographic profile | `2` (libsignal PQXDH + Triple Ratchet + XChaCha envelope) |
| maximum exact UTF-8 message | 4,096 bytes |
| maximum padded envelope | 9,600 bytes |

Profile 1 remains parseable only for explicit legacy migration. New contacts and new
sessions must never negotiate down to profile 1.

## Contact package

A contact package is a confidential capability transferred through an already trusted
channel. It contains:

- profile and network/genesis binding;
- a random 128-bit invitation identifier;
- libsignal registration and device identifiers;
- public identity key;
- one-time EC prekey;
- signed EC prekey and signature;
- ML-KEM-1024 prekey and signature;
- a fresh random 256-bit outer-root secret;
- an identity signature over the complete canonical package.

The receiver validates canonical encoding, version/profile, genesis, all signatures,
identifier uniqueness, and a user-confirmed identity fingerprint before import. Import
is idempotent. A consumed one-time prekey cannot be silently reused. Private identity,
EC, ML-KEM, and outer keys never leave the encrypted messenger store.

## Inner record and Triple Ratchet

The exact 4,096-byte limit applies to the user text. Before libsignal encryption, QMS
encodes a canonical inner record:

`QWC-QMS-INNER-V2 || wire || profile || genesis || invitation-id || session-id ||
direction || message-id || control-flags || [outer-offer] || [outer-ack] ||
content-type || text-length-le32 || exact-UTF8`.

The fixed metadata is 105 bytes including the domain and control-flags byte. An outer
offer adds an eight-byte epoch and 32-byte secret; an acknowledgement adds an
eight-byte epoch. Unknown flag bits, truncated fields, non-sequential offers, and
acknowledgements without the matching offer are rejected. The complete record is
encrypted by the pinned libsignal public API. PQXDH establishes the session; every
subsequent message uses libsignal's combined classical Double Ratchet and SPQR/ML-KEM
ratchet. QMS does not implement or modify those primitives.

`direction` is `0` for invitation-owner to importer and `1` for importer to owner.
Session and message identifiers are random 128-bit values. Address values supplied to
libsignal are deterministic, domain-separated encodings of the network, profile,
invitation, session, and direction; they are not wallet addresses.

## Confidential outer envelope

Three keys are derived from a random 256-bit contact-direction root with HKDF-SHA-256:

- `QWC-QMS2-ENVELOPE-KEY`
- `QWC-QMS2-DISCOVERY-KEY`
- `QWC-QMS2-FRAGMENT-MAC-KEY`

HKDF context binds network genesis, profile, invitation ID, session ID, and direction.
Keys are never reused across domains or directions.

The serialized libsignal ciphertext is wrapped with libsodium
XChaCha20-Poly1305-IETF using a fresh random 24-byte nonce. Its plaintext is
`ciphertext-length-le32 || ciphertext || random-padding`. Associated data is:

`QWC-QMS2-OUTER-AD || genesis || wire || profile || message-id || direction ||
padded-envelope-length-le32`.

The transmitted envelope is `nonce || AEAD-ciphertext-and-tag` and has exactly one of
these total sizes: 1,200, 2,400, 4,800, 7,200, or 9,600 bytes. Compression is forbidden.
The smallest fitting class is mandatory. A value that cannot fit 9,600 bytes is
rejected before any transaction is built or any ratchet state is committed.

The resulting carrier counts are exactly 2, 4, 8, 12, or 16 transactions. The review
screen shows that count, every fee, and the true total cost before authorization.

## Fragment header

All integers are little-endian. The fixed header is unchanged in shape:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | `QMS1` |
| 4 | 1 | wire version (`2`) |
| 5 | 1 | cryptographic profile (`2`) |
| 6 | 2 | flags (`0`) |
| 8 | 16 | random message ID |
| 24 | 2 | fragment index |
| 26 | 2 | fragment count |
| 28 | 4 | total padded envelope length |
| 32 | 16 | discovery hint |
| 48 | 32 | SHA-256 of complete padded envelope |
| 80 | 16 | fragment MAC |

The discovery hint is the first 16 bytes of HMAC-SHA-256 over a domain-separated
network/profile/direction/message context. The fragment MAC is the first 16 bytes of
HMAC-SHA-256 over a separate domain, header bytes 0..79, and the fragment data.

Only profile, timing, padding class, random linkage, and normal transaction metadata
remain public. Static contact identifiers, wallet addresses, session counters, and
message lengths are not placed in the QMS record.

## State and send transaction

Encryption is a state transition. For each contact, sends are serialized. The wallet:

1. clones the last committed session state;
2. encodes and encrypts once;
3. builds all carrier transactions using distinct mature inputs;
4. stores the post-encrypt ratchet state, padded ciphertext, complete signed transaction
   bytes, fee review, and send-plan status in one durable transaction;
5. only then allows broadcast.

Abort does not roll the ratchet back. Retry and restart reuse the identical ciphertext
and identical serialized transaction bytes. A partial broadcast resumes only remaining
stored transactions. Reorg changes confirmation state, never cryptographic state. No
repair operation may spend a new fee without a fresh user review.

The recipient commits the successful decrypt state, replay identifier, and optional
history record atomically. A failed authentication or incomplete message changes no
ratchet state.

Carrier transactions pay the sender's own standard address. The recipient requires no
output and no funds to receive the first message.

## Rotation and bounds

Each direction has one active context, optionally one offered/received context, and at
most one retiring context. After every 16 committed sends in one direction, the sender
creates the next sequential outer-secret epoch and includes the same authenticated
offer in every subsequent message until it is acknowledged. The receiver stores that
offer and immediately uses it for replies; the reply carries the authenticated
acknowledgement. Until acknowledgement, the offerer accepts both the active and offered
contexts. On acknowledgement, the offered context becomes active and the previous
active context becomes the single grace-period context. Completing the next rotation
overwrites that grace slot, which deletes the oldest secret. No control-only carrier is
created, and loss of an offer or acknowledgement does not delete a usable context.

Before persistence, receivers enforce global limits of 64 incomplete messages and
8 MiB reassembly data plus smaller per-contact limits. Duplicates are idempotent;
conflicting duplicates, non-canonical counts, unsupported flags, oversize values, and
cross-profile records are rejected.
