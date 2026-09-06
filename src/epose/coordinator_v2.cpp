// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/coordinator_v2.h"

#include "cryptonote_basic/cryptonote_format_utils.h"

#include <algorithm>
#include <limits>

namespace
{
  bool sum_coinbase_outputs(
      const cryptonote::transaction &coinbase,
      uint64_t &sum)
  {
    sum = 0;
    for (const cryptonote::tx_out &output : coinbase.vout)
    {
      if (sum > std::numeric_limits<uint64_t>::max() - output.amount)
        return false;
      sum += output.amount;
    }
    return true;
  }
}

namespace qwertycoin
{
namespace epose
{
  bool consensus_parameters_v2::valid() const
  {
    return nettype != cryptonote::UNDEFINED
        && genesis_hash != crypto::null_hash
        && parameter_set_hash != crypto::null_hash
        && timing.valid() && timing.activation_height == 0
        && admission.valid() && committee.valid() && limits.valid()
        && (empty_policy == empty_qualification_policy_v2::miner_fallback
            || empty_policy == empty_qualification_policy_v2::permanent_nonissuance)
        && service_reward_bps == EPOSE_SERVICE_REWARD_BPS_V2
        && state_commitment_schema == 1;
  }

  consensus_coordinator_v2::consensus_coordinator_v2(
      const consensus_parameters_v2 &parameters)
    : parameters_(parameters),
      transition_(
          parameters.nettype, parameters.genesis_hash,
          parameters.parameter_set_hash, parameters.timing,
          parameters.admission, parameters.committee, parameters.limits),
      valid_(parameters.valid() && transition_.valid())
  {
  }

  bool consensus_coordinator_v2::valid() const { return valid_; }

