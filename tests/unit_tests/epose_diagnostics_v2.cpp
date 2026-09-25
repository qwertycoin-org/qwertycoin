// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/error.hpp>

#include "net/http_client.h"
#include "epose/diagnostics_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  receipt_attempt_context_v2 context(uint64_t id = 1)
  {
    receipt_attempt_context_v2 out{};
    out.attempt_id = id;
    out.epoch = 11;
    out.round = 1;
    out.chain_height = 7999;
    out.deadline_height = 8039;
    out.slot = hash_text("slot");
    out.endpoint_commitment = hash_text("endpoint");
    out.started_utc_ms = 1000;
    out.started_steady_ms = 100;
    crypto::secret_key ignored{};
    crypto::generate_keys(out.subject_identity, ignored);
    crypto::generate_keys(out.verifier_identity, ignored);
    return out;
  }
}

TEST(epose_diagnostics_v2, reason_codes_are_stable_unique_and_machine_readable)
{
  const std::vector<receipt_failure_reason_v2> reasons{
      receipt_failure_reason_v2::descriptor_unavailable,
      receipt_failure_reason_v2::descriptor_invalid,
      receipt_failure_reason_v2::descriptor_expired,
      receipt_failure_reason_v2::descriptor_commitment_mismatch,
      receipt_failure_reason_v2::dns_resolution_failed,
      receipt_failure_reason_v2::connection_refused,
      receipt_failure_reason_v2::transport_timeout,
      receipt_failure_reason_v2::transport_failure,
      receipt_failure_reason_v2::http_status_unsuccessful,
      receipt_failure_reason_v2::remote_challenge_rejected,
      receipt_failure_reason_v2::response_malformed,
      receipt_failure_reason_v2::response_oversized,
      receipt_failure_reason_v2::response_unexpected,
      receipt_failure_reason_v2::response_identity_mismatch,
      receipt_failure_reason_v2::response_object_mismatch,
      receipt_failure_reason_v2::response_context_mismatch,
      receipt_failure_reason_v2::response_signature_mismatch,
      receipt_failure_reason_v2::receipt_encoding_failed,
      receipt_failure_reason_v2::envelope_encoding_failed,
      receipt_failure_reason_v2::local_submission_rejected,
      receipt_failure_reason_v2::cancelled,
      receipt_failure_reason_v2::deadline_expired,
      receipt_failure_reason_v2::internal_execution_failure};
  std::set<std::string> names;
  for (const auto reason : reasons)
  {
    const std::string name = to_string(reason);
    EXPECT_NE("unknown", name);
    EXPECT_NE("none", name);
    EXPECT_TRUE(names.insert(name).second) << name;
  }
  EXPECT_EQ(reasons.size(), names.size());
}

