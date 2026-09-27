// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <boost/system/error_code.hpp>

#include "crypto/crypto.h"
#include "cryptonote_basic/blobdatatype.h"
#include "epose/canonical_service_v2.h"

namespace qwertycoin
{
namespace epose
{
  // These names are an additive operator API. Do not rename existing values;
  // append a new value when a genuinely new failure boundary is introduced.
  enum class receipt_attempt_stage_v2 : uint8_t
  {
    none = 0,
    descriptor_lookup,
    descriptor_validation,
    network_connect,
    http_exchange,
    response_decode,
    response_validation,
    receipt_encoding,
    envelope_encoding,
    local_submission,
    canonical_inclusion
  };

  enum class receipt_failure_reason_v2 : uint8_t
  {
    none = 0,
    descriptor_unavailable,
    descriptor_invalid,
    descriptor_expired,
    descriptor_commitment_mismatch,
    dns_resolution_failed,
    connection_refused,
    transport_timeout,
    transport_failure,
    http_status_unsuccessful,
    remote_challenge_rejected,
    response_malformed,
    response_oversized,
    response_unexpected,
    response_identity_mismatch,
    response_object_mismatch,
    response_context_mismatch,
    response_signature_mismatch,
    receipt_encoding_failed,
    envelope_encoding_failed,
    local_submission_rejected,
    cancelled,
    deadline_expired,
    internal_execution_failure
  };

  enum class receipt_attempt_outcome_v2 : uint8_t
  {
    success = 0,
    failure,
    cancelled,
    expired
  };

  enum class receipt_scheduler_skip_v2 : uint8_t
  {
    not_local_member = 0,
    not_committee_member,
    already_canonical,
    retry_backoff,
    no_active_round
  };

  const char *to_string(receipt_attempt_stage_v2 value);
  const char *to_string(receipt_failure_reason_v2 value);
  const char *to_string(receipt_attempt_outcome_v2 value);
  const char *to_string(receipt_scheduler_skip_v2 value);

  receipt_failure_reason_v2 map_canonical_service_failure_v2(
      canonical_service_status_v2 status);

  receipt_failure_reason_v2 classify_transport_failure_v2(
      const boost::system::error_code &error,
      uint64_t duration_ms,
      uint64_t timeout_ms);

  struct receipt_step_result_v2
  {
    receipt_attempt_stage_v2 stage = receipt_attempt_stage_v2::none;
    receipt_failure_reason_v2 reason = receipt_failure_reason_v2::none;

    bool accepted() const
    {
      return reason == receipt_failure_reason_v2::none;
    }
  };

  struct receipt_attempt_context_v2
  {
    uint64_t attempt_id = 0;
    uint64_t epoch = 0;
    uint64_t round = 0;
    uint64_t chain_height = 0;
    uint64_t deadline_height = 0;
    crypto::hash slot{};
    crypto::public_key subject_identity{};
    crypto::public_key verifier_identity{};
    crypto::hash endpoint_commitment{};
    uint64_t started_utc_ms = 0;
    uint64_t started_steady_ms = 0;
  };

  struct receipt_attempt_terminal_v2
  {
    receipt_attempt_context_v2 context{};
    receipt_attempt_outcome_v2 outcome = receipt_attempt_outcome_v2::failure;
    receipt_attempt_stage_v2 stage = receipt_attempt_stage_v2::none;
    receipt_failure_reason_v2 reason = receipt_failure_reason_v2::none;
    uint64_t completed_utc_ms = 0;
    uint64_t duration_ms = 0;
    bool retry_scheduled = false;
    uint64_t next_retry_utc_ms = 0;
    bool local_submission_accepted = false;
    bool local_submission_relayed = false;
    bool canonical_inclusion_observed = false;
  };

  struct receipt_failure_counter_v2
  {
    uint64_t epoch = 0;
    uint64_t round = 0;
    receipt_attempt_stage_v2 stage = receipt_attempt_stage_v2::none;
    receipt_failure_reason_v2 reason = receipt_failure_reason_v2::none;
    uint64_t count = 0;
    uint64_t suppressed_log_count = 0;
  };

  struct receipt_round_counters_v2
  {
    uint64_t epoch = 0;
    uint64_t round = 0;
    uint64_t attempts_started = 0;
    uint64_t attempts_completed = 0;
    uint64_t attempts_succeeded = 0;
    uint64_t attempts_failed = 0;
    uint64_t attempts_cancelled = 0;
    uint64_t attempts_expired = 0;
    uint64_t local_submissions_accepted = 0;
    uint64_t canonical_inclusions_observed = 0;
  };

