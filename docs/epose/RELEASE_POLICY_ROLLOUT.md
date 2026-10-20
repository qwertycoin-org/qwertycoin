# Service release admission: rollout and follow-up work

Status: research design, **no Mainnet activation**, owner Sw0rdfish.
Read [source audit](RELEASE_POLICY_SOURCE_AUDIT.md) and [RFC](RELEASE_POLICY_RFC.md)
first. The following symbols refer to block HEIGHT, not chain block COUNT.

## Activation and real epoch math

The current compiled EPoSE profile starts at height 0. Existing epoch numbering
is absolute `start(E)=E*720`, end `E*720+719`, cutoff `E*720-61`, committee
anchor `E*720-60`, evidence deadline `E*720+659`. Freeze occurs before processing
the anchor block; close happens after processing the inclusive deadline block.
Admission context is `start(E-1)`. Round zero is signed against the earlier
committee anchor but its first permitted inclusion is `start(E)`; later rounds
use their offset's canonical block and first inclusion the following block.

The proposal adds `L(E)=start(E-1)-1`, not a reinterpretation of the existing
EPoSE activation parameter. H0 means the first block allowed to carry new policy
records. Service enforcement starts in a separately designated full epoch whose
lock has sufficient post-H0 notice. Before that epoch, old-format service
validation must remain the old validation. Both H0 and service bootstrap epoch
are consensus decisions, not command-line convenience flags on public networks.

| Symbol | Meaning |
| --- | --- |
| H0 | Initial protocol rollout: policy carrier/parsing enabled (Mainnet **unset**) |
| Hp | Height of canonical policy announcement, not website publication time |
| L(E) | Last block fixing policy/release availability for target service epoch E |
| H | `start(E)`; start of affected service epoch, NOT a stop-all-payouts height |
| E | First wholly affected service/qualification epoch |
| R | Reward epoch `E+1`, beginning at `(E+1)*720` in the current zero-offset profile |

`coordinator_v2::plan_reward` subtracts `timing.activation_height` for payout epoch
calculation; its parameter validation currently requires activation=0. Do not
substitute a new nonzero H0 into that field or assume this example applies to a
hypothetical future nonzero-offset profile. Re-derive any changed timing profile.

### Fully calculated illustrative later change (not a Mainnet proposal)

Assume a previously bootstrapped compatible network with existing window IDs
4/5, a new stable release ID6 (`2.1.0`, predecessor ID5), and a minimum notice of
5040 blocks. IDs and heights below are examples only, not production assignments.

| Event/boundary | Height | Consequence |
| --- | ---: | --- |
| Stable ID6 published in canonical registry | 29000 | Early use permitted for target epochs whose lock includes this record; GitHub publication alone is insufficient. |
| Hp: schedule head6 for E50 | 29500 | Announced, not yet the effective floor; no current earned claim removed. |
| Last acceptable Hp for notice=5040 | 30239 | `L(50)-5040`; height 30240 is too late. |
| L(50) | 35279 | `49*720-1`; 5779 blocks after Hp, exceeds minimum. |
| Admission context for E50 | 35280 | `start(49)`; policy was fixed before this first relevant context. |
| Enrollment cutoff | 35939 | Last admission inclusion; 35940 cannot affect frozen E50 membership. |
| Committee anchor / freeze | 35940 | Snapshot frozen before records in this block; round-0 anchor now known. |
| H−1 | 35999 | Last block of service E49; E49 evidence deadline was already 35939. Old closed E49 claims are unchanged. |
| H: start E50 | 36000 | New receipt wrapper/ID floor5 required for E50, round 0 opens. Existing reward epoch50 still pays from E49. |
| H+1 | 36001 | Same E50 requirement; no extra service penalty or identity reset. |
| Round-0 last inclusion | 36199 | Offset of next round minus one. |
| Round-1 anchor / first inclusion | 36200 / 36201 | Window through 36399. |
| Round-2 anchor / first inclusion | 36400 / 36401 | Window through inclusive deadline. |
| Evidence deadline / close E50 | 36659 | Last receipts count, then qualification closes. |
| E50 end | 36719 | No reopening of qualification. |
| R=51, first resulting reward block | 36720 | Uses closed E50; an ID4 node without eligible E50 evidence no longer participates. |

