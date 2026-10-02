# EPoSE relay-policy rollout at height 21,600

## Scope

Height `21,600` activates local receipt relay and mining-template hardening in
updated nodes. It does **not** activate a new hard-fork version. Mainnet block
version `17`, receipt wire format, canonical validation, parameter-set hash,
state commitment, RandomX chain selection, quorum (`6/9` in two of three
rounds), rewards and wallet rules remain unchanged.

The fixed height is the first block of service epoch `30`. Updated and older
daemons accept the same valid blocks before, at and after the boundary. Old
relay or template nodes can still delay receipt delivery, so rollout coverage
affects liveness even though it cannot create a block-validity split.

## Required rollout order

1. Upgrade every known mining-pool and solo-miner block-template daemon.
2. Upgrade the relay paths between service verifiers and those template
   daemons, including public seeds used as intermediaries.
3. Upgrade remaining public RPC daemons.
4. Upgrade all EPoSE service nodes before height `21,600`.

Do not infer mining coverage from the number of upgraded service nodes. Record
each known template operator, its confirmed release/build SHA and its update
status. Unknown mining coverage remains explicitly unknown.

## Activation and observation

| Boundary | Meaning |
| --- | --- |
| `21,599` | Last block before local policy activation |
| `21,600` | Policy activation and epoch 30 / round 0 start |
| `21,799` | Last eligible round 0 carrier |
| `21,800` | Round 1 anchor; not an eligible round 1 carrier |
| `21,801` | First eligible round 1 carrier |
| `21,999` | Last eligible round 1 carrier |
| `22,000` | Round 2 anchor; not an eligible round 2 carrier |
| `22,001` | First eligible round 2 carrier |
| `22,259` | Evidence deadline and qualification close |
| `22,320` | Epoch 31 and first reward epoch for epoch-30 qualification |

Individual payments still follow deterministic reward scheduling and wallet
unlock rules; qualification does not promise payment in block `22,320`.

## Operator evidence

Before activation, archive the following without secrets or full receipt
payloads:

- daemon version and exact build SHA for each known template provider;
- synchronized tip hash across `seed-00` through `seed-05`;
- EPoSE diagnostic summaries per epoch and round;
- pending queue item/byte counts, duplicate/expiry/context/capacity counters;
- relay retry counts and canonical receipt counts by round;
- a successful mixed-version HF17 block-validity test with receipt-bearing
  blocks across `21,599`, `21,600`, and `21,601`.

After activation, do not declare success until epoch 30 is finalized at block
`22,259`. Compare generated, admitted, retransmitted, template-selected,
canonical and qualification-counted receipts for every round. Reachability,
valid registration and sufficient authentic committee receipts remain
prerequisites; the policy cannot guarantee qualification for an unhealthy
node.

## Release gate

Publish one immutable release containing the fixed activation height and this
operator guidance no later than canonical height `19,440`. This preserves the
planned 2,160-block / three-epoch operator lead before height `21,600`.
Synchronized operator daemons reported height `14,023` during candidate
verification on 2026-10-02, leaving 5,417 blocks to that release gate and
7,577 blocks to activation. Height is only a release-planning observation.
Confirm the canonical height/hash directly with synchronized operator daemons
before publishing or deploying; neither value is a consensus input to the
policy.

Do not replace binaries under an existing version. If the release or critical
template-provider confirmations are not ready by height `19,440`, select one
later fixed epoch boundary using
`720 * ceil((release_height + 2160) / 720)`, rebuild, and repeat every
boundary, reorg, mixed-version and integration test. The formula is release
planning only; every published build contains one documented fixed height.
