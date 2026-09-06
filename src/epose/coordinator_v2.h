// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "epose/block_transition_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct consensus_parameters_v2
  {
    cryptonote::network_type nettype = cryptonote::UNDEFINED;
    crypto::hash genesis_hash{};
    crypto::hash parameter_set_hash{};
    epoch_timing_v2 timing{};
    admission_policy_v2 admission{};
    committee_policy_v2 committee{};
    block_transition_limits_v2 limits{};
    empty_qualification_policy_v2 empty_policy = empty_qualification_policy_v2::unset;
    uint64_t service_reward_bps = EPOSE_SERVICE_REWARD_BPS_V2;
    uint8_t state_commitment_schema = 0;

    bool valid() const;
  };

  struct coordinator_block_v2
  {
    uint8_t major_version = 0;
    uint64_t height = 0;
    crypto::hash block_hash{};
    crypto::hash parent_hash{};
    uint64_t scheduled_subsidy = 0;
    uint64_t transaction_fees = 0;
    const cryptonote::transaction *coinbase = nullptr;
    std::vector<const cryptonote::transaction *> transactions;
  };

  struct coordinator_result_v2
  {
    reward_allocation_v2 reward{};
    bool has_service_payee = false;
    service_payment_context_v2 payment{};
    block_apply_summary_v2 transition{};
    uint8_t state_commitment_schema = 0;
    crypto::hash state_hash{};
    crypto::hash parameter_set_hash{};
  };

  enum class coordinator_status_v2
  {
    accepted,
    invalid_configuration,
    invalid_block,
    missing_qualification,
    invalid_payout_state,
    unresolved_reward_policy,
    invalid_coinbase_amount,
    invalid_payment_proof,
    transition_failed,
    arithmetic_overflow
  };

  // Owns the canonical v2 state transition and derives Coinbase expectations
  // exclusively from the already-settled pre-block state. It has no legacy-v1
  // fallback and cannot be constructed from an incomplete launch manifest.
  class consensus_coordinator_v2
  {
  public:
    explicit consensus_coordinator_v2(const consensus_parameters_v2 &parameters);

    bool valid() const;
    coordinator_status_v2 connect_block(
        const coordinator_block_v2 &block,
        const canonical_context_source_v2 &contexts,
        coordinator_result_v2 &result);
    block_transition_status_v2 disconnect_tip(uint64_t height);
    const semantic_state_v2 &state() const;

  private:
    consensus_parameters_v2 parameters_{};
    block_transition_v2 transition_;
    bool valid_ = false;
  };
} // namespace epose
} // namespace qwertycoin
