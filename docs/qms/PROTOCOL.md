# Qwertycoin Messenger MVP protocol (QMS1)

Status: experimental application protocol. It does not change consensus, transaction
serialization, fees, ring size, or daemon behavior. Nodes see ordinary transactions
whose existing `TX_EXTRA_NONCE` fields contain opaque bytes.

## Baseline and compatibility

- Implementation baseline: Core `361b1fa003c749275a19ea7b889687c5a90379c9`.
- Paper baseline: Core `54308d8473dc5606d054c0ba428cfb2d64e758c1`.
- Existing limits remain unchanged: nonce payload 255 bytes and relay `tx_extra`
  1,060 bytes.
- Outer tag remains `TX_EXTRA_NONCE` (`0x02`).
- QMS uses inner subtype `0x72`. The paper's candidate `0x71` cannot be used:
  current Core already assigns it to EPoSE attestation records.

## Cryptographic profile 1

Profile 1 uses libsodium X25519/XSalsa20-Poly1305 sealed boxes and a separate
Ed25519 signature. The canonical inner record is signed and the record plus signature
is then sealed for the recipient. The signed record binds:

`QWC-QMS-SIGNED-MESSAGE-V1 || wire-version || profile || genesis || message-id ||
invitation-id || sender-fingerprint || recipient-fingerprint || content-type ||
text-length-le32 || exact UTF-8 text`.

Fingerprints are SHA-256 over a domain and both public keys. Invitations bind the
network genesis, random invitation ID, public encryption/signing keys and a random
32-byte discovery secret with a self-signature. Invitations are confidential contact
artifacts because the discovery secret grants inbox discovery and MAC capability.

HKDF-SHA-256 is implemented as RFC 5869 extract/expand using libsodium HMAC-SHA-256.
Separate info strings derive discovery and fragment-MAC keys. Context includes profile,
genesis, invitation ID and recipient fingerprint. The discovery hint is the first 16
bytes of HMAC-SHA-256 over `QWC-QMS-DISCOVERY-HINT-V1 || genesis || message-id`.

This profile does **not** provide forward secrecy, post-compromise ratchet recovery, or
post-quantum security. It is not an externally audited composite protocol. A stronger
future design requires a new application profile number, not a new blockchain tag.

## Fragment header

All integers are little-endian. The fixed 96-byte header is:

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | ASCII `QMS1` |
| 4 | 1 | wire version (`1`) |
| 5 | 1 | crypto profile (`1`) |
| 6 | 2 | flags (`0`) |
| 8 | 16 | random message ID |
| 24 | 2 | zero-based fragment index |
| 26 | 2 | fragment count |
| 28 | 4 | complete ciphertext size |
| 32 | 16 | discovery hint |
| 48 | 32 | SHA-256 of complete ciphertext |
| 80 | 16 | truncated fragment HMAC-SHA-256 |

The MAC input is `QWC-QMS-FRAGMENT-V1`, header bytes 0..79 and fragment bytes.
The complete ciphertext is split canonically into 600-byte pieces (last piece shorter),
with at most 16 fragments and 9,600 ciphertext bytes.

## Existing nonce carrier

Each serialized fragment is split canonically into pieces of at most 248 bytes. Each
nonce payload is `0x72 || QMS1 || segment-index-u8 || segment-count-u8 || piece`.
There are at most three such nonce fields in one transaction. The normal extra
serializer adds the unchanged outer `0x02` tag and canonical length varint. One QMS
fragment is permitted per carrier transaction.

After normal wallet construction, callers must parse the final transaction and compare
the extracted fragment byte-for-byte with the planned fragment. Integrated addresses
and user payment IDs are unsupported for QMS carrier transactions.