Old service nodes may still receive E49-derived payouts at and after H. The
policy must not re-check their CURRENT release during payment of closed E49.
An updated node keeps identity/lease/descriptor obligations and may qualify in
the next regular round-complete epoch. Missing rounds are never backfilled.

A reasonable **review recommendation**, not an approved parameter: minimum
5040 blocks (about seven days at the configured 120-second target) before L(E),
normal notice 10080 blocks (about fourteen days) before L(E). This resembles the
historical two-week service rollout and allows package/operator testing. Since
L precedes service start by 721 blocks, actual notice to H is longer. Need real
operator coverage and CI/artifact evidence before deciding; block production
variance means all calendar dates remain estimates. Three days is not a rule.

The compressed unit-test profile is intentionally different: length20, anchor2,
H0=5, first enforced E4, notice20, initial policy at7; publication ID3 at8,
next schedule at9 for E6, L6=99, H=120, deadline137, R7=140. It exercises existing
RandomX admission, committee, receipt coverage and reward selection with reduced
work/blocks. It is not public testnet configuration and does not reserve heights.

## Bootstrap compatibility matrix

| Software | Existing chain validation | New generic policy | New signed release evidence |
| --- | --- | --- | --- |
| Published v2.0.4 / v2.0.5 | Existing protocol | No | No |
| This branch's ordinary daemon | Unchanged existing protocol | No (test-only code is not linked) | No production support |
| Isolated prototype test target | Synthetic ordered records plus real membership primitives | Yes, schema1 vocabulary only | Yes, separate claim wrapper; test keys only |
| Future reviewed bootstrap release | Must preserve all pre-H0 states | Requires production carrier/migration work | Requires producer, transport and validator implementation |
| Later still-compatible but service-obsolete release | Can retain generic policy validation | Can process subsequent known-schema policy records | Own obsolete ID excluded from new epochs |
| Future unknown wire/function upgrade | Cannot assume compatibility | Does not load unknown code | Separate coordinated protocol upgrade required |

A truthful bootstrap starts a new compatible release-ID lineage or uses reviewed
bootstrap entries for genuinely compatible artifacts. It cannot prove that a
peer with a modified string is a historical official v2.0.4 installation.
Artifact release authorization is not execution attestation. A preparatory
upgrade is required; no published tag in this investigation supports bootstrap.

## Deterministic chain integration work (next focused PR)

This is required before daemon-level rollout, not silently accomplished by the
prototype. These are real consensus changes:

1. Allocate explicit policy and new evidence record types/versions under a
   separately reviewed H0 gate; preserve old supported versions historically.
   The existing `version>=HF17` state-machine predicate is not a license to
   reinterpret old payloads. Unknown new formats must fail deterministically.
2. Add structural budgets (including up to six authority or four wrapper checks)
   before expensive verification, canonical transaction order, fees/carrier rules,
   reorg-safe policy/claim state and versioned commitment schema. Existing
   consensus record budget/undo paths must include this cost; they currently do not.
3. Enforce wrapper versus legacy evidence by target SERVICE epoch, including old
   evidence manufactured before H but intended for E. Retain closed qualification
   and previous service-epoch reward ownership. Do not filter frozen membership
   or reset committees automatically when a release becomes stale.
4. Extend producer, subject response and verifier transport. A producer must pick
   the release/policy binding for the target epoch, not today's latest register.
   Expose unsupported/missing/unknown/below-minimum distinctly; use same view as
   canonical validation. No remote-update executable downloader.
5. Extend atomic LMDB commitments and migration/replay. Historical pre-H0 hash
   commitments must match original code exactly. Restart, rescan, corrupt-cache
   recovery, disconnect/alternate-chain replay and deep reorg cannot trust an
   endpoint cache or external manifest. Retain canonical full policy payloads.
6. Define production archive lifetime/hot-index bounds; research 4096-record cap
   is not a finalized production policy. Add parser fuzzing and operational
   worst-case measurements before signing a release candidate.

For reviewability these are narrower implementation PRs than a single broad
policy/governance/transport/migration rewrite. No implementation approval for a
Mainnet height is inferred from reviewing this research PR.

## Reproducible multi-daemon acceptance plan (not yet executed)

