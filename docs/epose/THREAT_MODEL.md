# EPoSE v2 threat model

## Security objective

EPoSE should reward service identities that provide the implemented canonical
object service without weakening RandomX chain security, changing supply,
exposing wallet secrets, or allowing local/off-chain observations to decide
consensus.

Protected assets include:

- canonical chain and deterministic EPoSE state;
- scheduled subsidy and service-reward recipient;
- operator and online service authorities;
- wallet spend/view keys and reward funds;
- daemon CPU, memory, disk, and network capacity;
- reorg-safe persistent state.

## Trust boundaries

Untrusted inputs include blocks, transactions, tx-extra bytes, P2P relay
messages, endpoint descriptors, DNS answers, service responses, RPC requests,
and peer timing. Explorer, monitoring, discovery endpoints, and relay queues are
not consensus authorities.

The offline operator key authorizes identity lifecycle. The online service key
answers challenges and participates as verifier. A normal QWC reward address
receives funds; its wallet secrets are outside the service node.

## Threats and implemented mitigations

### Free identity/Sybil creation

Admission requires epoch- and context-bound RandomX work at the compiled
18-leading-zero-bit target. Membership is capped at 100 identities, leases are
target-epoch scoped, service keys are unique, and admission verification is
bounded per envelope and block.

Residual risk: 18 bits is a public-test launch parameter, not proof that one
economic operator controls only one identity. Optimized hardware and parallel
search reduce admission cost. A future change requires measurement and an
explicit parameter activation.

### Committee collusion and false availability

Verifier committees are selected deterministically from a frozen snapshot.
Receipts require independent subject and selected-verifier signatures and are
bound to a canonical block-object challenge. Quorum is two thirds of the
actual committee and two of three rounds must pass.

Residual risk: colluding identities can attest to each other. Small bootstrap
populations provide materially less operator diversity than a full nine-member
committee. EPoSE proves the implemented signed canonical-object exchange, not
legal identity, dedicated hardware, continuous uptime, latency SLA, or
geographic independence.

### Miner manipulation and censorship

Reward eligibility comes from a qualification set closed before its payout
epoch. A miner cannot choose the current block's payee or fabricate a receipt
without the required keys. Full nodes validate the expected payment.

Residual risk: miners can censor otherwise valid enrollment/evidence records
from their templates. Reserved relay/template capacity reduces accidental
starvation but cannot force an adversarial miner to include a record. Continued
network-wide censorship can prevent qualification; it cannot redirect a
qualified identity's payment.

### Replay and cross-context substitution

Hashes and signatures bind the network, genesis, parameter set, epoch,
snapshot, anchors, roles, endpoint, nonce, and record-specific context.
Lifecycle sequence numbers and semantic slot keys reject stale conflicts.

An object valid on another chain, genesis, parameter set, epoch, round,
identity, or endpoint is invalid here.

### Malformed-input and resource exhaustion

Canonical envelope parsing rejects overlong/overflowing varints, malformed
lengths, unknown types, unsupported versions, nonzero flags, trailing bytes,
and context-invalid records. Per-envelope and per-block byte, record,
signature, and RandomX budgets are charged before expensive verification or
duplicate elimination. P2P batches, endpoint cache, DNS resolution, probe
concurrency, relay queue, RPC pages, and template selection are bounded.

Residual risk: limits bound one validation path but do not remove ordinary
network volumetric DoS. Operators still need firewall, connection, and RPC
rate limits.

### Endpoint and DNS attacks

Descriptors are signed and their hashes are committed by lifecycle state.
Hosts must be canonical and public; private, loopback, link-local, multicast,
mapped, or malformed targets are rejected. Resolution is bounded and targets
are revalidated before probing. Consensus validation never resolves DNS.

Residual risk: DNS and routing remain availability dependencies for live
probing. A valid signature proves descriptor authorization, not control of the
network path at every instant.

### Key compromise

The keystore separates the stable operator authority from the online service
key and is bound to the chain/profile. Lifecycle recovery can rotate the
service key without changing identity. POSIX mode and Windows owner/DACL checks
reject broadly readable, inherited, symlink/reparse, and wrong-owner files.

Residual risk: compromise of the operator authority can authorize lifecycle
changes. Compromise of the online service key can answer/sign service records
until recovery takes effect. Operators must back up the keystore securely and
protect the host; there is no remote revocation service.

### Wallet-funds compromise

The daemon receives only a public primary reward address. The EPoSE keystore
contains no wallet secret. Coinbase uses normal one-time outputs and a scoped
payment proof validated by every full node.

Residual risk: public lifecycle/reward data can link a service identity,
endpoint, and reward address. Use of a dedicated reward wallet limits linkage
to other wallet activity but does not make service rewards private.

### Reorg and state corruption

Transitions are fail-atomic, recent disconnects use bounded undo, and deeper
recovery replays canonical blocks. LMDB commitments bind the derived state to
the same database transaction as the block. Genesis/profile mismatches and
missing or corrupt commitments fail closed.

Residual risk: deep replay is operationally expensive. Pruned validation is
declared unsupported/fail-closed for this profile; service producers should run
an unpruned daemon.

## Explicit non-goals

EPoSE v2 does not prove:

- that one identity equals one human or one machine;
- permanent uptime or a latency/bandwidth SLA;
- geographic, ASN, hosting-provider, or operator independence;
- immunity to majority mining censorship;
- privacy of service endpoint or reward-address relationships;
- protection of an operator key after host compromise.

Any future service kind, slashing rule, collateral system, privacy scheme, or
new admission economy requires a separately versioned design and threat model.
