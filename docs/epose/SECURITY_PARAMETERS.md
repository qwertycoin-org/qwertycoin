# EPoSE v2 security parameter study

Status: **launch parameters frozen; exact-genesis rehearsal passed**

> **Reset-1 notice (2026-09-13):** The parameter values in this study remain
> unchanged, but its exact-genesis rehearsal and artifact binding belong to the
> superseded `90662948...` chain. The Reset-1 identity is defined by ADR-0009.
> Historical evidence below is retained as evidence for its actual source and
> must not be promoted to candidate-bound evidence for the new genesis.

Model version: 2

Generated results: `review/results/security_parameters_v1.json`

## Launch decision

The launch candidate uses the following bounded profile:

- RandomX admission target: **18 leading zero bits**, renewed every target epoch;
- verifier committee target: **9**;
- quorum: **`ceil(actual_committee_size * 2 / 3)`**, represented as 6-of-9
  when the full target committee is available;
- three receipt rounds at offsets `0 / 200 / 400`, with two passing rounds
  required;
- maximum active population: **100 identities per target epoch**;
- reward: **10% of scheduled subsidy only**, with fees retained by the miner.

The exact candidate at source `2e118dd2cca468ae52fec5c9c30e33673e312043`,
canonical manifest
`385f7bfa3b568f04a931ffb9f91c3e3ec4d01f620c3d4ea1075504ae60ea96be`
and genesis `906629482787e94cb00463696a0e95ec75a480da09257c6270c65ba1a74a76b0`
passed the isolated four-node final rehearsal. Parameter selection is final,
subject to the independent release gates; the overall release remains NO-GO.

The actual committee is `min(9, frozen_population - 1)` because the subject
cannot verify itself. This permits a four-identity bootstrap with a 2-of-3
quorum and prevents one outbound-withholding verifier from becoming the only
qualified subject. It does not pretend that four founding identities provide
the independence of a mature nine-verifier committee.

For the documented 100-identity / 20%-controlled sensitivity case, 6-of-9 has
a per-round controlled-subject capture probability of `0.001383297129`. Under
the explicitly limited assumption that round committees are independent, the
probability of capture in at least two of three rounds is `0.000005735239`.
The corresponding honest qualification estimate is `0.977242821083` when
honest verifier seats independently succeed with 99% probability. These are
model bounds for the stated assumptions, not claims about real operator
independence.

The 100-identity ceiling is selected because its optimistic two-round minimum
is 1,200 receipts at 6-of-9, within the 660-block evidence window and current
template/relay limits. The previous ceiling of 1,000 is rejected: its 12,000
minimum receipts exceed the 10,560-record window even at 16 receipts per block.

The 18-bit admission target is an anti-spam work floor, not a substitute for
stake or operator identity. It is four times the rehearsal work and remains
operationally testable. Optimized attackers retain a material Sybil advantage;
that residual risk is explicit and can be tightened only through a future
consensus parameter hardfork. The tested HF17-to-future-version continuation
path exists for that purpose.

## Reproduction

```text
python3 -m py_compile \
  tests/epose/security_parameter_model.py \
  tests/epose/test_security_parameter_model.py
PYTHONPATH=tests/epose python3 tests/epose/test_security_parameter_model.py
python3 tests/epose/security_parameter_model.py \
  --output docs/epose/review/results/security_parameters_v1.json
```

The model uses only Python's standard library. Exact committee results use the
hypergeometric distribution and exclude the subject from verifier candidates.
Independent honest-seat availability uses an exact binomial tail. Correlated
operator results use deterministic Monte Carlo with the seed recorded in every
scenario and report a normal-approximation 95% interval. The latter interval is
a simulation diagnostic, not a cryptographic bound.

## Committee capture and honest liveness

The primary finite-population example has 100 identities, of which 20 are
controlled. A controlled subject therefore leaves 19 controlled verifier
candidates among 99 candidates. Honest-round liveness assumes controlled
verifiers withhold and each honest selected identity succeeds independently
with probability 99%.

