// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "epose/envelope_v2.h"
#include "epose/membership_v2.h"
#include "epose/resource_policy_v2.h"

namespace qwertycoin
{
namespace epose
{
  enum class relay_record_status_v2
  {
    accepted,
    idempotent_duplicate,
    invalid_configuration,
    invalid_record,
    payment_proof_forbidden,
    expired,
    full,
    conflict
  };

  struct relay_record_selection_v2
  {
    crypto::hash id{};
    envelope_record_v2 record{};
  };

  // Local policy cache for already signed, self-contained records. It is not
  // consensus state: complete blocks are always parsed and validated again.
  class relay_record_pool_v2
  {
  public:
    relay_record_pool_v2(
        const epoch_timing_v2 &timing,
        const envelope_limits_v2 &envelope_limits,
        const relay_queue_limits_v2 &queue_limits,
        const relay_template_limits_v2 &template_limits);

    bool valid() const;
    relay_record_status_v2 enqueue(
        const envelope_record_v2 &record,
        uint64_t current_height);
    void prune_expired(uint64_t current_height);
    relay_record_status_v2 select_for_template(
        uint64_t current_height,
        std::vector<relay_record_selection_v2> &selected) const;
    void erase_confirmed(const std::vector<crypto::hash> &ids);
    size_t size() const;
    size_t bytes() const;

  private:
    struct stored_record_v2
    {
      crypto::hash id{};
      envelope_record_v2 record{};
      uint64_t deadline_height = 0;
    };

    bool describe(
        const envelope_record_v2 &record,
        relay_class_v2 &record_class,
        uint64_t &deadline_height) const;

    epoch_timing_v2 timing_{};
    envelope_limits_v2 envelope_limits_{};
    relay_queue_limits_v2 queue_limits_{};
    relay_template_limits_v2 template_limits_{};
    deadline_relay_queue_v2 queue_;
    std::vector<stored_record_v2> records_;
  };
} // namespace epose
} // namespace qwertycoin