TEST(epose_diagnostics_v2, every_injected_failure_survives_the_step_runner)
{
  using injected = std::pair<receipt_attempt_stage_v2,
      receipt_failure_reason_v2>;
  const std::vector<injected> failures{
      {receipt_attempt_stage_v2::descriptor_lookup,
          receipt_failure_reason_v2::descriptor_unavailable},
      {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_invalid},
      {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_expired},
      {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_commitment_mismatch},
      {receipt_attempt_stage_v2::network_connect,
          receipt_failure_reason_v2::dns_resolution_failed},
      {receipt_attempt_stage_v2::network_connect,
          receipt_failure_reason_v2::connection_refused},
      {receipt_attempt_stage_v2::network_connect,
          receipt_failure_reason_v2::transport_timeout},
      {receipt_attempt_stage_v2::network_connect,
          receipt_failure_reason_v2::transport_failure},
      {receipt_attempt_stage_v2::http_exchange,
          receipt_failure_reason_v2::http_status_unsuccessful},
      {receipt_attempt_stage_v2::http_exchange,
          receipt_failure_reason_v2::remote_challenge_rejected},
      {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed},
      {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_oversized},
      {receipt_attempt_stage_v2::response_validation,
          receipt_failure_reason_v2::response_unexpected},
      {receipt_attempt_stage_v2::response_validation,
          receipt_failure_reason_v2::response_identity_mismatch},
      {receipt_attempt_stage_v2::response_validation,
          receipt_failure_reason_v2::response_object_mismatch},
      {receipt_attempt_stage_v2::response_validation,
          receipt_failure_reason_v2::response_context_mismatch},
      {receipt_attempt_stage_v2::response_validation,
          receipt_failure_reason_v2::response_signature_mismatch},
      {receipt_attempt_stage_v2::receipt_encoding,
          receipt_failure_reason_v2::receipt_encoding_failed},
      {receipt_attempt_stage_v2::envelope_encoding,
          receipt_failure_reason_v2::envelope_encoding_failed},
      {receipt_attempt_stage_v2::local_submission,
          receipt_failure_reason_v2::local_submission_rejected},
      {receipt_attempt_stage_v2::local_submission,
          receipt_failure_reason_v2::deadline_expired},
      {receipt_attempt_stage_v2::none,
          receipt_failure_reason_v2::internal_execution_failure}};
  receipt_diagnostics_v2 diagnostics(64, 64, 100, 42);
  for (size_t index = 0; index < failures.size(); ++index)
  {
    const auto &failure = failures[index];
    bool later_step_called = false;
    const auto result = run_receipt_attempt_steps_v2({
        [failure]() {
          return receipt_step_result_v2{failure.first, failure.second};
        },
        [&later_step_called]() {
          later_step_called = true;
          return receipt_step_result_v2{};
        }}, []() { return false; });
    EXPECT_EQ(failure.second, result.reason);
    EXPECT_EQ(failure.first, result.stage);
    EXPECT_FALSE(later_step_called);

    auto attempt = context(index + 1);
    attempt.slot = hash_text(std::to_string(index).c_str());
    ASSERT_TRUE(diagnostics.started(attempt));
    receipt_attempt_terminal_v2 terminal{};
    terminal.context = attempt;
    terminal.outcome = receipt_attempt_outcome_v2::failure;
    terminal.stage = result.stage;
    terminal.reason = result.reason;
    ASSERT_TRUE(diagnostics.terminal(terminal, index + 1).terminal_recorded);
  }

  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(failures.size(), snapshot.recent_attempts.size());
  for (size_t index = 0; index < failures.size(); ++index)
  {
    EXPECT_EQ(failures[index].first, snapshot.recent_attempts[index].stage);
    EXPECT_EQ(failures[index].second, snapshot.recent_attempts[index].reason);
  }

  const auto cancelled = run_receipt_attempt_steps_v2(
      {[]() { return receipt_step_result_v2{}; }}, []() { return true; });
  EXPECT_EQ(receipt_failure_reason_v2::cancelled, cancelled.reason);
}

TEST(epose_diagnostics_v2, canonical_validation_failures_keep_specific_reasons)
{
  EXPECT_EQ(receipt_failure_reason_v2::none,
      map_canonical_service_failure_v2(canonical_service_status_v2::accepted));
  EXPECT_EQ(receipt_failure_reason_v2::response_context_mismatch,
      map_canonical_service_failure_v2(
          canonical_service_status_v2::invalid_challenge));
  EXPECT_EQ(receipt_failure_reason_v2::response_identity_mismatch,
      map_canonical_service_failure_v2(
          canonical_service_status_v2::wrong_signing_key));
  EXPECT_EQ(receipt_failure_reason_v2::response_object_mismatch,
      map_canonical_service_failure_v2(
          canonical_service_status_v2::wrong_object));
  EXPECT_EQ(receipt_failure_reason_v2::response_signature_mismatch,
      map_canonical_service_failure_v2(
          canonical_service_status_v2::invalid_subject_signature));
}

TEST(epose_diagnostics_v2, transport_errors_keep_specific_reasons)
{
  EXPECT_EQ(receipt_failure_reason_v2::dns_resolution_failed,
      classify_transport_failure_v2(
          boost::asio::error::host_not_found, 10, 100));
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused,
      classify_transport_failure_v2(
          boost::asio::error::connection_refused, 10, 100));
  EXPECT_EQ(receipt_failure_reason_v2::transport_timeout,
      classify_transport_failure_v2(
          boost::asio::error::timed_out, 10, 100));
  EXPECT_EQ(receipt_failure_reason_v2::transport_timeout,
      classify_transport_failure_v2({}, 100, 100));
  EXPECT_EQ(receipt_failure_reason_v2::transport_failure,
      classify_transport_failure_v2(
          boost::asio::error::connection_reset, 10, 100));
}

TEST(epose_diagnostics_v2, http_response_body_limit_is_enforced_before_append)
{
  epee::net_utils::http::http_simple_client client;
  client.set_response_body_limit(4);
  std::string first = "1234";
  EXPECT_TRUE(client.handle_target_data(first));
  EXPECT_FALSE(client.response_body_limit_exceeded());
  std::string overflow = "5";
  EXPECT_FALSE(client.handle_target_data(overflow));
  EXPECT_TRUE(client.response_body_limit_exceeded());
}