  struct receipt_skip_counter_v2
  {
    receipt_scheduler_skip_v2 reason = receipt_scheduler_skip_v2::no_active_round;
    uint64_t count = 0;
  };

  struct receipt_diagnostics_snapshot_v2
  {
    uint64_t reset_utc_ms = 0;
    uint64_t attempts_in_flight = 0;
    std::vector<receipt_round_counters_v2> rounds;
    std::vector<receipt_failure_counter_v2> failures;
    std::vector<receipt_skip_counter_v2> skips;
    std::vector<receipt_attempt_terminal_v2> recent_attempts;
  };

  struct receipt_diagnostic_effect_v2
  {
    bool terminal_recorded = false;
    bool emit_repeated_warning = false;
    uint64_t suppressed_since_last_warning = 0;
    uint64_t recovered_after_failures = 0;
  };

  struct qualification_diagnostics_v2
  {
    bool available = false;
    bool finalized = false;
    bool qualified = false;
    bool subject_in_snapshot = false;
    uint64_t epoch = 0;
    uint64_t chain_height = 0;
    uint64_t evidence_deadline_height = 0;
    uint64_t rounds_required = 0;
    uint64_t rounds_passed = 0;
    uint64_t rounds_remaining = 0;
    crypto::public_key subject_identity{};
    std::vector<uint64_t> canonical_unique_receipts;
    std::vector<uint64_t> committee_sizes;
    std::vector<uint64_t> required_receipts;
  };

  const char *qualification_state_v2(
      const qualification_diagnostics_v2 &qualification);
  const char *qualification_unmet_rule_v2(
      const qualification_diagnostics_v2 &qualification);

  // The current producer is serial, but the collector is synchronized because
  // the unrestricted administrative RPC may read it concurrently.
  class receipt_diagnostics_v2
  {
  public:
    explicit receipt_diagnostics_v2(
        size_t max_recent_attempts = 256,
        size_t max_slot_states = 4096,
        uint64_t repeated_warning_interval_ms = 300000,
        uint64_t reset_utc_ms = 0);

    uint64_t next_attempt_id();
    bool started(const receipt_attempt_context_v2 &context);
    receipt_diagnostic_effect_v2 terminal(
        const receipt_attempt_terminal_v2 &terminal,
        uint64_t warning_now_steady_ms);
    bool canonical_inclusion(
        uint64_t epoch, uint64_t round, const crypto::hash &slot);
    void skipped(receipt_scheduler_skip_v2 reason);
    receipt_diagnostics_snapshot_v2 snapshot() const;

  private:
    struct slot_state
    {
      crypto::hash slot{};
      receipt_attempt_stage_v2 last_failure_stage = receipt_attempt_stage_v2::none;
      receipt_failure_reason_v2 last_failure_reason = receipt_failure_reason_v2::none;
      uint64_t consecutive_failures = 0;
      uint64_t last_warning_steady_ms = 0;
      uint64_t suppressed_since_warning = 0;
    };

    receipt_round_counters_v2 &round_locked(uint64_t epoch, uint64_t round);
    receipt_failure_counter_v2 &failure_locked(
        uint64_t epoch, uint64_t round,
        receipt_attempt_stage_v2 stage, receipt_failure_reason_v2 reason);
    slot_state &slot_locked(const crypto::hash &slot);
    void prune_rounds_locked();

    const size_t max_recent_attempts_;
    const size_t max_slot_states_;
    const uint64_t repeated_warning_interval_ms_;
    mutable std::mutex mutex_;
    uint64_t reset_utc_ms_ = 0;
    uint64_t next_attempt_id_ = 1;
    uint64_t in_flight_ = 0;
    std::vector<receipt_round_counters_v2> rounds_;
    receipt_round_counters_v2 discarded_round_{};
    std::vector<receipt_failure_counter_v2> failures_;
    receipt_failure_counter_v2 discarded_failure_{};
    std::vector<receipt_skip_counter_v2> skips_;
    std::deque<receipt_attempt_terminal_v2> recent_attempts_;
    std::vector<uint64_t> active_attempt_ids_;
    std::vector<slot_state> slots_;
    std::vector<crypto::hash> canonical_slots_;
  };

  using receipt_attempt_step_v2 = std::function<receipt_step_result_v2()>;

  // Small dependency-injected runner used by the daemon and deterministic
  // fault tests. It never changes retry or consensus behavior.
  receipt_step_result_v2 run_receipt_attempt_steps_v2(
      const std::vector<receipt_attempt_step_v2> &steps,
      const std::function<bool()> &cancelled);

} // namespace epose
} // namespace qwertycoin
