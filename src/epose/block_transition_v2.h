// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "cryptonote_basic/cryptonote_basic.h"
#include "epose/envelope_v2.h"
#include "epose/semantic_batch_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct block_transition_limits_v2
  {
    size_t max_envelopes_per_transaction = 0;
    envelope_limits_v2 envelope{};
    block_budget_limits_v2 block{};

    bool valid() const;
  };

  struct block_transaction_context_v2
  {
    const cryptonote::transaction *transaction = nullptr;
    bool coinbase = false;
    const service_payment_context_v2 *payment = nullptr;
  };

  struct block_apply_summary_v2
  {
    size_t transactions = 0;
    semantic_apply_summary_v2 semantic{};
    envelope_budget_v2 charged{};
  };

  enum class block_transition_status_v2
  {
    accepted,
    invalid_configuration,
    inactive_protocol,
    invalid_block_context,
    invalid_envelope,
    resource_limit_exceeded,
    invalid_semantics,
    freeze_failed,
    qualification_close_failed,
    verification_budget_mismatch,
    nonsequential_height,
    arithmetic_overflow
  };

  // Canonical, fail-atomic block adapter. It performs a structural/resource
  // pass over every transaction before any RandomX or signature verification,
  // then applies records in miner/transaction wire order to a copied semantic
  // state. The state is committed only after boundary transitions also pass.
  class block_transition_v2
  {
  public:
    block_transition_v2(
        cryptonote::network_type nettype,
        const crypto::hash &genesis_hash,
        const crypto::hash &parameter_set_hash,
        const epoch_timing_v2 &timing,
        const admission_policy_v2 &admission_policy,
        const committee_policy_v2 &committee_policy,
        const block_transition_limits_v2 &limits);

    bool valid() const;
    block_transition_status_v2 apply_block(
        uint8_t major_version,
        uint64_t height,
        const crypto::hash &block_hash,
        const std::vector<block_transaction_context_v2> &transactions,
        const canonical_context_source_v2 &contexts,
        block_apply_summary_v2 &summary);

    const semantic_state_v2 &state() const;

  private:
    epoch_timing_v2 timing_{};
    block_transition_limits_v2 limits_{};
    semantic_state_v2 state_;
    bool valid_ = false;
    bool have_tip_ = false;
    uint64_t tip_height_ = 0;
  };
} // namespace epose
} // namespace qwertycoin
