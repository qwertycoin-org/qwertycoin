// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/diagnostics_v2.h"

#include <algorithm>
#include <chrono>
#include <limits>

#include <boost/asio/error.hpp>

namespace
{
  uint64_t utc_milliseconds()
  {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  }

  bool same_hash(const crypto::hash &left, const crypto::hash &right)
  {
    return left == right;
  }
}

namespace qwertycoin
{
namespace epose
{
  const char *to_string(const receipt_attempt_stage_v2 value)
  {
    switch (value)
    {
      case receipt_attempt_stage_v2::none: return "none";
      case receipt_attempt_stage_v2::descriptor_lookup: return "descriptor_lookup";
      case receipt_attempt_stage_v2::descriptor_validation: return "descriptor_validation";
      case receipt_attempt_stage_v2::network_connect: return "network_connect";
      case receipt_attempt_stage_v2::http_exchange: return "http_exchange";
      case receipt_attempt_stage_v2::response_decode: return "response_decode";
      case receipt_attempt_stage_v2::response_validation: return "response_validation";
      case receipt_attempt_stage_v2::receipt_encoding: return "receipt_encoding";
      case receipt_attempt_stage_v2::envelope_encoding: return "envelope_encoding";
      case receipt_attempt_stage_v2::local_submission: return "local_submission";
      case receipt_attempt_stage_v2::canonical_inclusion: return "canonical_inclusion";
    }
    return "unknown";
  }

  const char *to_string(const receipt_failure_reason_v2 value)
  {
    switch (value)
    {
      case receipt_failure_reason_v2::none: return "none";
      case receipt_failure_reason_v2::descriptor_unavailable: return "descriptor_unavailable";
      case receipt_failure_reason_v2::descriptor_invalid: return "descriptor_invalid";
      case receipt_failure_reason_v2::descriptor_expired: return "descriptor_expired";
      case receipt_failure_reason_v2::descriptor_commitment_mismatch: return "descriptor_commitment_mismatch";
      case receipt_failure_reason_v2::dns_resolution_failed: return "dns_resolution_failed";
      case receipt_failure_reason_v2::connection_refused: return "connection_refused";
      case receipt_failure_reason_v2::transport_timeout: return "transport_timeout";
      case receipt_failure_reason_v2::transport_failure: return "transport_failure";
      case receipt_failure_reason_v2::http_status_unsuccessful: return "http_status_unsuccessful";
      case receipt_failure_reason_v2::remote_challenge_rejected: return "remote_challenge_rejected";
      case receipt_failure_reason_v2::response_malformed: return "response_malformed";
      case receipt_failure_reason_v2::response_oversized: return "response_oversized";
      case receipt_failure_reason_v2::response_unexpected: return "response_unexpected";
      case receipt_failure_reason_v2::response_identity_mismatch: return "response_identity_mismatch";
      case receipt_failure_reason_v2::response_object_mismatch: return "response_object_mismatch";
      case receipt_failure_reason_v2::response_context_mismatch: return "response_context_mismatch";
      case receipt_failure_reason_v2::response_signature_mismatch: return "response_signature_mismatch";
      case receipt_failure_reason_v2::receipt_encoding_failed: return "receipt_encoding_failed";
      case receipt_failure_reason_v2::envelope_encoding_failed: return "envelope_encoding_failed";
      case receipt_failure_reason_v2::local_submission_rejected: return "local_submission_rejected";
      case receipt_failure_reason_v2::cancelled: return "cancelled";
      case receipt_failure_reason_v2::deadline_expired: return "deadline_expired";
      case receipt_failure_reason_v2::internal_execution_failure: return "internal_execution_failure";
    }
    return "unknown";
  }

  const char *to_string(const receipt_attempt_outcome_v2 value)
  {
    switch (value)
    {
      case receipt_attempt_outcome_v2::success: return "success";
      case receipt_attempt_outcome_v2::failure: return "failure";
      case receipt_attempt_outcome_v2::cancelled: return "cancelled";
      case receipt_attempt_outcome_v2::expired: return "expired";
    }
    return "unknown";
  }

  const char *to_string(const receipt_scheduler_skip_v2 value)
  {
    switch (value)
    {
      case receipt_scheduler_skip_v2::not_local_member: return "not_local_member";
      case receipt_scheduler_skip_v2::not_committee_member: return "not_committee_member";
      case receipt_scheduler_skip_v2::already_canonical: return "already_canonical";
      case receipt_scheduler_skip_v2::retry_backoff: return "retry_backoff";
      case receipt_scheduler_skip_v2::no_active_round: return "no_active_round";
    }
    return "unknown";
  }