TEST(epose_diagnostics_v2, qualification_pending_is_not_final_failure)
{
  qualification_diagnostics_v2 state{};
  EXPECT_STREQ("unknown", qualification_state_v2(state));
  EXPECT_STREQ("unknown", qualification_unmet_rule_v2(state));
  state.available = true;
  EXPECT_STREQ("pending", qualification_state_v2(state));
  EXPECT_STREQ("pending", qualification_unmet_rule_v2(state));
  state.finalized = true;
  EXPECT_STREQ("not_qualified", qualification_state_v2(state));
  EXPECT_STREQ("snapshot_membership", qualification_unmet_rule_v2(state));
  state.subject_in_snapshot = true;
  EXPECT_STREQ("receipt_rounds", qualification_unmet_rule_v2(state));
  state.qualified = true;
  EXPECT_STREQ("qualified", qualification_state_v2(state));
  EXPECT_STREQ("none", qualification_unmet_rule_v2(state));
}

TEST(epose_diagnostics_v2, restart_resets_only_local_operational_counters)
{
  receipt_diagnostics_v2 before_restart(4, 4, 100, 41);
  auto attempt = context();
  attempt.attempt_id = before_restart.next_attempt_id();
  before_restart.started(attempt);
  receipt_attempt_terminal_v2 terminal{};
  terminal.context = attempt;
  terminal.outcome = receipt_attempt_outcome_v2::failure;
  terminal.stage = receipt_attempt_stage_v2::descriptor_lookup;
  terminal.reason = receipt_failure_reason_v2::descriptor_unavailable;
  before_restart.terminal(terminal, 100);

  receipt_diagnostics_v2 after_restart(4, 4, 100, 42);
  EXPECT_EQ(1u, after_restart.next_attempt_id());
  const auto snapshot = after_restart.snapshot();
  EXPECT_EQ(42u, snapshot.reset_utc_ms);
  EXPECT_TRUE(snapshot.rounds.empty());
  EXPECT_TRUE(snapshot.failures.empty());
  EXPECT_TRUE(snapshot.recent_attempts.empty());
}

TEST(epose_diagnostics_v2, descriptor_restart_and_renewal_failures_are_explicit)
{
  receipt_diagnostics_v2 diagnostics(8, 8, 100, 42);
  const std::vector<receipt_failure_reason_v2> reasons{
      receipt_failure_reason_v2::descriptor_unavailable,
      receipt_failure_reason_v2::descriptor_expired,
      receipt_failure_reason_v2::descriptor_commitment_mismatch};
  auto attempt = context();
  for (size_t index = 0; index < reasons.size(); ++index)
  {
    attempt.attempt_id = diagnostics.next_attempt_id();
    attempt.slot = hash_text(std::to_string(index).c_str());
    ASSERT_TRUE(diagnostics.started(attempt));
    receipt_attempt_terminal_v2 terminal{};
    terminal.context = attempt;
    terminal.outcome = receipt_attempt_outcome_v2::failure;
    terminal.stage = index == 0
        ? receipt_attempt_stage_v2::descriptor_lookup
        : receipt_attempt_stage_v2::descriptor_validation;
    terminal.reason = reasons[index];
    ASSERT_TRUE(diagnostics.terminal(terminal, index + 1).terminal_recorded);
  }
  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(reasons.size(), snapshot.failures.size());
  for (size_t index = 0; index < reasons.size(); ++index)
  {
    EXPECT_EQ(reasons[index], snapshot.failures[index].reason);
    EXPECT_EQ(1u, snapshot.failures[index].count);
  }
}