  coordinator_status_v2 consensus_coordinator_v2::connect_block(
      const coordinator_block_v2 &block,
      const canonical_context_source_v2 &contexts,
      coordinator_result_v2 &result)
  {
    result = {};
    if (!valid_)
      return coordinator_status_v2::invalid_configuration;
    if (block.major_version != HF_VERSION_QWC_EPOSE
        || block.block_hash == crypto::null_hash
        || (block.height != parameters_.timing.activation_height
            && block.parent_hash == crypto::null_hash)
        || block.coinbase == nullptr
        || !cryptonote::is_coinbase(*block.coinbase))
      return coordinator_status_v2::invalid_block;
    for (const cryptonote::transaction *transaction : block.transactions)
    {
      if (transaction == nullptr || cryptonote::is_coinbase(*transaction))
        return coordinator_status_v2::invalid_block;
    }

    reward_allocation_v2 allocation{};
    bool has_payee = false;
    service_payment_expectation_v2 expected{};
    uint64_t first_payout_height = 0;
    if (!parameters_.timing.first_payout_height(first_payout_height))
      return coordinator_status_v2::arithmetic_overflow;
    if (block.height < first_payout_height)
    {
      allocation.scheduled_subsidy = block.scheduled_subsidy;
      allocation.transaction_fees = block.transaction_fees;
      allocation.miner_subsidy = block.scheduled_subsidy;
      allocation.miner_fees = block.transaction_fees;
      allocation.issued_subsidy = block.scheduled_subsidy;
      allocation.emission_advance = block.scheduled_subsidy;
      if (block.scheduled_subsidy
          > std::numeric_limits<uint64_t>::max() - block.transaction_fees)
        return coordinator_status_v2::arithmetic_overflow;
      allocation.coinbase_total = block.scheduled_subsidy + block.transaction_fees;
    }
    else
    {
      uint64_t payout_epoch = 0;
      if (block.height < parameters_.timing.activation_height)
        return coordinator_status_v2::invalid_block;
      payout_epoch = (block.height - parameters_.timing.activation_height)
          / parameters_.timing.epoch_length;
      if (payout_epoch == 0)
        return coordinator_status_v2::invalid_payout_state;
      const uint64_t source_epoch = payout_epoch - 1;
      const qualification_set_v2 *qualification =
          transition_.state().membership().qualification(source_epoch);
      if (qualification == nullptr)
        return coordinator_status_v2::missing_qualification;

      if (!qualification->qualified_nodes.empty())
      {
        uint64_t payout_seed_height = 0;
        crypto::hash payout_seed{};
        if (!parameters_.timing.committee_anchor(
                payout_epoch, payout_seed_height)
            || !contexts.block_hash(payout_seed_height, payout_seed)
            || payout_seed == crypto::null_hash)
          return coordinator_status_v2::invalid_payout_state;
        crypto::public_key payee{};
        if (select_service_payee_v2(
                *qualification, payout_seed, parameters_.genesis_hash,
                parameters_.parameter_set_hash, parameters_.timing,
                payout_epoch, block.height, payee) != reward_status_v2::accepted)
          return coordinator_status_v2::invalid_payout_state;

        const membership_snapshot_v2 *snapshot =
            transition_.state().membership().snapshot(source_epoch);
        if (snapshot == nullptr)
          return coordinator_status_v2::invalid_payout_state;
        const auto member = std::find_if(
            snapshot->members.begin(), snapshot->members.end(),
            [&payee](const frozen_member_v2 &candidate) {
              return candidate.service_public_key == payee;
            });
        if (member == snapshot->members.end())
          return coordinator_status_v2::invalid_payout_state;
        const identity_descriptor_v2 *descriptor =
            transition_.state().lifecycle().descriptor_for_epoch(
                member->identity_id, source_epoch);
        if (descriptor == nullptr
            || descriptor->service_public_key != payee
            || descriptor->sequence != member->sequence
            || hash_identity_descriptor_v2(
                   parameters_.nettype, parameters_.genesis_hash,
                   parameters_.parameter_set_hash, *descriptor)
                != member->descriptor_hash
            || hash_reward_binding_v2(
                   parameters_.nettype, parameters_.genesis_hash,
                   parameters_.parameter_set_hash, descriptor->reward_address)
                != member->reward_binding_hash)
          return coordinator_status_v2::invalid_payout_state;

        has_payee = true;
        expected.nettype = parameters_.nettype;
        expected.genesis_hash = parameters_.genesis_hash;
        expected.parameter_set_hash = parameters_.parameter_set_hash;
        expected.height = block.height;
        expected.parent_hash = block.parent_hash;
        expected.payout_epoch = payout_epoch;
        expected.qualification_hash = qualification->qualification_hash;
        expected.payee_service_public_key = payee;
        expected.reward_address = descriptor->reward_address;
      }

      const reward_status_v2 reward_status = calculate_reward_allocation_v2(
          block.scheduled_subsidy, block.transaction_fees, has_payee,
          parameters_.empty_policy, allocation, parameters_.service_reward_bps);
      if (reward_status == reward_status_v2::unresolved_empty_policy)
        return coordinator_status_v2::unresolved_reward_policy;
      if (reward_status != reward_status_v2::accepted)
        return coordinator_status_v2::arithmetic_overflow;
      expected.service_reward = allocation.service_reward;
    }

    uint64_t actual_coinbase_total = 0;
    if (!sum_coinbase_outputs(*block.coinbase, actual_coinbase_total))
      return coordinator_status_v2::arithmetic_overflow;
    if (actual_coinbase_total != allocation.coinbase_total)
      return coordinator_status_v2::invalid_coinbase_amount;

    service_payment_context_v2 payment{};
    if (has_payee)
    {
      if (allocation.service_reward == 0)
        return coordinator_status_v2::invalid_payout_state;
      if (verify_coinbase_service_payment_v2(
              *block.coinbase, block.major_version,
              parameters_.limits.max_envelopes_per_transaction,
              parameters_.limits.envelope, expected, payment)
          != reward_status_v2::accepted)
        return coordinator_status_v2::invalid_payment_proof;
    }

    std::vector<block_transaction_context_v2> transactions;
    transactions.reserve(block.transactions.size() + 1);
    transactions.push_back({block.coinbase, true, has_payee ? &payment : nullptr});
    for (const cryptonote::transaction *transaction : block.transactions)
      transactions.push_back({transaction, false, nullptr});
    block_apply_summary_v2 transition_summary{};
    if (transition_.apply_block(
            block.major_version, block.height, block.block_hash,
            transactions, contexts, transition_summary)
        != block_transition_status_v2::accepted)
      return coordinator_status_v2::transition_failed;

    result.reward = allocation;
    result.has_service_payee = has_payee;
    result.payment = std::move(payment);
    result.transition = transition_summary;
    result.state_commitment_schema = parameters_.state_commitment_schema;
    result.state_hash = transition_.state().state_hash();
    result.parameter_set_hash = parameters_.parameter_set_hash;
    return coordinator_status_v2::accepted;
  }

  block_transition_status_v2 consensus_coordinator_v2::disconnect_tip(uint64_t height)
  {
    return transition_.disconnect_tip(height);
  }

  const semantic_state_v2 &consensus_coordinator_v2::state() const
  {
    return transition_.state();
  }
} // namespace epose
} // namespace qwertycoin