  receipt_failure_reason_v2 map_canonical_service_failure_v2(
      const canonical_service_status_v2 status)
  {
    switch (status)
    {
      case canonical_service_status_v2::accepted:
        return receipt_failure_reason_v2::none;
      case canonical_service_status_v2::invalid_configuration:
        return receipt_failure_reason_v2::internal_execution_failure;
      case canonical_service_status_v2::invalid_challenge:
      case canonical_service_status_v2::unauthorized_challenge:
        return receipt_failure_reason_v2::response_context_mismatch;
      case canonical_service_status_v2::wrong_signing_key:
        return receipt_failure_reason_v2::response_identity_mismatch;
      case canonical_service_status_v2::object_not_found:
        return receipt_failure_reason_v2::response_unexpected;
      case canonical_service_status_v2::object_too_large:
        return receipt_failure_reason_v2::response_oversized;
      case canonical_service_status_v2::malformed_object:
        return receipt_failure_reason_v2::response_malformed;
      case canonical_service_status_v2::wrong_object:
        return receipt_failure_reason_v2::response_object_mismatch;
      case canonical_service_status_v2::invalid_subject_signature:
        return receipt_failure_reason_v2::response_signature_mismatch;
    }
    return receipt_failure_reason_v2::internal_execution_failure;
  }

  receipt_failure_reason_v2 classify_transport_failure_v2(
      const boost::system::error_code &error,
      const uint64_t duration_ms,
      const uint64_t timeout_ms)
  {
    if (error == boost::asio::error::host_not_found
        || error == boost::asio::error::host_not_found_try_again)
      return receipt_failure_reason_v2::dns_resolution_failed;
    if (error == boost::asio::error::connection_refused)
      return receipt_failure_reason_v2::connection_refused;
    if (error == boost::asio::error::timed_out
        || error == boost::asio::error::operation_aborted
        || duration_ms >= timeout_ms)
      return receipt_failure_reason_v2::transport_timeout;
    return receipt_failure_reason_v2::transport_failure;
  }

  const char *qualification_state_v2(
      const qualification_diagnostics_v2 &qualification)
  {
    if (!qualification.available)
      return "unknown";
    if (!qualification.finalized)
      return "pending";
    return qualification.qualified ? "qualified" : "not_qualified";
  }

  const char *qualification_unmet_rule_v2(
      const qualification_diagnostics_v2 &qualification)
  {
    if (!qualification.available)
      return "unknown";
    if (!qualification.finalized)
      return "pending";
    if (qualification.qualified)
      return "none";
    return qualification.subject_in_snapshot
        ? "receipt_rounds" : "snapshot_membership";
  }

  receipt_diagnostics_v2::receipt_diagnostics_v2(
      const size_t max_recent_attempts,
      const size_t max_slot_states,
      const uint64_t repeated_warning_interval_ms,
      const uint64_t reset_utc_ms)
    : max_recent_attempts_(max_recent_attempts),
      max_slot_states_(std::max<size_t>(1, max_slot_states)),
      repeated_warning_interval_ms_(repeated_warning_interval_ms),
      reset_utc_ms_(reset_utc_ms == 0 ? utc_milliseconds() : reset_utc_ms)
  {
  }

  uint64_t receipt_diagnostics_v2::next_attempt_id()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t result = next_attempt_id_;
    if (next_attempt_id_ != std::numeric_limits<uint64_t>::max())
      ++next_attempt_id_;
    return result;
  }

  receipt_round_counters_v2 &receipt_diagnostics_v2::round_locked(
      const uint64_t epoch, const uint64_t round)
  {
    auto found = std::find_if(rounds_.begin(), rounds_.end(),
        [epoch, round](const receipt_round_counters_v2 &entry) {
          return entry.epoch == epoch && entry.round == round;
        });
    if (found != rounds_.end())
      return *found;

    if (!rounds_.empty())
    {
      const uint64_t newest_epoch = std::max_element(
          rounds_.begin(), rounds_.end(), [](const auto &left, const auto &right) {
            return left.epoch < right.epoch;
          })->epoch;
      if (newest_epoch > 3 && epoch < newest_epoch - 3)
      {
        // A late completion outside the bounded retention window must not
        // resurrect old epochs or be attributed to a retained one.
        discarded_round_ = {};
        discarded_round_.epoch = epoch;
        discarded_round_.round = round;
        return discarded_round_;
      }
    }
    rounds_.push_back({});
    rounds_.back().epoch = epoch;
    rounds_.back().round = round;
    prune_rounds_locked();
    return *std::find_if(rounds_.begin(), rounds_.end(),
        [epoch, round](const receipt_round_counters_v2 &entry) {
          return entry.epoch == epoch && entry.round == round;
        });
  }

