# EPoSE v2 security parameter study

Status: **CO-03 study in progress; no-go for economic activation**

Model version: 1

Generated results: `review/results/security_parameters_v1.json`

## Decision

No committee size, threshold, admission target, lease duration, round count, or
supported population is approved for mainnet activation by this study.

The earlier `16-bit / committee 9 / ceil(2/3)` choice was based on one slow
light-mode solver and a simulator that allowed the quorum to shrink with the
available committee. That evidence is insufficient for economic security. The
HF17 constants remain historical behavior; the v2 parameter manifest remains
`not-activatable` and its unresolved fields remain `null`.

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

## Activation gates

CO-03 cannot approve parameters until all of the following exist:

1. Alex approves a numerical risk budget for false qualification, honest
   qualification loss, operator cost, and evidence inclusion.
2. The required optimized hardware matrix is measured and reproducible.
3. Operator concentration and shared-backend assumptions are explicit.
4. CO-05 measures final bytes, validation operations, sync cost, and inclusion
   delay at target population.
5. Seed-withholding value is compared with QWC PoW opportunity cost and service
   rewards.
6. Verification-duty incentives and inbound-only behavior are modeled against
   the final receipt protocol.
7. The selected parameter set is committed into the signed parameter manifest
   through a separately reviewed activation decision.

Until then, the only valid conclusion is **no-go for economic activation**.
