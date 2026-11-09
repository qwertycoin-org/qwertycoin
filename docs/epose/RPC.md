# EPoSE daemon RPC

EPoSE RPC handlers are implemented in `src/rpc/core_rpc_server.cpp`, routed by
`src/rpc/core_rpc_server.h`, and serialized by
`src/rpc/core_rpc_server_commands_defs.h`.

Most read/service methods are available on the restricted listener. The only
EPoSE write/relay method, `submit_epose_envelope`, is explicitly unavailable
when `m_restricted` is true.

## Method matrix

| Method | HTTP path | JSON-RPC | Restricted RPC | Purpose |
| --- | --- | --- | --- | --- |
| `get_epose_info` | yes | yes | yes | Chain and local producer summary. |
| `get_epose_diagnostics` | yes | yes | **no** | Bounded local attempts and chain-derived qualification evidence. |
| `get_service_nodes` | yes | yes | yes | Bounded list of canonical descriptors. |
| `get_service_node_status` | yes | yes | yes | Lookup by service public key. |
| `get_service_node_registration_payload` | yes | yes | yes | Retired-v1 compatibility response; v2 enrollment is automatic. |
| `get_epose_epoch` | yes | yes | yes | Epoch boundaries and active/qualified counts. |
| `get_epose_service_endpoint_v2` | yes | no | yes | Return a signed local endpoint descriptor and optional display-only local telemetry. |
| `epose_service_challenge_v2` | yes | no | yes | Serve/sign the requested canonical block object. |
| `get_service_rewards` | yes | yes | yes | Preview source qualification and expected payee. |
| `get_epose_block_reward` | yes | yes | yes | Verify and map a canonical block's EPoSE reward. |
| `submit_epose_envelope` | yes | no | **no** | Admit and relay one encoded v2 envelope. |

The HTTP form is a JSON POST to `/<method>`. The JSON-RPC methods use
`/json_rpc`. The table follows the actual route macros; do not expose an
unrestricted listener merely to simplify monitoring.

## Status examples

Local operator status:

```bash
curl -s http://127.0.0.1:8197/get_epose_info
```

`get_epose_info` returns the active protocol version, current epoch and
boundaries, canonical identity/qualification counts, state hash, reward basis
points, and local producer flags. The local progression fields mean:

- `local_service_node`: `--epose-v2-service` was requested;
- `local_service_node_key_loaded`: the bound keystore and configured endpoint
  passed local initialization;
- `local_service_node_registered`: the identity has a canonical descriptor;
- `local_service_node_active`: the descriptor is effective in the current
  epoch;
- `local_service_node_qualified`: the service key is in the latest closed,
  retained qualification set identified by `qualification_epoch`.

`local_service_node_step_failed_count` is a restart-local producer failure
counter. RPC envelope submissions do not increment it.

`qualified_count` and `local_service_node_qualified` refer to
`qualification_epoch`: the current epoch after its evidence deadline,
otherwise the preceding epoch. `qualification_available` is false when no
closed qualification set is retained; in that case `qualification_epoch`,
`qualified_count`, and `local_service_node_qualified` are zero, zero, and false.
Every other `get_epose_info` field refers to the current epoch.

Under the contracts-version rolling window, `get_epose_epoch`, historical
reward preview, historical block-reward mapping, and `get_epose_diagnostics` return
`EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW` for a requested epoch older than the
retained floor.

For epochs whose membership bucket has folded, `get_epose_qualified_service_nodes`,
`get_epose_membership_snapshot_v2`, `get_epose_qualification_v2`, and
`get_epose_committee_v2` read the archived membership bucket while it is within
the ChainLock carrier retention window measured from the tip epoch. They return
the archived keys, snapshot,
qualification, or committee. After the bucket is evicted, the membership
getters return empty or unavailable. `get_epose_epoch` and
`get_epose_block_reward` use the status `Qualification pruned` when their
requested qualification exists but is pruned. Its qualification getter returns
unavailable and its qualified-node getter returns an empty vector. An epoch older than
the retained floor returns `EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW` from those
RPC handlers before qualification lookup.

Starting a producer does not make the later flags immediately true.

List canonical identities with the default or a smaller bound:

```bash
curl -s -X POST http://127.0.0.1:8197/get_service_nodes \
  -H 'Content-Type: application/json' \
  -d '{"limit":25}'
```