| k | q | Rule | False qualification of controlled subject | Honest one-round success |
|---:|---:|---|---:|---:|
| 9 | 6 | ceil(2k/3) | 0.138330% | 91.017276% |
| 9 | 7 | strict supermajority | 0.009553% | 71.605556% |
| 15 | 11 | strict supermajority | 0.000057% | 82.488541% |
| 21 | 15 | strict supermajority | 0.000000074% | 89.251046% |
| 31 | 21 | strict supermajority | 0% in this finite case | 97.989086% |

The last zero is structural for this example: after excluding the controlled
subject, only 19 controlled candidates remain and cannot fill 21 seats. It is
not a general security claim. Identity share is not operator share, and many
keys may share one backend.

Changing 6-of-9 to 7-of-9 lowers false qualification by roughly 14.5x in this
example but also lowers honest one-round success by about 19.4 percentage
points under withholding. Larger committees recover liveness while increasing
network, verification, and evidence costs. A threshold cannot be selected in
isolation from CO-04 service checks and CO-05 capacity.

## Operator correlation

The deterministic simulation groups honest identities into 1, 2, 5, or 10
identities per operator. Operator availability is shared by the group; an
additional 99% identity-level success applies only when that operator is
online. With 20 controlled identities, k=9, q=7, and 95% operator availability,
the measured one-round success estimates across the four group sizes are
60.132% to 60.504% over 50,000 trials each. Individual 95% intervals are stored
in the JSON result.

This model exposes correlated failures but does not establish the real
operator distribution. Cloud, ASN, jurisdiction, and shared-backend
concentration require measurement and must not become consensus identity
weights.

## Grinding and seed withholding

For each committee row, the result file reports both:

```text
independent estimate = 1 - (1 - p)^J
union bound          = min(1, p * J)
```

where `p` is the exact single-opportunity capture probability and `J` is the
number of attempted favorable opportunities. The independence estimate is
valid only if attempts are independent. The union bound is conservative and
does not require independence. Neither calculation proves that a PoW block
hash is unbiased or quantifies the cost of withholding it. Admission and
membership must remain frozen before the anchor so post-seed key grinding is
invalid regardless of these estimates.

## Admission economics

The machine-readable result uses the transparent relationship:

```text
mean hashes per ticket       = 2^d
mean seconds per ticket      = 2^d / H
p95 seconds per ticket       = -ln(0.05) * 2^d / H
amortized seconds per epoch  = 2^d / (H * lease_epochs)
```

The `H = 1,000 / 10,000 / 100,000 hashes/s` rows are illustrative sensitivity
inputs, **not hardware measurements**. They demonstrate why 16 bits combined
with a 30-epoch lease cannot be called a credible Sybil price without an
optimized solver benchmark: at 100,000 hashes/s it amortizes to approximately
0.022 seconds of hashing per eligible epoch.

Required measurements must separate RandomX cache/dataset initialization from
steady-state solving and reuse the same seed context across attempts. Required
classes are:

- supported x86-64 ordinary operator hardware;
- supported ARM64 ordinary operator hardware;
- an affordable operator VM;
- a high-throughput attacker configuration with shared cache/dataset;
- verification in every supported RandomX implementation mode.

Each result must record CPU model, architecture, memory, OS, compiler, build
flags, RandomX mode, warm-up, trials, median, p95, throughput, variance, energy
or rental assumptions, and exact source commit. Measurements from one host
cannot approve a mainnet target.

The isolated four-service-node reset-risk rehearsal on commit `f5521a0d7`
provides a bounded operational observation, not a cross-hardware benchmark.
Four concurrent real 16-bit admission jobs on the same 8 GiB host completed in
approximately 157, 197, 464, and 2,020 seconds. The wide spread is expected
for independent proof search. An 18-bit job has four times the mean search
space; its actual completion time is not inferred from any single sample.
The final exact-genesis rehearsal completed four genuine 18-bit admissions.
All four nodes converged on four qualified members in reward-source epoch 1,
the height-1440 canonical reward path, restart persistence and fresh-genesis
replay. The OOM guard recorded no event and at least 3,083 MiB available; the
separately frozen production rehearsal remained unchanged. Immutable evidence
is recorded in `review/results/final_genesis_rehearsal_v1.json`.

