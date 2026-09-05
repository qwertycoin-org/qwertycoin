# Service Node

## Operator Model

A Qwertycoin EPoSE service node has:

- a service private key,
- a service public key,
- a standard QWC reward address,
- the reward address private view key,
- an endpoint commitment,
- a registration epoch,
- an expiry epoch,
- an admission proof.

The service private key is not a wallet spend key. It signs registrations and
attestations only. Service-node identity is the service public key, not an IP
address and not the reward address.

The reward private view key is disclosed in the registration so validators can
verify governance-style one-time reward outputs. It can reveal incoming reward
activity for the dedicated reward wallet. It cannot spend funds. The reward
private spend key must remain secret.

## Daemon CLI

```bash
qwertycoind \
  --service-node \
  --service-node-key /service-node/service-node.key \
  --service-reward-address QWC... \
  --service-reward-view-key <matching private view key> \
  --service-node-advertise-address node.example:8196
```

Implemented behavior:

- `--service-node` enables local EPoSE service-node mode.
- `--service-node-key` loads or creates the service-node private key.
- If the key file does not exist, the daemon generates a service key and writes
  it with owner-only read/write permissions.
- Operators should store the service-node key outside the chain database, for
  example `/service-node/service-node.key`.
- `--service-reward-address` is required in service-node mode and must be a
  primary address for the selected network.
- `--service-reward-view-key` is required in service-node mode and must derive
  to the reward address public view key.
- `--service-node-advertise-address` is required and is committed via
  `endpoint_commitment`.
- Service-node mode currently requires an unpruned chain database.

Implemented inspection surfaces:

- `get_epose_info`,
- `get_service_nodes`,
- `get_service_node_status`,
- `get_epose_epoch`,
- `get_service_rewards`,
- `get_service_node_registration_payload`,
- daemon console `epose_status`,
- daemon console `prepare_service_node_registration`.

## Non-Mining Flow

The current flow does not require a service node to mine.

1. Start a synced daemon with service-node mode enabled.
2. Let the daemon create or load the service-node key.
3. Configure reward address and matching private view key.
4. Configure the public P2P endpoint.
5. Fetch or create the signed registration payload.
6. Submit the registration through a normal wallet transaction, or let a miner
   include a valid relayed registration payload.
7. Wait for deterministic verifier attestations.
8. Once enough valid attestations are on-chain for an epoch, the service node
   qualifies for the next reward-source epoch.
9. Rewards are paid as normal denominated one-time outputs to the reward wallet.

Normal nodes can relay EPoSE payloads via `NOTIFY_NEW_EPOSE_PAYLOADS`, and
normal miner daemons can include accepted relay-pool payloads in block
templates. A miner does not need a service-node key.

## Wallet Registration Helper

`qwertycoin-wallet-cli` can register a service node through the daemon it is
connected to:

```text
refresh
register_service_node
```

Without arguments, `register_service_node` creates a transaction that sends one
atomic unit back to the wallet's primary address and attaches the daemon's
signed EPoSE registration payload. To fund another address while registering:

```text
register_service_node <funding_address> <amount>
```

The wallet must be able to sign and relay transactions. Watch-only and multisig
wallets are rejected by the current wrapper. The daemon still exposes
`get_service_node_registration_payload` for diagnostics and low-level testing.

## Docker Mainnet Deployment

Use `deploy/mainnet/docker-compose.yml` with one host-specific env file:

```bash
docker compose --env-file deploy/mainnet/seed-00.env -f deploy/mainnet/docker-compose.yml up -d
```

Hosts without the Docker Compose plugin can use:

```bash
./deploy/mainnet/start-qwertycoin-node.sh deploy/mainnet/seed-00.env
```

The deployment separates chain data from service identity:

```text
QWC_DATA_VOLUME=...
QWC_IDENTITY_VOLUME=...
QWC_SERVICE_NODE_KEY_PATH=/service-node/service-node.key
```

For a normal chain reset, delete only the chain volume and keep the identity
volume. That preserves the same service public key. Delete the identity volume
only when intentionally creating a new service-node identity.

## Duplicate Registrations

Current `main` rejects overlapping active registrations for:

- the same service public key,
- the same endpoint commitment.

Endpoint-commitment uniqueness is an operational duplicate guard. It is not a
complete Sybil-resistance rule and must not be described as IP-based identity.

Currently implemented lifecycle:

```text
REGISTER -> ACTIVE -> EXPIRE
```

Explicit `RENEW`, `UPDATE`, and `DEREGISTER` semantics are open hardening work.
