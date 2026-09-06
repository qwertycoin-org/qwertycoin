// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/block_transition_v2.h"

#include <limits>

namespace
{
  bool add_size(size_t left, size_t right, size_t &out)
  {
    if (left > std::numeric_limits<size_t>::max() - right)
      return false;
    out = left + right;
    return true;
  }

  bool add_semantic_summary(
      qwertycoin::epose::semantic_apply_summary_v2 &total,
      const qwertycoin::epose::semantic_apply_summary_v2 &next)
  {
    return add_size(total.lifecycle_records, next.lifecycle_records, total.lifecycle_records)
        && add_size(total.admission_records, next.admission_records, total.admission_records)
        && add_size(total.receipt_records, next.receipt_records, total.receipt_records)
        && add_size(total.payment_proof_records, next.payment_proof_records, total.payment_proof_records)
        && add_size(total.verifications.signatures,
            next.verifications.signatures,
            total.verifications.signatures)
        && add_size(total.verifications.randomx,
            next.verifications.randomx,
            total.verifications.randomx);
  }
}

namespace qwertycoin
{
namespace epose
{
  bool block_transition_limits_v2::valid() const
  {
    return max_envelopes_per_transaction > 0 && max_recent_undo_blocks > 0
        && envelope.valid() && block.valid();
  }

  block_transition_v2::block_transition_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const epoch_timing_v2 &timing,
      const admission_policy_v2 &admission_policy,
      const committee_policy_v2 &committee_policy,
      const block_transition_limits_v2 &limits)
    : timing_(timing),
      limits_(limits),
      state_(nettype, genesis_hash, parameter_set_hash, timing, admission_policy, committee_policy),
      valid_(timing.valid() && limits.valid() && state_.valid())
  {
  }

  bool block_transition_v2::valid() const { return valid_; }

  block_transition_status_v2 block_transition_v2::apply_block(
      uint8_t major_version,
      uint64_t height,
      const crypto::hash &block_hash,
      const std::vector<block_transaction_context_v2> &transactions,
      const canonical_context_source_v2 &contexts,
      block_apply_summary_v2 &summary)
  {
    summary = {};
    if (!valid_)
      return block_transition_status_v2::invalid_configuration;
    if (major_version != HF_VERSION_QWC_EPOSE)
      return block_transition_status_v2::inactive_protocol;
    if ((!have_tip_ && height != timing_.activation_height)
        || (have_tip_ && (tip_height_ == std::numeric_limits<uint64_t>::max()
            || height != tip_height_ + 1)))
      return block_transition_status_v2::nonsequential_height;
    if (block_hash == crypto::null_hash || transactions.empty()
        || !transactions.front().coinbase)
      return block_transition_status_v2::invalid_block_context;
    for (size_t index = 0; index < transactions.size(); ++index)
    {
      if (transactions[index].transaction == nullptr
          || (index != 0 && transactions[index].coinbase))
        return block_transition_status_v2::invalid_block_context;
    }

    struct parsed_transaction
    {
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 budget{};
    };
    std::vector<parsed_transaction> parsed(transactions.size());
    envelope_budget_v2 block_budget{};

    // Complete the cheap structural and cumulative-cost pass first. No
    // semantic decoder, signature verifier, or RandomX function runs here.
    for (size_t index = 0; index < transactions.size(); ++index)
    {
      const envelope_status_v2 parsed_status = parse_transaction_extra_v2(
          transactions[index].transaction->extra,
          major_version,
          limits_.max_envelopes_per_transaction,
          limits_.envelope,
          parsed[index].records,
          parsed[index].budget);
      if (parsed_status != envelope_status_v2::accepted)
        return block_transition_status_v2::invalid_envelope;
      if (charge_block_budget_v2(parsed[index].budget, limits_.block, block_budget)
          != envelope_status_v2::accepted)
        return block_transition_status_v2::resource_limit_exceeded;
    }

    semantic_state_v2 next = state_;

    // Freeze before processing the anchor block: records through cutoff
    // height anchor-1 are eligible, records in the anchor block are not.
    if (height <= std::numeric_limits<uint64_t>::max() - timing_.anchor_depth)
    {
      const uint64_t shifted = height + timing_.anchor_depth;
      if (shifted % timing_.epoch_length == 0)
      {
        const uint64_t epoch = shifted / timing_.epoch_length;
        uint64_t first_service_epoch = 0;
        if (!timing_.first_service_epoch(first_service_epoch))
          return block_transition_status_v2::arithmetic_overflow;
        if (epoch >= first_service_epoch)
        {
          const pipeline_status_v2 status = next.freeze_membership(epoch, height, block_hash);
          if (status != pipeline_status_v2::accepted)
            return block_transition_status_v2::freeze_failed;
        }
      }
    }

    block_apply_summary_v2 next_summary{};
    next_summary.transactions = transactions.size();
    next_summary.charged = block_budget;
    for (size_t index = 0; index < transactions.size(); ++index)
    {
      semantic_apply_summary_v2 transaction_summary{};
      const semantic_transaction_context_v2 transaction_context{
          height, transactions[index].coinbase, transactions[index].payment};
      if (next.apply_transaction(
              parsed[index].records, transaction_context, contexts,
              transaction_summary) != semantic_status_v2::accepted)
        return block_transition_status_v2::invalid_semantics;
      if (!add_semantic_summary(next_summary.semantic, transaction_summary))
        return block_transition_status_v2::arithmetic_overflow;
    }

    if (next_summary.semantic.verifications.signatures
            > next_summary.charged.signature_verifications
        || next_summary.semantic.verifications.randomx
            > next_summary.charged.admission_verifications)
      return block_transition_status_v2::verification_budget_mismatch;

    // Close after processing the deadline block so receipts included at the
    // inclusive deadline participate in the settled qualification set.
    if (height <= std::numeric_limits<uint64_t>::max() - timing_.anchor_depth - 1)
    {
      const uint64_t shifted = height + timing_.anchor_depth + 1;
      if (shifted % timing_.epoch_length == 0)
      {
        const uint64_t next_epoch = shifted / timing_.epoch_length;
        if (next_epoch > 0)
        {
          const uint64_t epoch = next_epoch - 1;
          uint64_t first_service_epoch = 0;
          if (!timing_.first_service_epoch(first_service_epoch))
            return block_transition_status_v2::arithmetic_overflow;
          if (epoch >= first_service_epoch)
          {
            const pipeline_status_v2 status = next.close_qualification(epoch, height);
            if (status != pipeline_status_v2::accepted)
              return block_transition_status_v2::qualification_close_failed;
          }
        }
      }
    }

    undo_.push_back({height, state_, have_tip_, tip_height_});
    while (undo_.size() > limits_.max_recent_undo_blocks)
      undo_.pop_front();
    state_ = std::move(next);
    have_tip_ = true;
    tip_height_ = height;
    summary = next_summary;
    return block_transition_status_v2::accepted;
  }

  const semantic_state_v2 &block_transition_v2::state() const { return state_; }

  block_transition_status_v2 block_transition_v2::disconnect_tip(uint64_t height)
  {
    if (!valid_)
      return block_transition_status_v2::invalid_configuration;
    if (!have_tip_ || height != tip_height_)
      return block_transition_status_v2::nonsequential_height;
    if (undo_.empty() || undo_.back().height != height)
      return block_transition_status_v2::deep_replay_required;
    undo_entry prior = std::move(undo_.back());
    undo_.pop_back();
    state_ = std::move(prior.prior_state);
    have_tip_ = prior.prior_have_tip;
    tip_height_ = prior.prior_tip_height;
    return block_transition_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
