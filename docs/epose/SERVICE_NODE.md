# EPoSE Service Node

## Operator Model

A Qwertycoin EPoSE service node has:

- a genesis- and parameter-bound operator/service keystore,
- a stable identity derived from the operator key,
- a rotating online service key,
- a standard primary QWC reward address,
- a signed public endpoint descriptor,
- an epoch-scoped descriptor lifecycle and RandomX admission lease.

The keystore is not a wallet and contains no wallet spend or view key. It signs
EPoSE lifecycle, admission, endpoint, and service records only. Keep it
separate from the blockchain database, back it up securely, and do not share it.
The daemon needs only the public reward address; no wallet secret belongs in an
EPoSE configuration.

## Network And Host Requirements

The producer must run online on a fully synchronized, unpruned mainnet daemon.
The public endpoint host must be a canonical public IPv4, IPv6, or lowercase
DNS name. Loopback, private, link-local, multicast, unspecified, and malformed
hosts are rejected.

Default ports:

| Purpose | Port | Exposure |
| --- | ---: | --- |
| P2P | `8196` | Public |
| Unrestricted daemon RPC | `8197` | Local/private only |
| Restricted EPoSE probe RPC | `8198` | Public |
| ZMQ | `8199` | Local/private only |

Expose `8196/tcp` and `8198/tcp`. Never expose unrestricted RPC merely to make
EPoSE work. The restricted listener must not expose `/submit_epose_envelope`.

## Daemon CLI

```bash
qwertycoind \
  --epose-service \
  --epose-keystore /secure/path/epose-v2-keystore \
  --epose-reward-address QWC... \
  --epose-host node.example.org \
  --epose-port 8198 \
  --epose-discovery-endpoint http://seed-00.qwertycoin.org:8198 \
  --epose-discovery-endpoint http://seed-01.qwertycoin.org:8198 \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 8197 \
  --rpc-restricted-bind-ip 0.0.0.0 \
  --rpc-restricted-bind-port 8198 \
  --confirm-external-bind
```

Required EPoSE options:

- `--epose-service` enables the v2 producer.
- `--epose-keystore` loads or creates the protected operator/service
  keystore. A keystore is bound to the selected network genesis and parameter
  set; do not copy one from a different chain.
- `--epose-reward-address` sets the primary public QWC reward address.
- `--epose-host` and `--epose-port` describe the public
  restricted-RPC probe endpoint.
- `--epose-discovery-endpoint` bootstraps signed endpoint discovery and may
  be repeated. It is not an allowlist and does not grant admission.

`--confirm-external-bind` is required when the restricted listener binds to a
non-loopback address. The example intentionally keeps unrestricted RPC local.

## Automatic Enrollment Flow

After the daemon is synchronized, the producer:

1. creates or loads the bound keystore;
2. signs and relays its public endpoint descriptor;
3. builds the lifecycle record for the next eligible epoch;
4. performs the bounded RandomX admission search;
5. submits and relays the accepted lifecycle and admission envelope;
6. renews the descriptor and admission lease when required;
7. answers canonical service challenges and produces eligible receipts.

No funded registration transaction is required. The wallet command
`register_service_node`, the RPC `get_service_node_registration_payload`, and
the `--service-node`/`--service-node-key`/`--service-reward-view-key` options are
retired v1 compatibility surfaces. They are intentionally rejected and must not
be used for EPoSE.

## Deprecated Input Aliases

The earlier `--epose-v2-*` option names remain accepted as hidden migration
aliases for existing installations. They are not shown by `--help` and are not
emitted by maintained examples or wrappers. A process using any old name emits
one value-free migration warning. If old and canonical names at the same input
level disagree, the daemon exits before it can start EPoSE or create a
keystore. Exact duplicate values are applied once; repeated discovery endpoint
order is preserved.

The same compatibility window applies to the old
`QWC_EPOSE_V2_KEYSTORE_PATH`, `QWC_EPOSE_V2_ENDPOINT_HOST`,
`QWC_EPOSE_V2_ENDPOINT_PORT`, and `QWC_EPOSE_V2_DISCOVERY_ENDPOINTS`
environment variables. Maintained deployments use `QWC_EPOSE_KEYSTORE_PATH`,
`QWC_EPOSE_HOST`, `QWC_EPOSE_PORT`, and `QWC_EPOSE_DISCOVERY_ENDPOINTS`.
Removing the hidden aliases is a later, separately announced compatibility
change; it is not part of this release.

## Inspect Status

Use the local unrestricted RPC:

```bash
curl -s http://127.0.0.1:8197/get_info
curl -s http://127.0.0.1:8197/get_epose_info
curl -s http://127.0.0.1:8197/get_epose_service_endpoint_v2
```

`get_epose_info` distinguishes producer configuration from chain state:

- `local_service_node`: the v2 producer was requested;
- `local_service_node_key_loaded`: the bound keystore and endpoint passed local
  validation;
- `local_service_node_registered`: the identity descriptor is canonical;
- `local_service_node_active`: the descriptor is active for the current epoch;
- `local_service_node_qualified`: the service key is in the current qualified
  set.

The normal progression is `ready -> registered -> active -> qualified`, but it
is governed by chain height, enrollment cutoffs, the two-epoch warm-up,
admission proof, committee receipts, and current reachability. Starting the
producer does not guarantee qualification or an immediate reward. Epoch zero
has no EPoSE rewards.

## Docker Mainnet Deployment

The maintained deployment contract is in `deploy/mainnet/`:

```bash
docker compose --env-file deploy/mainnet/seed-00.env \
  -f deploy/mainnet/docker-compose.yml up -d
```

Hosts without the Docker Compose plugin can use:

```bash
./deploy/mainnet/start-qwertycoin-node.sh deploy/mainnet/seed-00.env
```

It separates chain data from service identity:

```text
QWC_DATA_VOLUME=...
QWC_IDENTITY_VOLUME=...
QWC_EPOSE_KEYSTORE_PATH=/service-node/epose-v2-keystore
```

Keep the identity volume during normal restarts or chain-database recovery.
Delete it only when intentionally creating a new identity. Never commit real
reward addresses, keystore contents, or private host configuration.

## Security And Recovery

- Back up the EPoSE keystore and wallet seed separately.
- Never place wallet private keys in node environment files.
- Keep unrestricted RPC and ZMQ private.
- Verify public DNS, firewall rules, and the restricted endpoint from an
  external network.
- Preserve the same keystore while a descriptor is active; the daemon rejects
  unexpected service-key, reward-address, or endpoint changes rather than
  redirecting rewards silently.
- Do not bypass admission, committee, epoch, or qualification rules during
  testing.