The response includes `total_count`, `returned_count`,
`qualification_epoch`, and `qualification_available`. Identity entries
contain public keys, public reward-address components, endpoint commitment,
descriptor sequence/epochs, and active/qualified flags. The active flag uses
the current epoch; the qualified flag uses `qualification_epoch`. No secret key is
returned.

`get_service_node_status` uses the same epoch rule for its qualified flag and
returns `qualification_epoch` and `qualification_available`. For
`get_epose_epoch`, an omitted or zero epoch
keeps the current epoch boundaries and active count while its qualified count
uses the latest closed qualification. An explicit epoch returns that epoch's
qualification set. Both responses include `qualification_epoch` and
`qualification_available`. The latter is false when no retained closed set
exists for a default request or when the requested explicit epoch has no
retained qualification set. An available set may contain zero qualified nodes.

`get_service_rewards` and `get_epose_block_reward` also include
`qualification_epoch`. Their qualified counts remain tied to the reward source
epoch for the requested height or canonical block.
`get_service_rewards` previews the next block height through the EPoSe-v2
coordinator reward plan for an empty block with zero transaction fees. `preview_available` is true
when the plan succeeds. `service_reward_active` and the expected public payee
keys reflect that plan. Historical or future heights have no preview.

## Endpoint discovery and challenges

The restricted public listener may serve:

```bash
curl -s http://127.0.0.1:8198/get_epose_service_endpoint_v2
```

An optional `descriptor_hash` requests one exact signed descriptor. `ready` is
false when the local producer cannot serve the requested canonical descriptor.
When that descriptor belongs to the responding local service node, the response
also sets `telemetry_available` and returns telemetry schema version 1,
`software_version`, `tip_height`, `tip_hash`, and `synchronized`. Cached remote
descriptors never receive local telemetry. The fields are informational
self-reports: they are not signed into the descriptor, do not change EPoSE
qualification or rewards, and must not be used as consensus evidence. Consumers
that show chain status must compare `tip_hash` with an independently observed
canonical block at the same `tip_height`.

Old nodes omit these optional fields and remain wire-compatible. Publishing the
restricted service endpoint makes the daemon release easier to fingerprint;
operators already publish that endpoint as part of EPoSE service discovery.

`epose_service_challenge_v2` accepts the fully context-bound challenge fields
defined by `COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2`. The response contains the
canonical block blob and subject signature. The caller must independently
validate the block bytes and produce the selected-verifier signature; an HTTP
success response is not consensus evidence by itself.

The challenge route remains available to P2P-banned hosts and has a per-address
bucket of eight requests that refills by one request per second. Other RPC
routes reject P2P-banned hosts. Apply normal request-body and connection limits
at the edge.

## Reward views

Preview a height:

```bash
curl -s -X POST http://127.0.0.1:8197/get_service_rewards \
  -H 'Content-Type: application/json' \
  -d '{"height":1440}'
```

The response distinguishes whether a preview is available and whether a
service reward is active. It returns the source epoch's exact qualified keys
and expected public reward-address components.

Map a stored canonical block:

```bash
curl -s -X POST http://127.0.0.1:8197/get_epose_block_reward \
  -H 'Content-Type: application/json' \
  -d '{"block_hash":"<64 lowercase hex characters>"}'
```

Core returns `mapping_available` only after reconstructing the historical
reward plan and reusing the production Coinbase payment verifier. The response
contains allocation fields and every verified service output. Unknown,
alternative, malformed, or inconsistent block hashes fail closed.

## Envelope submission

`submit_epose_envelope` accepts one hex-encoded canonical v2 envelope and may
admit it to the local bounded relay/template pool. It is deliberately
unavailable on restricted RPC:

```bash
curl -s -X POST http://127.0.0.1:8197/submit_epose_envelope \
  -H 'Content-Type: application/json' \
  -d '{"envelope":"<hex>"}'
```

Do not publish unrestricted RPC to expose this method. Service producers submit
locally and relay over authenticated/bounded protocol paths.

## Receipt diagnostics

`get_epose_diagnostics` is restricted to unrestricted RPC with `--rpc-login`
configured; otherwise it returns no diagnostic data. It is not routed on
restricted RPC or the public EPoSE probe listener. It returns
bounded, restart-local operational counters and recent terminal attempts plus
separately labelled canonical qualification evidence. `recent_limit` defaults
to 50 and is capped at 100. Optional `epoch` selects the chain-derived
qualification epoch; omitted means the current epoch.