Extend existing `tests/epose/integration/local_epose_network.sh`, retaining its
internal-only Docker networks, disposable volumes, readiness and convergence
assertions. Do not use running Mainnet datadirs. Need five service daemons plus
one non-service miner, a fresh sync validator and an older **bootstrap-compatible**
validator binary; published pre-bootstrap binaries cannot play that role.

- Deploy generic policy support before H0; mine H0−1/H0/H0+1 and compare ordinary
  validation. Only the service producer's claimed fixture release differs later.
- Include test-authority publications and a later schedule without upgrading the
  generic validator. Assert same tip, policy/state hashes and frozen epoch view.
- Delay/offline each optional HTTP/DNS mirror independently: same chain, same
  decision. Mirrors neither unblock invalid records nor block valid replay.
- Mine across L, admission cutoff, freeze, round anchors, H and deadline; assert
  stale official fixture has no new qualification, earlier claims still pay,
  remaining current service nodes satisfy actual distinct-verifier thresholds.
- Upgrade that service identity, without replacing keys; demonstrate next regular
  qualification/reward and no retroactive claims. Also demonstrate true-ID spoof
  bypass explicitly; do not equate it to binary detection.
- Persist/restart/fresh-sync and branch/reorg across policy publication, schedule,
  L, activation, close and first payout; compare to replay, not just live RPC.
- Submit unknown future record schema to the otherwise compatible old validator:
  reject; generic policy support cannot magically supply its parser or service.

No daemon carrier currently consumes the prototype records, so running the old
network harness unchanged would NOT prove any of these new acceptance criteria.
No live multi-daemon policy rollout or production state migration is claimed.

## Additive diagnostics and GUI/Explorer follow-ups

Proposed additive `release_policy` object on existing status/reward diagnostics:

- `own_version_label`, `own_release_id` (nullable/unknown distinct from 0);
- `supported_policy_schemas`, `supported_receipt_versions`;
- `target_service_epoch`, `lock_height`, `active_stable_id`, `minimum_release_id`;
- `policy_sequence`, `policy_hash` derived from the same canonical epoch view;
- `next_change`: `{inclusion_height, effective_height, service_epoch,
  reward_epoch, reward_start_height, minimum_release_id}`;
- `blocks_remaining=max(0,H-next_block_height)` with an explicitly named height
  basis, `estimated_seconds=blocks_remaining*120` checked for overflow; no timer
  can activate a policy;
- `closed_qualification`: `{source_epoch, hash, earned, payout_epoch}` separate
  from `future_eligibility`;
- reasons: `release_unknown`, `release_missing`, `release_below_minimum`,
  `policy_not_locked`, `record_unsupported`, alongside existing admission,
  unreachable endpoint, missing rounds/receipts and no committee diagnostics.

CLI follow-up: show current earned qualification separately from future service
admission. A version error must not replace the actual receipt/admission reason.
Use shared `epoch_view`, no separate SemVer/GitHub implementation in RPC or GUI.

GUI follow-up: add accessible current/future status panels, explicit target epoch,
block countdown and estimated date; link authorized release information without
executing downloads. Example: “Update required before block H. From service
epoch E the minimum release ID is N. Already earned entitlements remain valid.”

Explorer follow-up: index canonical policy records with inclusion block/hash,
signer generation, signature indices, head/predecessor and Hp/H/E/R. Show
reorged announcements as noncanonical, not currently active; never fetch a live
JSON minimum when rendering historical reward eligibility. Display authority
rights and signed self-claim limitations openly.

DNS/GitHub/HTTPS follow-up: optional signed JSON mirror of canonical bytes/hash
and expected network; stale/conflicting/incomplete/unreachable mirrors produce
warnings only. Chain data wins for every reward decision. DNSSEC and signatures
cannot prevent withholding/stale delivery; no online query during validation.

## Operational review gates

Mainnet H0 stays absent until source/artifact review, independent authority-key
custody, compatible-validator/miner coverage, migration and multi-daemon evidence
are approved. Pools/validators need initial rollout, and again for genuine future
wire/consensus changes. Service minimum updates alone are not that rollout.

Before H0, an unpublished candidate can be replaced. After activation or policy
inclusion, deleting config, altering DNS or reinstalling an old binary is NOT a
consensus rollback. Only the protocol's bounded pending cancellation/rotation or
an explicitly coordinated new protocol can change future behavior; past chain
history and earned claims cannot be rewritten by an operations runbook.