  receipt_failure_counter_v2 &receipt_diagnostics_v2::failure_locked(
      const uint64_t epoch,
      const uint64_t round,
      const receipt_attempt_stage_v2 stage,
      const receipt_failure_reason_v2 reason)
  {
    if (!rounds_.empty())
    {
      const uint64_t newest_epoch = std::max_element(
          rounds_.begin(), rounds_.end(), [](const auto &left, const auto &right) {
            return left.epoch < right.epoch;
          })->epoch;
      if (newest_epoch > 3 && epoch < newest_epoch - 3)
      {
        discarded_failure_ = {epoch, round, stage, reason, 0, 0};
        return discarded_failure_;
      }
    }
    const auto found = std::find_if(failures_.begin(), failures_.end(),
        [epoch, round, stage, reason](const receipt_failure_counter_v2 &entry) {
          return entry.epoch == epoch && entry.round == round
              && entry.stage == stage && entry.reason == reason;
        });
    if (found != failures_.end())
      return *found;
    failures_.push_back({epoch, round, stage, reason, 0, 0});
    return failures_.back();
  }

  receipt_diagnostics_v2::slot_state &receipt_diagnostics_v2::slot_locked(
      const crypto::hash &slot)
  {
    const auto found = std::find_if(slots_.begin(), slots_.end(),
        [&slot](const slot_state &entry) { return same_hash(entry.slot, slot); });
    if (found != slots_.end())
      return *found;
    if (max_slot_states_ != 0 && slots_.size() >= max_slot_states_)
      slots_.erase(slots_.begin());
    slots_.push_back({});
    slots_.back().slot = slot;
    return slots_.back();
  }

  void receipt_diagnostics_v2::prune_rounds_locked()
  {
    if (rounds_.empty())
      return;
    const uint64_t newest_epoch = std::max_element(
        rounds_.begin(), rounds_.end(), [](const auto &left, const auto &right) {
          return left.epoch < right.epoch;
        })->epoch;
    const uint64_t first_retained = newest_epoch > 3 ? newest_epoch - 3 : 0;
    rounds_.erase(std::remove_if(rounds_.begin(), rounds_.end(),
        [first_retained](const receipt_round_counters_v2 &entry) {
          return entry.epoch < first_retained;
        }), rounds_.end());
    failures_.erase(std::remove_if(failures_.begin(), failures_.end(),
        [first_retained](const receipt_failure_counter_v2 &entry) {
          return entry.epoch < first_retained;
        }), failures_.end());
  }