TEST(epose_diagnostics_v2, outcomes_rate_limits_and_recovery_are_bounded)
{
  receipt_diagnostics_v2 diagnostics(2, 2, 100, 42);
  auto attempt = context();
  for (uint64_t index = 0; index < 5; ++index)
  {
    attempt.attempt_id = diagnostics.next_attempt_id();
    diagnostics.started(attempt);
    receipt_attempt_terminal_v2 terminal{};
    terminal.context = attempt;
    terminal.outcome = receipt_attempt_outcome_v2::failure;
    terminal.stage = receipt_attempt_stage_v2::network_connect;
    terminal.reason = receipt_failure_reason_v2::connection_refused;
    terminal.completed_utc_ms = 1100 + index;
    terminal.duration_ms = 10;
    const uint64_t warning_time = index == 4 ? 300 : 100 + index;
    const auto effect = diagnostics.terminal(terminal, warning_time);
    EXPECT_TRUE(effect.terminal_recorded);
    EXPECT_EQ(index == 2 || index == 4, effect.emit_repeated_warning);
    if (index == 4)
    {
      EXPECT_EQ(1u, effect.suppressed_since_last_warning);
    }
  }

  attempt.attempt_id = diagnostics.next_attempt_id();
  diagnostics.started(attempt);
  receipt_attempt_terminal_v2 recovered{};
  recovered.context = attempt;
  recovered.outcome = receipt_attempt_outcome_v2::success;
  recovered.stage = receipt_attempt_stage_v2::local_submission;
  recovered.local_submission_accepted = true;
  const auto effect = diagnostics.terminal(recovered, 205);
  EXPECT_TRUE(effect.terminal_recorded);
  EXPECT_EQ(5u, effect.recovered_after_failures);

  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(1u, snapshot.rounds.size());
  EXPECT_EQ(6u, snapshot.rounds[0].attempts_started);
  EXPECT_EQ(6u, snapshot.rounds[0].attempts_completed);
  EXPECT_EQ(5u, snapshot.rounds[0].attempts_failed);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_succeeded);
  EXPECT_EQ(0u, snapshot.attempts_in_flight);
  EXPECT_EQ(2u, snapshot.recent_attempts.size());
  ASSERT_EQ(1u, snapshot.failures.size());
  EXPECT_EQ(11u, snapshot.failures[0].epoch);
  EXPECT_EQ(1u, snapshot.failures[0].round);
  EXPECT_EQ(5u, snapshot.failures[0].count);
  EXPECT_EQ(1u, snapshot.failures[0].suppressed_log_count);
}

TEST(epose_diagnostics_v2, a_started_attempt_has_exactly_one_terminal_outcome)
{
  receipt_diagnostics_v2 diagnostics(4, 4, 100, 42);
  auto attempt = context();
  EXPECT_TRUE(diagnostics.started(attempt));
  EXPECT_FALSE(diagnostics.started(attempt));

  receipt_attempt_terminal_v2 terminal{};
  terminal.context = attempt;
  terminal.outcome = receipt_attempt_outcome_v2::failure;
  terminal.stage = receipt_attempt_stage_v2::network_connect;
  terminal.reason = receipt_failure_reason_v2::transport_timeout;
  EXPECT_TRUE(diagnostics.terminal(terminal, 100).terminal_recorded);
  EXPECT_FALSE(diagnostics.terminal(terminal, 100).terminal_recorded);

  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(1u, snapshot.rounds.size());
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_started);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_completed);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_failed);
  EXPECT_EQ(0u, snapshot.attempts_in_flight);
  EXPECT_EQ(1u, snapshot.recent_attempts.size());
}

TEST(epose_diagnostics_v2, cancelled_expired_failed_and_success_are_distinct)
{
  receipt_diagnostics_v2 diagnostics(8, 8, 100, 42);
  const std::vector<receipt_attempt_outcome_v2> outcomes{
      receipt_attempt_outcome_v2::success,
      receipt_attempt_outcome_v2::failure,
      receipt_attempt_outcome_v2::cancelled,
      receipt_attempt_outcome_v2::expired};
  auto attempt = context();
  for (size_t index = 0; index < outcomes.size(); ++index)
  {
    attempt.attempt_id = diagnostics.next_attempt_id();
    attempt.slot = hash_text(std::to_string(index).c_str());
    ASSERT_TRUE(diagnostics.started(attempt));
    receipt_attempt_terminal_v2 terminal{};
    terminal.context = attempt;
    terminal.outcome = outcomes[index];
    terminal.stage = index == 0 || index == 3
        ? receipt_attempt_stage_v2::local_submission
        : receipt_attempt_stage_v2::network_connect;
    terminal.reason = index == 0
        ? receipt_failure_reason_v2::none
        : index == 2
            ? receipt_failure_reason_v2::cancelled
            : index == 3
                ? receipt_failure_reason_v2::deadline_expired
                : receipt_failure_reason_v2::transport_failure;
    ASSERT_TRUE(diagnostics.terminal(terminal, index + 1).terminal_recorded);
  }
  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(1u, snapshot.rounds.size());
  EXPECT_EQ(4u, snapshot.rounds[0].attempts_started);
  EXPECT_EQ(4u, snapshot.rounds[0].attempts_completed);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_succeeded);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_failed);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_cancelled);
  EXPECT_EQ(1u, snapshot.rounds[0].attempts_expired);
}

