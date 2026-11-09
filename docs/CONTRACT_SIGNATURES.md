# Contract call signatures

At the contracts fork, a `tx_extra_vm_call` carries ordered signer public keys, their contract ids, methods, 32-byte key ids, nested-call flags, and signatures. The vectors have equal length. VM execution collects the first occurrence of each `AddSig` public key, and a later nested request for the same key marks it as nested. Consensus compares the signer keys, contract ids, methods, and nested flags with the VM result before checking one signature per distinct key. Calls can request at most 16 distinct keys.

## Signed message

The message is `cn_fast_hash` of the following byte sequence:

| Field | Encoding |
| --- | --- |
| Domain | ASCII `qwc-contract-sig` followed by a zero byte |
| Network | One byte containing the network type |
| Signature scheme version | One byte containing the fixed contract signature scheme version (`1`), independent of the block hard fork |
| Called contract | 32-byte contract id; for deployment, the id derived from code and constructor arguments |
| Method | Unsigned 32-bit little-endian |
| Transaction fee | Unsigned 64-bit little-endian |
| Normalized prefix | 32-byte CryptoNote prefix hash with the call's signature vector cleared and extra fields sorted |
| RCT base | 32-byte hash of `serialize_rctsig_base`; the null hash for version 1 or null RCT transactions |

The normalized prefix binds inputs, outputs, call arguments, signer metadata, and all other prefix fields. The RCT base serialization binds the RCT type, fee, `ecdhInfo`, `outPk` commitments, and any base `pseudoOuts`. Contract transactions use the normalized prefix hash as the ring-signature message. Ring signatures do not cover the AddSig signatures: the wallet fills AddSig signatures after RCT construction. The transaction id commits to the complete prefix and signatures. Consensus requires canonical sorted extra for contract calls, so reordering extra cannot change the transaction id while preserving valid signatures. Changing an AddSig signature requires the derived private key holder to sign again. A reserve-proof signature cannot be reused as an AddSig signature because its message lacks the contract domain and context.

## Keys and authorization

The wallet derives each contract secret by reducing `qwc-contract-key\0 || spend_secret || contract_id || key_id` to a scalar and multiplying by the base point for the public key. The key id is 32 bytes chosen by the app. A watch-only wallet has no spend secret and cannot derive or sign a contract key. Master spend, view, and subaddress keys are never selected for AddSig.

An app shader calls `DerivePk(contract_id, key_id, output_public_key)` to obtain a registration key. The caller names the contract ids for the app invocation through `contract=` in simplewallet or `contract_ids` in `invoke_app_shader` RPC. The shader may derive keys for those ids. Another id requires a simplewallet approval prompt or an exact `approved_keys` entry in RPC naming the contract id, key id, and derived public key. Generated signed calls carry the public key and its derivation metadata. Wallet RPC `derive_contract_key` returns the same public key from a contract id and key id. This RPC method and `invoke_app_shader` are owner-only interfaces and are unavailable in restricted mode. `contract_call` accepts ordered `signers`, `signer_contracts`, `signer_methods`, and `signer_key_ids`; `tools/contracts.py derive` and repeated `--key-id` arguments provide the corresponding command-line flow. A zero key id is used when no key ids are supplied.

Before preparing a call, the wallet runs the VM itself against state proofs pinned to the state root in a block already in its chain. It compares the daemon's signer list with its own result and checks locally signed public keys against local derivation; external signers skip that derivation check. A top-level request for a key derived for the called contract and method proceeds without another signer prompt. A nested `CallFar` request or a request for another contract or method requires a per-key simplewallet prompt or an exact `approved_signers` entry in wallet RPC. Each approval names contract id, method, key id, public key, and whether the request is nested. An unsigned transaction presented for cold signing displays those fields; the signer verifies local derivation and requires the caller's confirmation for nested or foreign requests.

`simplewallet contract_call ... external_signers=<pubkey>[,...]` prints `Contract signature digest`, prompts `Contract signature for <pubkey>`, and relays only after all signatures verify. `contract_dry_run` rejects `external_signers`. RPC `contract_call` accepts `external_signers` only with `do_not_relay` and `get_tx_metadata`, returns `signature_hash`, and requires a spendable local wallet rather than a watch-only, multisig, or cold-signing device wallet. RPC `attach_contract_signatures` takes `{tx_metadata, signatures:[{public_key, signature}], do_not_relay, get_tx_hex}`. `do_not_relay=true` permits partial attachment; relay requires full verification, and `relay_tx` refuses incomplete calls. Invalid calls use `-53`, invalid hex uses `BAD_HEX`, and invalid metadata uses `BAD_TX_METADATA`. The wire `DIGEST` is 64 lowercase hex characters, `P` is a 64-character lowercase hex public key accepted by `crypto::check_key`, and `SIGNATURE` is 128 lowercase hex characters encoding `generate_signature(DIGEST, P, s)` as `c` then `r`.

Release, redeem, and refund methods should request the key authorized to receive funds through `AddSig`. `FundsUnlock` tracks an amount and does not select the transaction's output destination.
