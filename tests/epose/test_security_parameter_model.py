#!/usr/bin/env python3

import math
import unittest

from security_parameter_model import (
    CorrelatedScenario,
    build_report,
    capture_probability,
    grinding_probability,
    honest_qualification_probability,
    probability_at_least_hypergeometric,
    simulate_correlated_liveness,
)


class SecurityParameterModelTests(unittest.TestCase):
    def test_hypergeometric_matches_small_enumerable_case(self):
        self.assertAlmostEqual(1.0 / 6.0, probability_at_least_hypergeometric(4, 2, 2, 2))

    def test_documented_binomial_reference_is_reproduced_by_large_population(self):
        capture_6 = sum(
            math.comb(9, i) * 0.2**i * 0.8 ** (9 - i) for i in range(6, 10)
        )
        capture_7 = sum(
            math.comb(9, i) * 0.2**i * 0.8 ** (9 - i) for i in range(7, 10)
        )
        self.assertAlmostEqual(0.003066368, capture_6)
        self.assertAlmostEqual(0.000313856, capture_7)

    def test_subject_is_excluded_from_finite_population(self):
        attacker_subject = capture_probability(100, 20, 9, 6, True)
        honest_subject = capture_probability(100, 20, 9, 6, False)
        self.assertLess(attacker_subject, honest_subject)

    def test_small_population_never_shrinks_committee(self):
        self.assertEqual(0.0, capture_probability(10, 2, 9, 6, True))

    def test_honest_liveness_decreases_for_stricter_threshold(self):
        six = honest_qualification_probability(100, 20, 9, 6, 0.99)
        seven = honest_qualification_probability(100, 20, 9, 7, 0.99)
        self.assertGreater(six, seven)

    def test_grinding_reports_estimate_and_conservative_bound(self):
        result = grinding_probability(0.01, 10)
        self.assertLess(result["independent_estimate"], result["union_bound"])
        self.assertEqual(0.1, result["union_bound"])

    def test_correlated_simulation_is_reproducible(self):
        scenario = CorrelatedScenario(100, 20, 9, 6, 5, 0.99, 0.99, 1_000, 42)
        self.assertEqual(
            simulate_correlated_liveness(scenario), simulate_correlated_liveness(scenario)
        )

    def test_report_remains_no_go_and_labels_assumptions(self):
        report = build_report()
        self.assertEqual("no_go_for_economic_activation", report["status"])
        self.assertIn("owner_approved_numeric_risk_budget", report["unresolved_gates"])
        self.assertEqual(
            "illustrative_not_hardware_measurements", report["labels"]["admission_hash_rates"]
        )


if __name__ == "__main__":
    unittest.main()
