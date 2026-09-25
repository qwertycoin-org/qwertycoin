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
| `get_epose_service_endpoint_v2` | yes | no | yes | Return a signed local endpoint descriptor. |
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
- `local_service_node_qualified`: the service key is in the current closed or
  visible qualification view.

Starting a producer does not make the later flags immediately true.

List canonical identities with the default or a smaller bound:

```bash
curl -s -X POST http://127.0.0.1:8197/get_service_nodes \
  -H 'Content-Type: application/json' \
  -d '{"limit":25}'
```

The response includes `total_count` and `returned_count`. Identity entries
contain public keys, public reward-address components, endpoint commitment,
descriptor sequence/epochs, and active/qualified flags. No secret key is
returned.

## Endpoint discovery and challenges

The restricted public listener may serve:

```bash
curl -s http://127.0.0.1:8198/get_epose_service_endpoint_v2
```

An optional `descriptor_hash` requests one exact signed descriptor. `ready` is
false when the local producer cannot serve the requested canonical descriptor.

`epose_service_challenge_v2` accepts the fully context-bound challenge fields
defined by `COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2`. The response contains the
canonical block blob and subject signature. The caller must independently
validate the block bytes and produce the selected-verifier signature; an HTTP
success response is not consensus evidence by itself.

Apply normal request-body, connection, concurrency, and rate limits at the edge.

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