TEST(epose_diagnostics_v2, expected_scheduler_skips_are_not_failures)
{
  receipt_diagnostics_v2 diagnostics(4, 4, 100, 42);
  diagnostics.skipped(receipt_scheduler_skip_v2::not_committee_member);
  diagnostics.skipped(receipt_scheduler_skip_v2::already_canonical);
  diagnostics.skipped(receipt_scheduler_skip_v2::retry_backoff);
  const auto snapshot = diagnostics.snapshot();
  EXPECT_EQ(3u, snapshot.skips.size());
  EXPECT_TRUE(snapshot.failures.empty());
  EXPECT_TRUE(snapshot.rounds.empty());
}

TEST(epose_diagnostics_v2, operational_history_retains_only_four_epochs)
{
  receipt_diagnostics_v2 diagnostics(32, 32, 100, 42);
  for (uint64_t epoch = 1; epoch <= 6; ++epoch)
  {
    auto attempt = context(epoch);
    attempt.epoch = epoch;
    attempt.slot = hash_text(std::to_string(epoch).c_str());
    attempt.attempt_id = diagnostics.next_attempt_id();
    diagnostics.started(attempt);
    receipt_attempt_terminal_v2 terminal{};
    terminal.context = attempt;
    terminal.outcome = receipt_attempt_outcome_v2::failure;
    terminal.stage = receipt_attempt_stage_v2::descriptor_lookup;
    terminal.reason = receipt_failure_reason_v2::descriptor_unavailable;
    diagnostics.terminal(terminal, epoch * 10);
  }
  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(4u, snapshot.rounds.size());
  ASSERT_EQ(4u, snapshot.failures.size());
  EXPECT_EQ(3u, snapshot.rounds.front().epoch);
  EXPECT_EQ(6u, snapshot.rounds.back().epoch);
  EXPECT_EQ(3u, snapshot.failures.front().epoch);
  EXPECT_EQ(6u, snapshot.failures.back().epoch);
}

TEST(epose_diagnostics_v2, late_terminal_outside_retention_does_not_resurrect_epoch)
{
  receipt_diagnostics_v2 diagnostics(32, 32, 100, 42);
  for (uint64_t epoch = 3; epoch <= 6; ++epoch)
  {
    auto attempt = context(epoch);
    attempt.epoch = epoch;
    attempt.slot = hash_text(std::to_string(epoch).c_str());
    attempt.attempt_id = diagnostics.next_attempt_id();
    ASSERT_TRUE(diagnostics.started(attempt));
    receipt_attempt_terminal_v2 terminal{};
    terminal.context = attempt;
    terminal.outcome = receipt_attempt_outcome_v2::success;
    ASSERT_TRUE(diagnostics.terminal(terminal, epoch).terminal_recorded);
  }

  auto late = context(100);
  late.epoch = 1;
  late.slot = hash_text("late-old-epoch");
  late.attempt_id = diagnostics.next_attempt_id();
  ASSERT_TRUE(diagnostics.started(late));
  receipt_attempt_terminal_v2 terminal{};
  terminal.context = late;
  terminal.outcome = receipt_attempt_outcome_v2::failure;
  terminal.stage = receipt_attempt_stage_v2::network_connect;
  terminal.reason = receipt_failure_reason_v2::transport_timeout;
  ASSERT_TRUE(diagnostics.terminal(terminal, 100).terminal_recorded);

  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(4u, snapshot.rounds.size());
  EXPECT_EQ(3u, snapshot.rounds.front().epoch);
  EXPECT_TRUE(snapshot.failures.empty());
}

TEST(epose_diagnostics_v2, canonical_inclusion_is_distinct_and_idempotent)
{
  receipt_diagnostics_v2 diagnostics(4, 4, 100, 42);
  auto attempt = context();
  diagnostics.started(attempt);
  receipt_attempt_terminal_v2 submitted{};
  submitted.context = attempt;
  submitted.outcome = receipt_attempt_outcome_v2::success;
  submitted.stage = receipt_attempt_stage_v2::local_submission;
  submitted.local_submission_accepted = true;
  diagnostics.terminal(submitted, 100);
  EXPECT_TRUE(diagnostics.canonical_inclusion(11, 1, attempt.slot));
  EXPECT_FALSE(diagnostics.canonical_inclusion(11, 1, attempt.slot));
  const auto snapshot = diagnostics.snapshot();
  ASSERT_EQ(1u, snapshot.rounds.size());
  EXPECT_EQ(1u, snapshot.rounds[0].local_submissions_accepted);
  EXPECT_EQ(1u, snapshot.rounds[0].canonical_inclusions_observed);
  ASSERT_EQ(1u, snapshot.recent_attempts.size());
  EXPECT_TRUE(snapshot.recent_attempts[0].canonical_inclusion_observed);
}
