# EPoSE Service Node

## Operator Model

A Qwertycoin EPoSE-v2 service node has:

- a genesis- and parameter-bound operator/service keystore,
- a stable identity derived from the operator key,
- a rotating online service key,
- a standard primary QWC reward address,
- a signed public endpoint descriptor,
- an epoch-scoped descriptor lifecycle and RandomX admission lease.

The keystore is not a wallet and contains no wallet spend or view key. It signs
EPoSE-v2 lifecycle, admission, endpoint, and service records only. Keep it
separate from the blockchain database, back it up securely, and do not share it.
The daemon needs only the public reward address; no wallet secret belongs in an
EPoSE-v2 configuration.

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
  --epose-v2-service \
  --epose-v2-keystore /secure/path/epose-v2-keystore \
  --epose-v2-reward-address QWC... \
  --epose-v2-endpoint-host node.example.org \
  --epose-v2-endpoint-port 8198 \
  --epose-v2-discovery-endpoint http://seed-00.qwertycoin.org:8198 \
  --epose-v2-discovery-endpoint http://seed-01.qwertycoin.org:8198 \
  --epose-v2-discovery-endpoint http://seed-02.qwertycoin.org:8198 \
  --epose-v2-discovery-endpoint http://seed-03.qwertycoin.org:8198 \
  --p2p-bind-ip 0.0.0.0 \
  --p2p-bind-port 8196 \
  --rpc-bind-ip 127.0.0.1 \
  --rpc-bind-port 8197 \
  --rpc-restricted-bind-ip 0.0.0.0 \
  --rpc-restricted-bind-port 8198 \
  --confirm-external-bind
```

Required EPoSE-v2 options:

- `--epose-v2-service` enables the v2 producer.
- `--epose-v2-keystore` loads or creates the protected operator/service
  keystore. A keystore is bound to the selected network genesis and parameter
  set; do not copy one from a different chain.
- `--epose-v2-reward-address` sets the primary public QWC reward address.
- `--epose-v2-endpoint-host` and `--epose-v2-endpoint-port` describe the public
  restricted-RPC probe endpoint.
- `--epose-v2-discovery-endpoint` bootstraps signed endpoint discovery and may
  be repeated. It is not an allowlist and does not grant admission.

`--confirm-external-bind` is required when the restricted listener binds to a
non-loopback address. The example intentionally keeps unrestricted RPC local.

## Keystore File Security

The daemon validates the keystore as a file object before reading it. On Linux
and macOS it must be a regular, non-symlink file with no group or other access
(`0600`). On Windows it must be a regular, non-reparse, single-link file owned by the
executing account and have a protected DACL with no inherited entries. Allow
entries are accepted only for:

- the executing account, with read/write and access-control rights;
- `NT AUTHORITY\\SYSTEM` (optional); and
- the built-in Administrators group (optional).

The Windows file is created with this DACL before any key bytes are written.
An ACL that grants access to `Users`, `Authenticated Users`, `Everyone`, an
unrelated account, or an inherited principal is rejected. A missing, malformed,
or unreadable DACL is also rejected; Windows ACL validation is never skipped.

### Repair an older Windows keystore ACL

Do **not** delete the keystore: deletion creates a new service identity. Stop
the daemon, back up the existing file, then add this option to one start using
the otherwise unchanged EPoSE-v2 configuration:

```text
--epose-v2-repair-keystore-permissions
```

The option requires `--epose-v2-service` and repairs only the configured
keystore file DACL. It refuses files not owned by the executing account and
refuses reparse points or non-regular files. It neither rewrites the file nor
changes its parent directory. The normal loader then still verifies format,
network/genesis/parameter binding, checksum, distinct authorities and key
validity. After one successful start, confirm the same operator and service
public keys and remove the repair option from subsequent starts.

### Windows package impact

The fix is part of the Core library and `qwertycoind`. Publish a rebuilt
Windows Core/CLI archive from the fixed revision; treat the official archive
as one release unit even though the keystore path is daemon-only. The desktop
GUI repository pins this Core repository as a submodule and must update that
pin before rebuilding its Windows package. Wallet seed files and existing
EPoSE-v2 keystores are inputs to neither rebuild and must not be replaced.

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
be used for EPoSE v2.

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
has no EPoSE-v2 rewards.

See [`RPC.md`](RPC.md) for the complete method and exposure matrix.
Use [`DIAGNOSTICS.md`](DIAGNOSTICS.md) when a receipt round or final
qualification does not meet its chain-derived threshold.

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
QWC_EPOSE_V2_KEYSTORE_PATH=/service-node/epose-v2-keystore
```

Keep the identity volume during normal restarts or chain-database recovery.
Delete it only when intentionally creating a new identity. Never commit real
reward addresses, keystore contents, or private host configuration.

## Security And Recovery

- Back up the EPoSE-v2 keystore and wallet seed separately.
- Never place wallet private keys in node environment files.
- Keep unrestricted RPC and ZMQ private.
- Verify public DNS, firewall rules, and the restricted endpoint from an
  external network.
- Preserve the same keystore while a descriptor is active; the daemon rejects
  unexpected service-key, reward-address, or endpoint changes rather than
  redirecting rewards silently.
- Never delete a keystore to work around a permission error. Use the targeted
  one-file repair option above and verify that the public identity is unchanged.
- Do not bypass admission, committee, epoch, or qualification rules during
  testing.
