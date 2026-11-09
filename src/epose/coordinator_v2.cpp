// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/coordinator_v2.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/compiled_profile_v2.h"
#include "epose/service_receipt_v2.h"
#include "string_tools.h"

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
        && state_commitment_schema == 1
        && retained_epochs == QWC_EPOSE_RETAINED_EPOCHS;
  }

  bool compiled_consensus_parameters_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      consensus_parameters_v2 &parameters)
  {
    parameters = {};
    const char *genesis_hex = nullptr;
    const char *parameter_set_hex = nullptr;
    crypto::hash expected_genesis{};
    crypto::hash parameter_set_hash{};
    if (nettype == cryptonote::FAKECHAIN)
    {
      expected_genesis = genesis_hash;
      parameter_set_hex = FAKECHAIN_PARAMETER_SET_HASH_V2;
    }
    else if (!compiled_profile_hashes_v2(nettype, genesis_hex, parameter_set_hex)
        || !epee::string_tools::hex_to_pod(genesis_hex, expected_genesis)
        || genesis_hash != expected_genesis)
      return false;
    if (expected_genesis == crypto::null_hash
        || !epee::string_tools::hex_to_pod(parameter_set_hex, parameter_set_hash))
      return false;

    parameters.nettype = nettype;
    parameters.genesis_hash = expected_genesis;
    parameters.parameter_set_hash = parameter_set_hash;
    if (nettype == cryptonote::FAKECHAIN)
    {
      parameters.timing = {0, 120, 10};
      parameters.admission = {admission_work_algorithm_v2::randomx, 4, 1};
      parameters.committee.round_offsets = {0, 40, 80};
    }
    else if (nettype == cryptonote::DEVNET)
    {
      parameters.timing = {0, 60, 5};
      parameters.admission = {admission_work_algorithm_v2::randomx, 4, 1};
      parameters.committee.round_offsets = {0, 20, 40};
    }
    else
    {
      parameters.timing = {0, 720, 60};
      parameters.admission = {admission_work_algorithm_v2::randomx, 18, 1};
      parameters.committee.round_offsets = {0, 200, 400};
    }
    parameters.committee.committee_size = 9;
    parameters.committee.threshold = 6;
    parameters.committee.round_count = 3;
    parameters.committee.rounds_required = 2;
    parameters.committee.min_compatible_committee = QWC_EPOSE_MIN_COMPATIBLE_COMMITTEE;
    parameters.committee.service_kind =
        static_cast<uint8_t>(service_kind_v2::canonical_object);
    parameters.committee.max_active_population = 100;

    parameters.limits.max_envelopes_per_transaction = 4;
    parameters.limits.envelope.max_envelope_bytes = 65536;
    parameters.limits.envelope.max_records = 256;
    parameters.limits.envelope.max_record_payload_bytes = 65536;
    parameters.limits.envelope.max_signature_verifications = 2048;
    parameters.limits.envelope.max_admission_verifications = 8;
    parameters.limits.envelope.supported_record_versions = {0, 2, 2, 1, 2, 1};
    parameters.limits.block = {262144, 1024, 2048, 8};
    parameters.limits.max_recent_undo_blocks = 2160;
    parameters.empty_policy = empty_qualification_policy_v2::miner_fallback;
    parameters.service_reward_bps = EPOSE_SERVICE_REWARD_BPS_V2;
    parameters.state_commitment_schema = 1;
    parameters.retained_epochs = QWC_EPOSE_RETAINED_EPOCHS;
    if (!epee::string_tools::hex_to_pod(
            QWC_CONTRACTS_PARAMETER_SET_HASH,
            parameters.window_parameter_set_hash))
      return false;
    return parameters.valid();
  }

  consensus_coordinator_v2::consensus_coordinator_v2(
      const consensus_parameters_v2 &parameters)
    : parameters_(parameters),
      transition_(
          parameters.nettype, parameters.genesis_hash,
          parameters.parameter_set_hash, parameters.timing,
          parameters.admission, parameters.committee, parameters.limits,
          parameters.retained_epochs,
          parameters.window_parameter_set_hash),
      valid_(parameters.valid() && transition_.valid())
  {
  }

  bool consensus_coordinator_v2::valid() const { return valid_; }

  coordinator_status_v2 consensus_coordinator_v2::plan_reward(
      uint8_t major_version,
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
    if (transition_.state().membership().read_epoch(source_epoch)
        != pipeline_status_v2::accepted)
      return coordinator_status_v2::outside_retained_window;
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
    if (!service_payout_due_v2(
            major_version, plan.has_service_payee, plan.allocation.service_reward))
    {
      plan.has_service_payee = false;
      plan.expectation = {};
      return coordinator_status_v2::accepted;
    }
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
    if (!is_qwc_epose_v2_hardfork(block.major_version)
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
        block.major_version, block.height, block.parent_hash, block.scheduled_subsidy,
        block.transaction_fees, contexts, plan);
    if (plan_status != coordinator_status_v2::accepted)
      return plan_status;
    const reward_allocation_v2 &allocation = plan.allocation;
    const bool has_payee = plan.has_service_payee;
    const service_payment_expectation_v2 &expected = plan.expectation;

    uint64_t actual_coinbase_total = 0;
    if (!sum_coinbase_outputs(*block.coinbase, actual_coinbase_total))
      return coordinator_status_v2::arithmetic_overflow;
    if (actual_coinbase_total < block.contract_mints
        || actual_coinbase_total - block.contract_mints != allocation.coinbase_total) // H-30
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
    result.state_commitment_schema = transition_.state().state_commitment_schema();
    result.state_hash = transition_.state().state_hash();
    if (result.state_hash == crypto::null_hash)
    {
      transition_.disconnect_tip(block.height);
      return coordinator_status_v2::transition_failed;
    }
    result.parameter_set_hash =
        result.state_commitment_schema == QWC_EPOSE_ROLLING_STATE_SCHEMA
            ? transition_.state().window_parameter_hash()
            : parameters_.parameter_set_hash;
    ++generation_;
    return coordinator_status_v2::accepted;
  }

  block_transition_status_v2 consensus_coordinator_v2::disconnect_tip(uint64_t height)
  {
    ++generation_;
    return transition_.disconnect_tip(height);
  }

  const semantic_state_v2 &consensus_coordinator_v2::state() const
  {
    return transition_.state();
  }

  semantic_state_v2 &consensus_coordinator_v2::preview_state()
  {
    ++generation_;
    return transition_.preview_state();
  }

  state_storage_diagnostics_v2 consensus_coordinator_v2::state_diagnostics() const
  {
    return transition_.state_diagnostics();
  }

  bool consensus_coordinator_v2::tip_height(uint64_t &height) const
  {
    return transition_.tip_height(height);
  }

  std::string consensus_coordinator_v2::serialize_state() const
  {
    return transition_.serialize_state();
  }

  bool consensus_coordinator_v2::restore_state(uint64_t height, const std::string &image)
  {
    ++generation_;
    return valid_ && transition_.restore_state(height, image);
  }

  uint64_t consensus_coordinator_v2::generation() const noexcept
  {
    return generation_;
  }

  crypto::hash consensus_coordinator_v2::state_hash() const
  {
    ++state_hash_calls_;
    return transition_.state().state_hash();
  }
} // namespace epose
} // namespace qwertycoin