  bool receipt_diagnostics_v2::started(
      const receipt_attempt_context_v2 &context)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (context.attempt_id == 0
        || std::find(active_attempt_ids_.begin(), active_attempt_ids_.end(),
               context.attempt_id) != active_attempt_ids_.end())
      return false;
    if (active_attempt_ids_.size() >= max_slot_states_)
      return false;
    active_attempt_ids_.push_back(context.attempt_id);
    ++round_locked(context.epoch, context.round).attempts_started;
    in_flight_ = active_attempt_ids_.size();
    return true;
  }

  receipt_diagnostic_effect_v2 receipt_diagnostics_v2::terminal(
      const receipt_attempt_terminal_v2 &terminal,
      const uint64_t warning_now_steady_ms)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    receipt_diagnostic_effect_v2 effect{};
    const auto active = std::find(active_attempt_ids_.begin(),
        active_attempt_ids_.end(), terminal.context.attempt_id);
    if (active == active_attempt_ids_.end())
      return effect;
    active_attempt_ids_.erase(active);
    effect.terminal_recorded = true;
    receipt_round_counters_v2 &round = round_locked(
        terminal.context.epoch, terminal.context.round);
    ++round.attempts_completed;
    in_flight_ = active_attempt_ids_.size();

    slot_state &slot = slot_locked(terminal.context.slot);
    if (terminal.outcome == receipt_attempt_outcome_v2::success)
    {
      ++round.attempts_succeeded;
      if (terminal.local_submission_accepted)
        ++round.local_submissions_accepted;
      effect.recovered_after_failures = slot.consecutive_failures;
      slot.consecutive_failures = 0;
      slot.suppressed_since_warning = 0;
      slot.last_failure_stage = receipt_attempt_stage_v2::none;
      slot.last_failure_reason = receipt_failure_reason_v2::none;
    }
    else if (terminal.outcome == receipt_attempt_outcome_v2::cancelled)
    {
      ++round.attempts_cancelled;
    }
    else if (terminal.outcome == receipt_attempt_outcome_v2::expired)
    {
      ++round.attempts_expired;
    }
    else
    {
      ++round.attempts_failed;
    }

    if (terminal.outcome != receipt_attempt_outcome_v2::success)
    {
      receipt_failure_counter_v2 &failure = failure_locked(
          terminal.context.epoch, terminal.context.round,
          terminal.stage, terminal.reason);
      ++failure.count;
      if (terminal.outcome == receipt_attempt_outcome_v2::failure)
      {
        if (slot.last_failure_stage != terminal.stage
            || slot.last_failure_reason != terminal.reason)
        {
          slot.last_failure_stage = terminal.stage;
          slot.last_failure_reason = terminal.reason;
          slot.consecutive_failures = 0;
          slot.last_warning_steady_ms = 0;
          slot.suppressed_since_warning = 0;
        }
        if (slot.consecutive_failures != std::numeric_limits<uint64_t>::max())
          ++slot.consecutive_failures;
        const bool first_repeated = slot.consecutive_failures == 3;
        const bool interval_elapsed = slot.last_warning_steady_ms != 0
            && warning_now_steady_ms >= slot.last_warning_steady_ms
            && warning_now_steady_ms - slot.last_warning_steady_ms
                >= repeated_warning_interval_ms_;
        if (first_repeated || interval_elapsed)
        {
          effect.emit_repeated_warning = true;
          effect.suppressed_since_last_warning = slot.suppressed_since_warning;
          failure.suppressed_log_count += slot.suppressed_since_warning;
          slot.suppressed_since_warning = 0;
          slot.last_warning_steady_ms = warning_now_steady_ms;
        }
        else if (slot.consecutive_failures > 3)
          ++slot.suppressed_since_warning;
      }
    }

    if (max_recent_attempts_ != 0)
    {
      if (recent_attempts_.size() >= max_recent_attempts_)
        recent_attempts_.pop_front();
      recent_attempts_.push_back(terminal);
    }
    return effect;
  }

  bool receipt_diagnostics_v2::canonical_inclusion(
      const uint64_t epoch, const uint64_t round, const crypto::hash &slot)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(canonical_slots_.begin(), canonical_slots_.end(), slot)
        != canonical_slots_.end())
      return false;
    if (max_slot_states_ != 0 && canonical_slots_.size() >= max_slot_states_)
      canonical_slots_.erase(canonical_slots_.begin());
    canonical_slots_.push_back(slot);
    ++round_locked(epoch, round).canonical_inclusions_observed;
    for (receipt_attempt_terminal_v2 &attempt : recent_attempts_)
      if (same_hash(attempt.context.slot, slot))
        attempt.canonical_inclusion_observed = true;
    return true;
  }

  void receipt_diagnostics_v2::skipped(const receipt_scheduler_skip_v2 reason)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = std::find_if(skips_.begin(), skips_.end(),
        [reason](const receipt_skip_counter_v2 &entry) {
          return entry.reason == reason;
        });
    if (found == skips_.end())
      skips_.push_back({reason, 1});
    else if (found->count != std::numeric_limits<uint64_t>::max())
      ++found->count;
  }

  receipt_diagnostics_snapshot_v2 receipt_diagnostics_v2::snapshot() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    receipt_diagnostics_snapshot_v2 result{};
    result.reset_utc_ms = reset_utc_ms_;
    result.attempts_in_flight = in_flight_;
    result.rounds = rounds_;
    result.failures = failures_;
    result.skips = skips_;
    result.recent_attempts.assign(
        recent_attempts_.begin(), recent_attempts_.end());
    return result;
  }

  receipt_step_result_v2 run_receipt_attempt_steps_v2(
      const std::vector<receipt_attempt_step_v2> &steps,
      const std::function<bool()> &cancelled)
  {
    try
    {
      for (const receipt_attempt_step_v2 &step : steps)
      {
        if (cancelled && cancelled())
          return {receipt_attempt_stage_v2::none,
              receipt_failure_reason_v2::cancelled};
        if (!step)
          return {receipt_attempt_stage_v2::none,
              receipt_failure_reason_v2::internal_execution_failure};
        const receipt_step_result_v2 result = step();
        if (!result.accepted())
          return result;
      }
    }
    catch (...)
    {
      return {receipt_attempt_stage_v2::none,
          receipt_failure_reason_v2::internal_execution_failure};
    }
    return {};
  }

} // namespace epose
} // namespace qwertycoin
