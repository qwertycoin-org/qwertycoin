# EPoSE v2 Mainnet Rehearsal Profile

**Profile date:** 2026-09-07  
**Network path:** regular MAINNET, Qwertycoin block major version 17  
**EPoSE protocol:** version 2 from genesis height 0  
**Status:** complete activation candidate for a resettable rehearsal; **not a
final mainnet security-parameter approval**

## Commitments

- Candidate genesis hash:
  `e791e506200ba3a221b87b6c78359c2bbb13c3ef622e1c90b3b3fbb52f4943f5`
- Consensus-parameter SHA-256:
  `16bde722b05071956f10be18530235be42b374228ead95e2b053d99d53f328ff`
- Exact candidate implementation revision recorded by the manifest:
  `e306698ac23bb23d8a6050d479d8f2f2e32af505`

The parameter commitment is SHA-256 over the canonical JSON projection of
`activation`, `admission`, `carrier`, `committee`, `encoding`, `epoch`,
`network`, `resource_limits`, `reward`, `schema_version`, and `state`, with a
single trailing newline. Release metadata and the commitment field itself are
excluded. This permits the manifest to identify the exact build revision
without creating a self-referential hash while binding every consensus-bearing
value. `tests/epose/manifest_v2.py` is the authoritative implementation.

## Candidate choices and scope

| Area | Rehearsal value | Rationale and remaining acceptance work |
|---|---:|---|
| Epoch / anchor | 720 / 60 blocks | Preserves the reviewed bootstrap boundaries: cutoff 659, anchor 660, evidence close 1379, first possible payout 1440. |
| Admission | RandomX, 16 leading zero bits, one-epoch lease | Makes repeated real admissions feasible during the rehearsal. This is not a final Sybil-cost choice; multi-hardware measurements and an approved risk budget remain mandatory. |
| Committee | 3 members, threshold 3, rounds at 0/200/400, 2 rounds required | Allows four independently keyed nodes to exercise subject-excluding committees and failure behavior. It deliberately requires unanimity within each selected three-member committee. The small population is topology-driven rehearsal evidence, not a production independence claim. |
| Active population ceiling | 1000 per target epoch | Bounds canonical membership state and is enforced before freeze. Sustained-load and memory measurements remain required before final acceptance. |
| Block budget | 256 KiB, 1024 records, 2048 signatures, 8 RandomX checks | Conservative explicit ceilings inherited from component fixtures. They are now consensus-bound and must be benchmarked under worst-valid and invalid input on supported hardware. |
| Transaction envelope | 64 KiB, 4 envelopes, 256 records | Bounds parsing before cryptographic work while permitting relay data plus a Coinbase payment proof. |
| Undo | 2160 blocks | Three epochs, exceeding the validator's minimum two-epoch coverage. Crash, deep-reorg, replay and pruning evidence remains open. |
| Relay queue | 2048 items / 1 MiB, with 512 items / 256 KiB reserved for each enrollment and evidence | Local non-consensus fairness policy. The remaining capacity absorbs mixed traffic; backlog/inclusion measurements may require revision. |
| Mining template | 256 records / 32 KiB, with 64 records / 8 KiB reserved for each class | Fits one canonical transaction envelope, stays below block ceilings, and guarantees both traffic classes local capacity. |
| Empty qualification | miner fallback | With no qualified service payee, the miner receives the full scheduled subsidy. No privileged bootstrap payee is introduced. |
| Reward accounting | 10%, subsidy-only, actual-issued subsidy | Fees remain with the miner. When a qualified payee exists, 10% of scheduled subsidy is assigned to the service output and proven by the scoped transaction proof. |
| State / pruning | schema 1; pruning unsupported and fail-closed | Prevents a pruned node from claiming v2 validation before the history contract is proved. |

The candidate distinguishes **configuration validity** from **launch
readiness**. The release evaluator must remain NO-GO until its evidence ledger
is satisfied. Any changed consensus value requires a new parameter commitment,
genesis-bound vectors, build, and affected rehearsal evidence.

## Genesis and final reset

The rehearsal begins with an empty v2 registry, membership, receipt, and
qualification state. Genesis provides the admission context for service epoch
1; no record may use its own inclusion block as prior context. After evidence
is archived, all writers and the explorer/indexer are stopped and only the
inventoried chain-derived data is removed. The final launch must use a distinct
genesis and recomputed parameter commitment, so rehearsal history cannot be
reintroduced by an old peer. Wallet and compatible operator/service secrets are
preserved but must register again under the new genesis domain.