Schema version 2 also reports the current relay queue item/byte totals and
bounded counters for exact redelivery duplicates, rejected signature variants,
expired round evidence, invalid canonical contexts, capacity rejections,
template-selected receipts, and canonical receipt observations. These are
restart-local operational counters; qualification remains derived from the
canonical chain anchor returned in the same response.

`relay_canonical_receipts` is an aggregate count of pending queue records
removed after canonical observation, not a per-node qualification count.
`rounds[].canonical_inclusions_observed` is grouped by epoch and round for the
local service identity. Use the chain-derived `canonical_unique_receipts`
array, together with the returned chain anchor, for qualification evidence.

See [`DIAGNOSTICS.md`](DIAGNOSTICS.md) for stable reason codes, example events,
retention/reset behavior, and the operator troubleshooting sequence.

## Exposure rules

- Bind unrestricted RPC to loopback or another operator-controlled interface
  that is not reachable from untrusted networks.
- Expose only the restricted probe listener (`8198`) for a service node.
- Keep `/submit_epose_envelope`, daemon administration, mining control, and
  other unrestricted methods private.
- Treat all RPC responses as observations. Block validation, not RPC, is the
  consensus authority.
- Never log or publish a keystore, wallet seed, wallet private key, or raw
  secret-bearing configuration. Current EPoSE RPC schemas contain public
  protocol data only.
`get_epose_diagnostics` also returns `state_bytes`, `retained_epochs`,
`oldest_retained_epoch`, `history_root`, `undo_entries`, `undo_bytes`, and
`last_snapshot_height`. The byte counts are estimates, and
`last_snapshot_height` is the newest persisted boundary image at or below the
canonical tip, or zero if absent.

## ChainLocked VM state proofs

`/get_chainlock` accepts `{"height": 0}` for the highest accepted lock carried
on the main chain, or a specific locked block height on unrestricted RPC.
Restricted RPC accepts only `height: 0`. It returns the locked
block's `block_id`, `block_hashing_blob`, `miner_tx_blob`,
`miner_tx_branch`, coinbase `vm_state_root`, serialized `lock_blob`,
`lock_carrier_height`, `quorum_epoch`, ordered `quorum` keys, and `threshold`.
`/get_chainlock_quorum_inputs` provides paginated membership and qualification
keys plus the epoch timing, anchor, and source block IDs needed to derive the
ordered quorum independently. See [the light-client derivation](../chainlock-light-client.md).
Pruned ChainLock membership can be reconstructed only through unrestricted RPC.
`/get_vm_state_proof` accepts a hex `key` and optional `height`.
`/get_vm_state_root` also accepts an optional `height`. For both calls,
an omitted height selects the tip, and an explicit `0` selects genesis.
Request and response heights are block indices.
Historical proof requests use the VM undo journal; heights older than the
retained journal return `Height outside VM undo retention`. Both calls are
available on restricted RPC.
Restricted `vm_call_dry_run` charges RPC credits in proportion to requested
gas and rejects calls above `QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP`.

A light client should hash the serialized miner transaction, fold it with
`miner_tx_branch` in miner position zero, and compare the result with the
Merkle root in `block_hashing_blob`. Hash the length-prefixed hashing blob to
recover `block_id`. Read tag `0x07` from the miner transaction and compare its
root with `vm_state_root`. Decode `lock_blob`, confirm its height, block hash,
and epoch match the response. Require the epoch to equal
`lock.height / epoch_length` by integer division using locally pinned network
parameters; fetch quorum inputs for that derived epoch. Compare all RPC timing,
network, genesis, and parameter commitments with the pinned values. Then
independently authenticate the ordered quorum
from the epoch membership and qualification snapshot, and verify exactly
`threshold` distinct CryptoNote signatures indexed by its signer bitmap over
`hash_chainlock_message_v1` using the network, genesis hash, and parameter set
hash. Authenticate the carrier block at `lock_carrier_height` and its coinbase
`0x09` field against `lock_blob`. Finally verify the VM proof from
`/get_vm_state_proof` against the locked `vm_state_root`. The RPC's quorum and
carrier height are discovery data; clients must authenticate them against their
trusted chain view.

`tools/chainlock.py --parameters network.json --height 0 --key <hex>` fetches both responses and checks
that their roots agree.
