// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "epose/envelope_v2.h"
#include "epose/membership_v2.h"
#include "epose/resource_policy_v2.h"
#include "epose/semantic_batch_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct relay_policy_v2
  {
    relay_queue_limits_v2 queue{};
    relay_template_limits_v2 mining_template{};
    uint64_t receipt_slot_dedup_height =
        std::numeric_limits<uint64_t>::max();

    bool valid() const;
  };

  // Returns false until the same reviewed launch manifest that supplies the
  // consensus parameters also supplies all local relay/template bounds.
  bool compiled_relay_policy_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      relay_policy_v2 &policy);

  enum class relay_record_status_v2
  {
    accepted,
    // The exact authenticated wire envelope is already pending locally. It
    // may be re-forwarded under the bounded transport policy without adding
    // another queue entry.
    idempotent_duplicate,
    // A different authenticated envelope occupies the same logical receipt
    // slot in the same canonical context. Keep the existing queue entry and
    // do not forward the randomized signature variant.
    receipt_slot_variant,
    invalid_configuration,
    invalid_record,
    payment_proof_forbidden,
    expired,
    full,
    conflict
  };

  enum class relay_ingress_status_v2
  {
    accepted,
    invalid_configuration,
    invalid_batch,
    expired,
    capacity_exhausted,
    outside_retained_window,
    context_unavailable
  };

  constexpr bool local_ingress_is_expected_race_v2(relay_ingress_status_v2 status)
  {
    return status == relay_ingress_status_v2::expired
        || status == relay_ingress_status_v2::outside_retained_window;
  }

  enum class relay_ingress_rejection_v2
  {
    none,
    invalid_pool_or_policy,
    envelope_budget,
    envelope_parse,
    receipt_structure,
    receipt_window_unavailable,
    receipt_late_unauthenticated,
    receipt_slot_unauthenticated,
    semantic_rejected,
    queue_expired,
    queue_full,
    queue_rejected,
    receipt_window_closed
  };

  struct relay_ingress_trace_v2
  {
    relay_ingress_rejection_v2 rejection = relay_ingress_rejection_v2::none;
    semantic_status_v2 semantic = semantic_status_v2::accepted;
    lifecycle_status_v2 lifecycle = lifecycle_status_v2::accepted;
    size_t envelope_index = 0;
  };

  struct relay_record_selection_v2
  {
    crypto::hash id{};
    envelope_record_v2 record{};
  };

  struct relay_pool_diagnostics_v2
  {
    uint64_t queue_items = 0;
    uint64_t queue_bytes = 0;
    uint64_t exact_duplicates = 0;
    uint64_t slot_variants = 0;
    uint64_t expired_round_receipts = 0;
    uint64_t invalid_context_receipts = 0;
    uint64_t canonical_receipts = 0;
    uint64_t capacity_rejections = 0;
    uint64_t template_selected_receipts = 0;
  };

  // Local policy cache for already signed, self-contained records. It is not
  // consensus state: complete blocks are always parsed and validated again.
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
        semantic_state_v2 &canonical_state,
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

  private:
    void prune_canonical_enrollment(const semantic_state_v2 &canonical_state);
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

  // Validates one-record canonical envelopes against the current canonical
  // semantic state and commits the entire local relay batch atomically. Before
  // the configured hardening height only exact complete-byte duplicates are
  // idempotent. At and after the boundary, service receipts are additionally
  // deduplicated by their consensus slot so randomized re-signatures cannot
  // exhaust the bounded queue. Malformed or unauthenticated records cannot
  // enter the pool. `accepted` is the forwardable output: it also includes
  // bounded receipt retries and received enrollment prerequisites when another
  // member of their group is newly queued. All-duplicate enrollment is silent.
  relay_ingress_status_v2 admit_relay_envelopes_v2(
      const std::vector<std::string> &envelopes,
      uint64_t inclusion_height,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts,
      relay_record_pool_v2 &pool,
      std::vector<std::string> &accepted,
      relay_ingress_trace_v2 *trace = nullptr);
} // namespace epose
} // namespace qwertycoin
