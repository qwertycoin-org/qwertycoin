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

  bool compiled_consensus_parameters_v2(
      cryptonote::network_type,
      const crypto::hash &,
      consensus_parameters_v2 &parameters)
  {
    // PARAMETER_MANIFEST_V2.json is intentionally a non-activatable
    // reservation. Keeping this factory empty makes a public HF17 daemon fail
    // before genesis rather than silently selecting fixture or legacy values.
    parameters = {};
    return false;
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

  coordinator_status_v2 consensus_coordinator_v2::plan_reward(
      uint64_t height,
      const crypto::hash &parent_hash,
      uint64_t scheduled_subsidy,
      uint64_t transaction_fees,
      const canonical_context_source_v2 &contexts,
      coordinator_reward_plan_v2 &plan) const
  {
    plan = {};
    if (!valid_)
      return coordinator_status_v2::invalid_configuration;

    uint64_t first_payout_height = 0;
    if (!parameters_.timing.first_payout_height(first_payout_height))
      return coordinator_status_v2::arithmetic_overflow;
    if (height < first_payout_height)
    {
      plan.allocation.scheduled_subsidy = scheduled_subsidy;
      plan.allocation.transaction_fees = transaction_fees;
      plan.allocation.miner_subsidy = scheduled_subsidy;
      plan.allocation.miner_fees = transaction_fees;
      plan.allocation.issued_subsidy = scheduled_subsidy;
      plan.allocation.emission_advance = scheduled_subsidy;
      if (scheduled_subsidy
          > std::numeric_limits<uint64_t>::max() - transaction_fees)
        return coordinator_status_v2::arithmetic_overflow;
      plan.allocation.coinbase_total = scheduled_subsidy + transaction_fees;
      return coordinator_status_v2::accepted;
    }

    if (height < parameters_.timing.activation_height || parent_hash == crypto::null_hash)
      return coordinator_status_v2::invalid_block;
    const uint64_t payout_epoch =
        (height - parameters_.timing.activation_height)
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
              payout_epoch, height, payee) != reward_status_v2::accepted)
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

      plan.has_service_payee = true;
      plan.expectation.nettype = parameters_.nettype;
      plan.expectation.genesis_hash = parameters_.genesis_hash;
      plan.expectation.parameter_set_hash = parameters_.parameter_set_hash;
      plan.expectation.height = height;
      plan.expectation.parent_hash = parent_hash;
      plan.expectation.payout_epoch = payout_epoch;
      plan.expectation.qualification_hash = qualification->qualification_hash;
      plan.expectation.payee_service_public_key = payee;
      plan.expectation.reward_address = descriptor->reward_address;
    }

    const reward_status_v2 reward_status = calculate_reward_allocation_v2(
        scheduled_subsidy, transaction_fees, plan.has_service_payee,
        parameters_.empty_policy, plan.allocation,
        parameters_.service_reward_bps);
    if (reward_status == reward_status_v2::unresolved_empty_policy)
      return coordinator_status_v2::unresolved_reward_policy;
    if (reward_status != reward_status_v2::accepted)
      return coordinator_status_v2::arithmetic_overflow;
    plan.expectation.service_reward = plan.allocation.service_reward;
    return coordinator_status_v2::accepted;
  }

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

    coordinator_reward_plan_v2 plan{};
    const coordinator_status_v2 plan_status = plan_reward(
        block.height, block.parent_hash, block.scheduled_subsidy,
        block.transaction_fees, contexts, plan);
    if (plan_status != coordinator_status_v2::accepted)
      return plan_status;
    const reward_allocation_v2 &allocation = plan.allocation;
    const bool has_payee = plan.has_service_payee;
    const service_payment_expectation_v2 &expected = plan.expectation;

    uint64_t actual_coinbase_total = 0;
    if (!sum_coinbase_outputs(*block.coinbase, actual_coinbase_total))
      return coordinator_status_v2::arithmetic_overflow;
    if (actual_coinbase_total != allocation.coinbase_total)
      return coordinator_status_v2::invalid_coinbase_amount;

    validated_service_payment_v2 validated_payment{};
    verification_counters_v2 payment_verifications{};
    if (has_payee)
    {
      if (allocation.service_reward == 0)
        return coordinator_status_v2::invalid_payout_state;
      if (validate_coinbase_service_payment_v2(
              *block.coinbase, block.major_version,
              parameters_.limits.max_envelopes_per_transaction,
              parameters_.limits.envelope, expected, validated_payment,
              &payment_verifications)
          != reward_status_v2::accepted)
        return coordinator_status_v2::invalid_payment_proof;
    }

    std::vector<block_transaction_context_v2> transactions;
    transactions.reserve(block.transactions.size() + 1);
    transactions.push_back({
        block.coinbase, true, nullptr,
        has_payee ? &validated_payment : nullptr});
    for (const cryptonote::transaction *transaction : block.transactions)
      transactions.push_back({transaction, false, nullptr, nullptr});
    block_apply_summary_v2 transition_summary{};
    if (transition_.apply_block(
            block.major_version, block.height, block.block_hash,
            transactions, contexts, transition_summary)
        != block_transition_status_v2::accepted)
      return coordinator_status_v2::transition_failed;
    if (has_payee)
    {
      if (payment_verifications.randomx != 0
          || payment_verifications.signatures != 1)
      {
        transition_.disconnect_tip(block.height);
        return coordinator_status_v2::transition_failed;
      }
      if (transition_summary.semantic.verifications.signatures
              > std::numeric_limits<size_t>::max()
                  - payment_verifications.signatures)
      {
        transition_.disconnect_tip(block.height);
        return coordinator_status_v2::arithmetic_overflow;
      }
      transition_summary.semantic.verifications.signatures +=
          payment_verifications.signatures;
      if (transition_summary.semantic.verifications.signatures
          > transition_summary.charged.signature_verifications)
      {
        transition_.disconnect_tip(block.height);
        return coordinator_status_v2::transition_failed;
      }
    }

    result.reward = allocation;
    result.has_service_payee = has_payee;
    result.payment = has_payee
        ? validated_payment.context()
        : service_payment_context_v2{};
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
