#!/usr/bin/env python3
"""Deterministic CO-03 security and capacity model for EPoSE v2.

The model intentionally separates identity share from operator independence.
It does not select mainnet parameters or claim hardware measurements.
"""

from __future__ import annotations

import argparse
import json
import math
import random
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


MODEL_VERSION = 1
DEFAULT_SEED = 0x515743


def probability_at_least_binomial(n: int, threshold: int, probability: float) -> float:
    if threshold <= 0:
        return 1.0
    if threshold > n:
        return 0.0
    return sum(
        math.comb(n, successes)
        * probability**successes
        * (1.0 - probability) ** (n - successes)
        for successes in range(threshold, n + 1)
    )


def probability_at_least_hypergeometric(
    population: int, controlled: int, sample: int, threshold: int
) -> float:
    if population < 0 or controlled < 0 or controlled > population:
        raise ValueError("invalid population")
    if sample < 0 or sample > population:
        raise ValueError("invalid sample")
    if threshold <= 0:
        return 1.0
    if threshold > sample:
        return 0.0
    denominator = math.comb(population, sample)
    total = 0
    for selected in range(threshold, sample + 1):
        honest = sample - selected
        if selected <= controlled and honest <= population - controlled:
            total += math.comb(controlled, selected) * math.comb(population - controlled, honest)
    return total / denominator


def honest_qualification_probability(
    population: int,
    controlled: int,
    committee: int,
    threshold: int,
    honest_seat_availability: float,
) -> float:
    """Exact per-round probability for an honest subject when attackers withhold."""
    candidates = population - 1
    if committee > candidates:
        return 0.0
    denominator = math.comb(candidates, committee)
    probability = 0.0
    for attacker_seats in range(0, committee + 1):
        honest_seats = committee - attacker_seats
        if attacker_seats > controlled or honest_seats > candidates - controlled:
            continue
        selection = (
            math.comb(controlled, attacker_seats)
            * math.comb(candidates - controlled, honest_seats)
            / denominator
        )
        probability += selection * probability_at_least_binomial(
            honest_seats, threshold, honest_seat_availability
        )
    return probability


def capture_probability(
    population: int,
    controlled: int,
    committee: int,
    threshold: int,
    attacker_subject: bool,
) -> float:
    candidates = population - 1
    controlled_candidates = controlled - (1 if attacker_subject else 0)
    return probability_at_least_hypergeometric(
        candidates, controlled_candidates, committee, threshold
    )


def grinding_probability(single_probability: float, attempts: int) -> dict[str, float]:
    return {
        "independent_estimate": 1.0 - (1.0 - single_probability) ** attempts,
        "union_bound": min(1.0, single_probability * attempts),
    }


@dataclass(frozen=True)
class CorrelatedScenario:
    population: int
    controlled: int
    committee: int
    threshold: int
    honest_identities_per_operator: int
    operator_availability: float
    identity_availability_given_operator: float
    trials: int
    seed: int


def simulate_correlated_liveness(scenario: CorrelatedScenario) -> dict[str, float | int]:
    if scenario.population - scenario.controlled - 1 <= 0:
        raise ValueError("scenario needs at least one honest verifier candidate")
    rng = random.Random(scenario.seed)
    subject = scenario.controlled
    candidates = [index for index in range(scenario.population) if index != subject]
    successes = 0
    for _ in range(scenario.trials):
        selected = rng.sample(candidates, scenario.committee)
        operator_online: dict[int, bool] = {}
        votes = 0
        for identity in selected:
            if identity < scenario.controlled:
                continue
            honest_index = identity - scenario.controlled
            operator = honest_index // scenario.honest_identities_per_operator
            if operator not in operator_online:
                operator_online[operator] = rng.random() < scenario.operator_availability
            if operator_online[operator] and rng.random() < scenario.identity_availability_given_operator:
                votes += 1
        if votes >= scenario.threshold:
            successes += 1
    estimate = successes / scenario.trials
    standard_error = math.sqrt(estimate * (1.0 - estimate) / scenario.trials)
    return {
        "successes": successes,
        "trials": scenario.trials,
        "estimate": estimate,
        "normal_95_low": max(0.0, estimate - 1.96 * standard_error),
        "normal_95_high": min(1.0, estimate + 1.96 * standard_error),
    }


def committee_rows(population: int, controlled: int) -> list[dict[str, float | int | str]]:
    rows: list[dict[str, float | int | str]] = []
    for committee in (9, 15, 21, 31):
        for threshold_name, threshold in (
            ("ceil_two_thirds", math.ceil(2 * committee / 3)),
            ("strict_supermajority", math.floor(2 * committee / 3) + 1),
        ):
            if committee >= population:
                continue
            capture = capture_probability(
                population, controlled, committee, threshold, attacker_subject=True
            )
            liveness = honest_qualification_probability(
                population, controlled, committee, threshold, 0.99
            )
            rows.append(
                {
                    "committee": committee,
                    "threshold": threshold,
                    "threshold_rule": threshold_name,
                    "attacker_subject_capture": capture,
                    "honest_round_success_attacker_withholds": liveness,
                }
            )
    return rows