## Evidence capacity

For a three-round candidate requiring success in two rounds, the optimistic
minimum receipt count is:

```text
minimum receipts per epoch = population * threshold * 2
```

At 1,000 subjects, 7-of-9 therefore needs at least 14,000 receipts. Even 16
receipts in each of the 660 evidence-window blocks carries only 10,560 receipts
before enrollment, retries, framing, proofs, or other traffic. For 11-of-15 the
minimum is 22,000. This proves that committee policy, sampling frequency,
batching, wire size, and supported population must be approved together.

The model reports record counts only. It deliberately does not invent envelope
bytes, signature-verification cost, transaction weight, or minimum miner
inclusion share; those are measured by CO-05.

## Verification-duty incentives

With every subject checked by a committee each round, the mean outgoing duties
per identity are approximately `k * rounds`. At three rounds this is 27, 45,
63, or 93 checks per epoch for k=9, 15, 21, or 31. At one second per check that
is the same number of seconds of work per identity per epoch.

An operator can save this cost by answering inbound probes while skipping its
outgoing duties. Missing evidence cannot objectively distinguish that strategy
from subject failure, verifier failure, packet loss, or censorship. No per-vote
reward or penalty is proposed here because reciprocal fabrication and false
blame remain unresolved.

### Four-node rehearsal selective-withholding result

The activation-candidate rehearsal uses four members, a committee size of
three, a threshold of three and two required rounds. Because the subject is
excluded, every subject's committee is the other three members. If member D
answers inbound probes but withholds every outbound verifier receipt, each
round deterministically produces the receipt counts `A=2, B=2, C=2, D=3`.
Across two required rounds, D alone can qualify without forging a signature.

`full_committee_selective_withholding(4, 3, 3)` reproduces this historical
rehearsal result. The launch rule no longer uses fixed 3-of-3: with four frozen
members its actual committee is three and its quorum is two. Under the same
single-withholder scenario every subject still has at least two receipts, so
the withholder cannot qualify alone. Two colluding verifiers can nevertheless
fabricate a bootstrap subject's availability; that is an explicit limitation
of a four-identity bootstrap, not a claim of mature-network security.

## Candidate sets for further testing

The following are **experiment matrix entries**, not mainnet recommendations:

```text
k = 9, 15, 21, 31
q = ceil(2k/3) and floor(2k/3)+1
rounds = 1 and 3
rounds required = 1 and 2 where applicable
admission bits = 16, 18, 20, 22, 24
lease epochs = 1 and 30
population = 100, 1,000, 10,000
```

## Acceptance boundary

The selected values are release-final after all of the following passed on the
exact parameter commitment and final genesis:

1. focused unit tests prove dynamic committee sizing, quorum calculation,
   selective-withholding symmetry, record bounds and deterministic replay;
2. the isolated exact-genesis network rehearsal proves four-member bootstrap,
   qualification, the canonical reward boundary, restart persistence and fresh
   replay;
3. the 100-identity limit remains within the enforced block, template and relay
   ceilings under the focused EPoSE resource tests;
4. the signed exact-source commit, compiled profile, canonical manifest,
   genesis and Linux candidate artifacts bind the same values.

This parameter decision does not satisfy independent consensus/payment-proof
review, the complete release matrix, wallet funds-safety evidence, or the
remaining exact-candidate partition and deep-reorg obligations. Those release
gates keep the overall decision NO-GO.

Optimized admission hardware, real operator correlation, large-population soak
and seed-withholding economics remain post-launch measurement obligations.
They may justify a future parameter hardfork, but they must not be represented
as already measured or solved by this launch decision.
