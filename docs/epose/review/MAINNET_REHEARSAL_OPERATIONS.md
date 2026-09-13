# HF17/EPoSE-v2 mainnet rehearsal operations

**Scope:** resettable pre-launch rehearsal on seed-00 through seed-03. This is
not a mainnet launch approval. The release gate evaluator remains authoritative.

## Fixed inventory

| Node | Host | QWC container | Chain volume | Identity volume |
|---|---|---|---|---|
| seed-00 | 95.216.221.239 | `qwertycoin-mainnet` | `qwertycoin-mainnet-v2-chain` | `qwertycoin-mainnet-v2-service-identity` |
| seed-01 | 202.61.202.161 | `qwertycoin-mainnet` | `qwertycoin-mainnet-v2-chain` | `qwertycoin-mainnet-v2-service-identity` |
| seed-02 | 159.195.216.239 | `qwertycoin-mainnet` | `qwertycoin-mainnet-chain` | `qwertycoin-mainnet-service-identity` |
| seed-03 | 159.195.194.92 | `qwertycoin-mainnet` | `qwertycoin-mainnet-v2-chain` | `qwertycoin-mainnet-v2-service-identity` |

The seed-00 explorer container is `qwertycoin-explorer`. Its chain input is
read-only and its derived index/cache, if present, is part of the reset
inventory. nginx and TLS material are host configuration and are preserved.
Unrelated containers, images, volumes, networks, files and services are outside
this procedure.

## Before the rehearsal reset

1. Record PR base/head, submodule identities, manifest and parameter digests,
   genesis hash, build flags, binary hashes, image ID and per-host Docker
   inspection output.
2. Record block count and tip height separately, top block hash, EPoSE state
   hash, peer set and wallet public addresses.
3. Stop mining and wallet RPC writers. Stop `qwertycoin-explorer`, then stop the
   four `qwertycoin-mainnet` containers.
4. Archive daemon logs, the explicit container/volume inventory and any chain
   data needed to reproduce an observed failure. Archives must not contain
   wallet seeds, private keys, keystore plaintext or RPC credentials.
5. Verify all preserved identity/wallet volumes by name before removing any
   chain-derived volume.

For the rehearsal start, remove only the four chain volumes listed above and
the explorer's derived index/cache volume if one exists. Recreate empty chain
volumes and deploy one byte-identical Linux candidate image. Generate a new v2
keystore on each host; an old v1 key file is not a v2 identity.

## Listener policy

- `8196/tcp`: public P2P.
- `8197/tcp`: unrestricted admin RPC, host-loopback only.
- `8198/tcp`: restricted public RPC for ordinary reads and bounded EPoSE-v2
  endpoint/challenge responses.
- `8199/tcp`: ZMQ RPC, host-loopback only.
- nginx `/qwc-rpc`: proxy only to restricted `127.0.0.1:8198`.
- nginx `/`: proxy to the explorer on `127.0.0.1:29982`.

The public restricted listener must return HTTP 404 for
`/submit_epose_envelope`; the loopback admin listener must register it. No
operator, wallet or service secret is accepted by the public v2 endpoints.

## Evidence sequence

SSH usernames and their host mapping are operational access metadata and are
intentionally not stored in this repository. Before invoking the remote
rehearsal helper, provide three absolute paths:

- `QWC_REHEARSAL_SSH_KEY`: a readable, non-symlink private-key file;
- `QWC_REHEARSAL_KNOWN_HOSTS`: a pinned, readable known-hosts file;
- `QWC_REHEARSAL_INVENTORY_FILE`: an owner-only JSON file outside the checkout
  (for example, mode 0600).

The inventory schema is:

```json
{
  "schema_version": 1,
  "nodes": [
    {
      "name": "node-a",
      "ssh_target": "operator@node-a.example.invalid",
      "public_endpoint": "node-a.example.invalid"
    },
    {
      "name": "node-b",
      "ssh_target": "operator@node-b.example.invalid",
      "public_endpoint": "node-b.example.invalid"
    },
    {
      "name": "node-c",
      "ssh_target": "operator@node-c.example.invalid",
      "public_endpoint": "node-c.example.invalid"
    },
    {
      "name": "node-d",
      "ssh_target": "operator@node-d.example.invalid",
      "public_endpoint": "node-d.example.invalid"
    }
  ]
}
```

Exactly four unique entries are required. The helper rejects extra fields,
unsafe characters, symlinked access files and group/world-readable inventory
files before opening a network connection. Run `validate-config` first; its
output reports only the node count and never prints SSH targets or paths. In a
future GitHub-hosted rehearsal, inject the inventory JSON through an encrypted
Actions secret and materialize it as a temporary mode-0600 file. Do not store
the mapping in repository variables or workflow logs.

Run `tests/epose/integration/remote_mainnet_rehearsal.sh` for inventory, RPC
separation, four-node convergence, restart and SIGKILL persistence. Archive its
JSON output at each milestone. Mine through the manifest-derived boundaries:
enrollment through 659, freeze at 660, service epoch 720-1379, qualification
close after 1379, seed at 1380 and first possible payout at 1440. Continue to
reward maturity, transfer to a second rehearsal wallet, recipient detection and
rescan. Then perform the recorded disconnect, partition/reorg, replay and load
scenarios. Do not lower difficulty, committee, signature or work limits.

## Final-launch reset

The final reset is performed only after rehearsal evidence and failure artifacts
are archived and a distinct final-launch genesis has been reviewed and pinned.
It is not sufficient to delete LMDB while retaining the rehearsal genesis.

1. Stop every miner, wallet writer, explorer/indexer and participating daemon.
2. Archive the final rehearsal identities listed above.
3. Remove only the inventoried QWC chain volumes, relay/transaction pool state,
   EPoSE state/undo/commitments stored with those chains, wallet caches and the
   explorer's derived index. Preserve wallet seed/key files and unrelated data.
4. Deploy the final source/config/image identity and distinct genesis on all
   nodes before reconnecting them.
5. Rebind or regenerate every v2 keystore for the final genesis/parameter
   domain. Retained operator secrets do not make an old bound keystore valid.
6. Recreate wallet caches from preserved wallet keys. Rehearsal balances,
   registrations, receipts and qualifications are not imported.
7. Verify identical final genesis bytes/hash on Linux and macOS, empty EPoSE
   state, normal sync/mining and rejection of rehearsal-chain blocks/records.

Every destructive command must resolve one explicit container or volume from
the inventory above. Broad paths, globs and unresolved variables are forbidden.
