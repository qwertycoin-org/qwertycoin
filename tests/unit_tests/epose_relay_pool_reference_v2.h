// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// Test-only reference: the relay pool and admission algorithm exactly as they
// were before the pool moved off the chain lock (full copy, full prune before
// every admission). Property tests compare the incremental pool against it.

#pragma once

#include "epose/relay_pool_v2.h"

namespace qwertycoin
{
namespace epose
{
namespace reference_v2
{
  // Semantic apply_transaction calls made by the reference algorithm
  // (admission and prune), for the microbenchmark. Not thread safe.
  uint64_t &verification_count();

  class relay_record_pool_v2
  {
  public:
    relay_record_pool_v2(
        const epoch_timing_v2 &timing,
        const committee_policy_v2 &committee_policy,
        const envelope_limits_v2 &envelope_limits,
        const relay_queue_limits_v2 &queue_limits,
        const relay_template_limits_v2 &template_limits,
        uint64_t receipt_slot_dedup_height =
            std::numeric_limits<uint64_t>::max());

    bool valid() const;
    relay_record_status_v2 enqueue(
        const envelope_record_v2 &record,
        uint64_t current_height);
    void prune_expired(uint64_t current_height);
    void prune_unusable_receipts(
        uint64_t current_height,
        const semantic_state_v2 &canonical_state,
        const canonical_context_source_v2 &contexts);
    relay_record_status_v2 select_for_template(
        uint64_t current_height,
        std::vector<relay_record_selection_v2> &selected,
        const semantic_state_v2 *canonical_state = nullptr);
    void erase_confirmed(const std::vector<crypto::hash> &ids);
    void erase_confirmed_records(
        const std::vector<envelope_record_v2> &records,
        uint64_t current_height);
    relay_pool_diagnostics_v2 diagnostics() const;
    void record_ingress_rejection(relay_record_status_v2 status);
    size_t size() const;
    size_t bytes() const;
    std::vector<crypto::hash> record_ids() const;

  private:
    struct stored_record_v2
    {
      crypto::hash id{};
      envelope_record_v2 record{};
      uint64_t deadline_height = 0;
      std::string receipt_slot{};
      std::string receipt_context{};
    };

    bool describe(
        const envelope_record_v2 &record,
        uint64_t current_height,
        relay_class_v2 &record_class,
        uint64_t &deadline_height) const;
    void compact_receipt_slots(uint64_t current_height);

    epoch_timing_v2 timing_{};
    committee_policy_v2 committee_policy_{};
    envelope_limits_v2 envelope_limits_{};
    relay_queue_limits_v2 queue_limits_{};
    relay_template_limits_v2 template_limits_{};
    uint64_t receipt_slot_dedup_height_ =
        std::numeric_limits<uint64_t>::max();
    deadline_relay_queue_v2 queue_;
    std::vector<stored_record_v2> records_;
    relay_pool_diagnostics_v2 diagnostics_{};
  };

  relay_ingress_status_v2 admit_relay_envelopes_v2(
      const std::vector<std::string> &envelopes,
      uint64_t inclusion_height,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts,
      relay_record_pool_v2 &pool,
      std::vector<std::string> &accepted);
} // namespace reference_v2
} // namespace epose
} // namespace qwertycoin