def admission_rows() -> list[dict[str, float | int]]:
    rows = []
    for bits in (16, 18, 20, 22, 24):
        for hashes_per_second in (1_000, 10_000, 100_000):
            for lease_epochs in (1, 30):
                mean_seconds = 2**bits / hashes_per_second
                rows.append(
                    {
                        "leading_zero_bits": bits,
                        "illustrative_hashes_per_second": hashes_per_second,
                        "lease_epochs": lease_epochs,
                        "mean_seconds_per_ticket": mean_seconds,
                        "p95_seconds_per_ticket": 2.995732273553991 * mean_seconds,
                        "amortized_seconds_per_epoch": mean_seconds / lease_epochs,
                    }
                )
    return rows


def capacity_rows() -> list[dict[str, int]]:
    rows = []
    evidence_window_blocks = 660
    rounds_required = 2
    for population in (100, 1_000, 10_000):
        for committee, threshold in ((9, 6), (9, 7), (15, 11), (21, 15), (31, 21)):
            minimum_receipts = population * threshold * rounds_required
            for receipts_per_block in (1, 4, 16, 64):
                capacity = evidence_window_blocks * receipts_per_block
                rows.append(
                    {
                        "population": population,
                        "committee": committee,
                        "threshold": threshold,
                        "rounds_required": rounds_required,
                        "minimum_receipts": minimum_receipts,
                        "receipts_per_block": receipts_per_block,
                        "window_capacity": capacity,
                        "surplus": capacity - minimum_receipts,
                    }
                )
    return rows


def verification_duty_rows() -> list[dict[str, float | int]]:
    rows = []
    for committee in (9, 15, 21, 31):
        for rounds in (1, 3):
            for milliseconds_per_check in (100, 1_000, 5_000):
                checks_per_identity = committee * rounds
                rows.append(
                    {
                        "committee": committee,
                        "rounds": rounds,
                        "milliseconds_per_check": milliseconds_per_check,
                        "mean_checks_per_identity_per_epoch": checks_per_identity,
                        "mean_seconds_per_identity_per_epoch": checks_per_identity
                        * milliseconds_per_check
                        / 1_000,
                    }
                )
    return rows


def build_report() -> dict[str, object]:
    population = 100
    controlled = 20
    committees = committee_rows(population, controlled)
    grind = []
    for row in committees:
        if row["committee"] != 9:
            continue
        for attempts in (1, 10, 100, 1_000):
            grind.append(
                {
                    "committee": row["committee"],
                    "threshold": row["threshold"],
                    "threshold_rule": row["threshold_rule"],
                    "attempts": attempts,
                    **grinding_probability(float(row["attacker_subject_capture"]), attempts),
                }
            )

    correlated_scenarios = [
        CorrelatedScenario(population, controlled, 9, threshold, group, operator_availability, 0.99, 50_000, DEFAULT_SEED + threshold * 1_000 + group * 10 + int(operator_availability * 100))
        for threshold in (6, 7)
        for group in (1, 2, 5, 10)
        for operator_availability in (0.99, 0.95)
    ]
    correlated = [
        {"scenario": asdict(scenario), "result": simulate_correlated_liveness(scenario)}
        for scenario in correlated_scenarios
    ]

    return {
        "model_version": MODEL_VERSION,
        "status": "no_go_for_economic_activation",
        "labels": {
            "admission_hash_rates": "illustrative_not_hardware_measurements",
            "capture": "exact_hypergeometric_attacker_subject_excluded",
            "liveness": "exact_hypergeometric_plus_binomial_unless_marked_simulation",
            "correlated_liveness": "deterministic_monte_carlo_normal_95_interval",
            "grinding": "independence_estimate_and_union_bound_not_a_beacon_proof",
            "capacity": "record_count_only_wire_bytes_owned_by_CO_05",
        },
        "baseline": {"population": population, "controlled_identities": controlled},
        "committees": committees,
        "grinding": grind,
        "correlated_liveness": correlated,
        "admission": admission_rows(),
        "capacity": capacity_rows(),
        "verification_duties": verification_duty_rows(),
        "unresolved_gates": [
            "owner_approved_numeric_risk_budget",
            "optimized_solver_measurements_on_supported_x86_64_and_arm64",
            "ordinary_operator_and_high_throughput_attacker_cost_measurements",
            "CO_05_wire_size_and_minimum_inclusion_share",
            "operator_concentration_measurements",
            "PoW_security_budget_effect",
        ],
    }


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    rendered = json.dumps(build_report(), indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
