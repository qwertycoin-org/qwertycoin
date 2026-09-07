# Benchmarks

> **Historical HF17 evidence — superseded for parameter selection.** The
> measurements and decisions below describe the earlier implementation and a
> single light-mode solver path. They do not approve hardened EPoSE v2 mainnet
> parameters. The current CO-03 model and no-go decision are documented in
> `SECURITY_PARAMETERS.md`; its machine-readable results are under
> `review/results/`.

## Admission Proof

Measured on `seed host A` in Docker image
`qwertycoin-v2-epose-hardening:quorum-pr`, built from
`feature/epose-quorum-hardening` with:

```bash
docker build -f Dockerfile.epose-dev \
  --build-arg NPROC=2 \
  --build-arg EPOSE_BUILD_TARGETS="epose_unit_tests epose_admission_bench epose_sybil_sim" \
  -t qwertycoin-v2-epose-hardening:quorum-pr .
```

The admission hash uses RandomX with the previous finalized epoch hash as seed.
The current implementation uses RandomX light-mode hashing through
`crypto::rx_slow_hash`. The cache/VM is reused by the inherited RandomX helper
for repeated hashes with the same seed, but the measured solve rate is still far
below a mining-class optimized RandomX pipeline.

Command shape:

```bash
qwertycoin-epose-admission-bench <leading_zero_bits> <identities> <epoch> [max_hashes_per_identity] [steady_hashes]
```

The CO-03 revision reports first-hash setup separately from a repeated
same-seed steady-state loop. The latter still measures the inherited QWC helper
and must not be described as a mining-class optimized attacker implementation.

Bounded runs estimate creation cost without waiting for unlikely solutions:

| Leading zero bits | Max hashes | Total time | Hashes/sec | Expected create time |
| ---: | ---: | ---: | ---: | ---: |
| 14 | 64 | 3.339 s | 19.17 | 14.25 min |
| 15 | 64 | 3.546 s | 18.05 | 30.26 min |
| 16 | 64 | 3.381 s | 18.93 | 57.70 min |
| 18 | 64 | 3.301 s | 19.39 | 3.76 h |
| 20 | 64 | 3.286 s | 19.48 | 14.96 h |
| 22 | 64 | 3.307 s | 19.35 | 2.51 d |
| 24 | 64 | 3.392 s | 18.87 | 10.29 d |

Solved runs are useful for checking verification cost and sanity of the
projection:

| Leading zero bits | Identities | Total hashes | Total time | Avg create | Avg verify | Hashes/sec |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 8 | 3 | 595 | 23.376 s | 7.792 s | 39.0 ms | 25.45 |
| 12 | 3 | 3963 | 153.256 s | 51.085 s | 39.3 ms | 25.86 |

Decision for this branch:

```text
EPOSE_ADMISSION_LEADING_ZERO_BITS = 16
```

Rationale:

- 8 bits was too cheap for long-running mainnet assumptions.
- 18/20/22/24 bits are not operationally practical with the current solver.
- 16 bits is 256x harder than 8 bits and remains inside the desired
  20-60 minute average-registration corridor on the measured seed host A path.
- Verification remains one RandomX evaluation per registration and measured at
  roughly 39 ms in solved runs.

This value should be revisited if QWC adds a dedicated optimized admission
solver that uses a faster RandomX mode or precomputed dataset safely.

## Sybil / Committee Simulation

This branch includes a deterministic committee simulation tool:

```bash
qwertycoin-epose-sybil-sim
```

The default matrix covers total service-node counts `10`, `25`, `50`, and `100`
with controlled identity shares near `10%`, `20%`, `25%`, `33%`, `40%`, and
`50%`. It compares:

- committee 5 with fixed threshold 2,
- committee 5 with dynamic 2/3 threshold,
- committee 7 with dynamic 2/3 threshold,
- committee 9 with dynamic 2/3 threshold,
- committee 11 with dynamic 2/3 threshold.

The simulator uses the production `select_verifiers` function. Expired/empty
committees are excluded from threshold-success accounting, because a
zero-sized committee cannot qualify a subject in consensus.

Selected seed host A results from the corrected run:

| Total identities | Controlled share | Committee | Threshold | Attacker subjects meeting threshold | Full attacker committees |
| ---: | ---: | ---: | --- | ---: | ---: |
| 100 | 40% | 5 | fixed 2 | 31.68% | 0.48% |
| 100 | 40% | 5 | dynamic 2/3 | 7.83% | 0.48% |
| 100 | 40% | 7 | dynamic 2/3 | 8.67% | 0.06% |
| 100 | 40% | 9 | dynamic 2/3 | 8.92% | 0.00% |
| 100 | 40% | 11 | dynamic 2/3 | 2.42% | 0.00% |
| 100 | 50% | 5 | fixed 2 | 38.28% | 1.44% |
| 100 | 50% | 5 | dynamic 2/3 | 18.27% | 1.44% |
| 100 | 50% | 7 | dynamic 2/3 | 21.20% | 0.33% |
| 100 | 50% | 9 | dynamic 2/3 | 23.87% | 0.03% |
| 100 | 50% | 11 | dynamic 2/3 | 9.27% | 0.00% |

Decision for this branch:

```text
EPOSE_VERIFIER_COMMITTEE_SIZE = 9
required_attestations = ceil(actual_committee_size * 2 / 3)
```

Rationale:

- Committee 5 with fixed threshold 2 is too weak at high controlled-identity
  shares.
- Dynamic 2/3 removes the hidden fixed-2 bootstrap assumption and scales with
  the actual committee size.
- Committee 9 materially reduces full-attacker-committee exposure while keeping
  relay and tx-extra pressure lower than committee 11.
- Small networks continue to bootstrap because the rule uses the actual
  selected committee size, not the target size.

These numbers measure deterministic committee exposure only. They do not
replace live multi-node tests, economic modelling, or long-running adversarial
network tests.

## Upgrade Impact

Committee size, qualification threshold, and admission difficulty are
consensus-relevant. A chain using the older fixed threshold or 8-bit admission
target is not compatible with blocks validated under this branch.

For the controlled QWC-v2 mainnet validation phase, this branch should be
deployed only with a coordinated reset from genesis or behind an explicit future
hardfork rule. It must not be silently mixed with already-running nodes that use
the older EPoSE admission/qualification constants.
