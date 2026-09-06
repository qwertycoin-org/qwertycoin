// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/semantic_batch_v2.h"

#include <limits>
#include <string>
#include <utility>

#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"

namespace
{
  crypto::hash combined_state_hash(
      const crypto::hash &lifecycle, const crypto::hash &membership)
  {
    std::string bytes("QWC_EPOSE_SEMANTIC_STATE_V2");
    bytes.append(reinterpret_cast<const char *>(&lifecycle), sizeof(lifecycle));
    bytes.append(reinterpret_cast<const char *>(&membership), sizeof(membership));
    return crypto::cn_fast_hash(bytes.data(), bytes.size());
  }
}

namespace qwertycoin
{
namespace epose
{
  semantic_state_v2::semantic_state_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const epoch_timing_v2 &timing,
      const admission_policy_v2 &admission_policy,
      const committee_policy_v2 &committee_policy)
    : nettype_(nettype),
      genesis_hash_(genesis_hash),
      parameter_set_hash_(parameter_set_hash),
      timing_(timing),
      admission_policy_(admission_policy),
      lifecycle_(nettype, genesis_hash, parameter_set_hash),
      membership_(nettype, genesis_hash, parameter_set_hash, timing, admission_policy, committee_policy),
      valid_(lifecycle_.valid() && membership_.valid())
  {
  }

  bool semantic_state_v2::valid() const { return valid_; }

  semantic_status_v2 semantic_state_v2::apply_transaction(
      const std::vector<envelope_record_v2> &records,
      const semantic_transaction_context_v2 &transaction,
      const canonical_context_source_v2 &contexts,
      semantic_apply_summary_v2 &summary)
  {
    summary = {};
    if (!valid_)
      return semantic_status_v2::invalid_configuration;
    if (transaction.inclusion_height < timing_.activation_height)
      return semantic_status_v2::before_activation;

    const uint64_t inclusion_epoch = transaction.inclusion_height / timing_.epoch_length;
    if (inclusion_epoch == std::numeric_limits<uint64_t>::max())
      return semantic_status_v2::arithmetic_overflow;
    uint64_t minimum_effective_epoch = inclusion_epoch + 1;
    uint64_t cutoff = 0;
    if (!timing_.enrollment_cutoff(minimum_effective_epoch, cutoff)
        || transaction.inclusion_height > cutoff)
    {
      if (minimum_effective_epoch == std::numeric_limits<uint64_t>::max())
        return semantic_status_v2::arithmetic_overflow;
      ++minimum_effective_epoch;
    }

    lifecycle_registry_v2 next_lifecycle = lifecycle_;
    membership_pipeline_v2 next_membership = membership_;
    semantic_apply_summary_v2 next_summary{};
    const auto reject = [&](semantic_status_v2 status) {
      // Record counters remain atomic, but attempted cryptographic work is
      // observable so block-budget tests can cover rejected records too.
      summary.verifications = next_summary.verifications;
      return status;
    };

    for (const envelope_record_v2 &record : records)
    {
      switch (static_cast<record_type_v2>(record.type))
      {
        case record_type_v2::identity_descriptor:
        case record_type_v2::descriptor_lifecycle:
        {
          lifecycle_record_v2 lifecycle{};
          if (decode_lifecycle_record_structure_v2(record, lifecycle)
              != record_codec_status_v2::accepted)
            return reject(semantic_status_v2::invalid_record);
          const lifecycle_status_v2 status = next_lifecycle.apply(
              lifecycle, inclusion_epoch, minimum_effective_epoch,
              &next_summary.verifications);
          if (status != lifecycle_status_v2::accepted
              && status != lifecycle_status_v2::idempotent_duplicate)
            return reject(semantic_status_v2::invalid_lifecycle);
          ++next_summary.lifecycle_records;
          break;
        }
        case record_type_v2::admission_lease:
        {
          admission_lease_v2 lease{};
          if (decode_admission_lease_record_structure_v2(record, lease)
              != record_codec_status_v2::accepted)
            return reject(semantic_status_v2::invalid_record);

          uint64_t first_service_epoch = 0;
          uint64_t expected_context_height = 0;
          uint64_t enrollment_cutoff = 0;
          if (!timing_.first_service_epoch(first_service_epoch)
              || lease.target_epoch < first_service_epoch
              || lease.target_epoch < admission_policy_.context_epoch_offset
              || !timing_.epoch_start(
                    lease.target_epoch - admission_policy_.context_epoch_offset,
                    expected_context_height)
              || !timing_.enrollment_cutoff(lease.target_epoch, enrollment_cutoff)
              || lease.admission_context_height != expected_context_height
              || transaction.inclusion_height > enrollment_cutoff)
            return reject(semantic_status_v2::invalid_admission);

          const identity_descriptor_v2 *descriptor = next_lifecycle.descriptor_for_epoch(
              lease.member.identity_id, lease.target_epoch);
          if (descriptor == nullptr
              || descriptor->service_public_key != lease.member.service_public_key
              || descriptor->operator_authorization_public_key
                    != lease.member.operator_authorization_public_key
              || descriptor->sequence != lease.member.sequence
              || hash_identity_descriptor_v2(
                    nettype_, genesis_hash_, parameter_set_hash_, *descriptor)
                    != lease.member.descriptor_hash
              || hash_reward_binding_v2(
                    nettype_, genesis_hash_, parameter_set_hash_, descriptor->reward_address)
                    != lease.member.reward_binding_hash
              || descriptor->endpoint_descriptor_hash != lease.member.endpoint_descriptor_hash)
            return reject(semantic_status_v2::lifecycle_binding_mismatch);

          crypto::hash context_hash{};
          if (!contexts.block_hash(expected_context_height, context_hash)
              || context_hash == crypto::null_hash)
            return reject(semantic_status_v2::context_unavailable);
          const admission_context_v2 admission_context{
              nettype_, genesis_hash_, parameter_set_hash_,
              expected_context_height, context_hash};
          const pipeline_status_v2 status = next_membership.apply_admission(
              lease, admission_context, transaction.inclusion_height,
              &next_summary.verifications);
          if (status != pipeline_status_v2::accepted
              && status != pipeline_status_v2::idempotent_duplicate)
            return reject(semantic_status_v2::invalid_admission);
          ++next_summary.admission_records;
          break;
        }
        case record_type_v2::service_receipt:
        {
          const receipt_context_v2 receipt_context{nettype_, genesis_hash_, parameter_set_hash_};
          authenticated_service_receipt_v2 receipt{};
          if (decode_service_receipt_record_structure_v2(record, receipt)
              != record_codec_status_v2::accepted)
            return reject(semantic_status_v2::invalid_record);
          crypto::hash round_anchor{};
          if (!contexts.round_anchor(
                  receipt.challenge.epoch, receipt.challenge.round, round_anchor)
              || round_anchor == crypto::null_hash)
            return reject(semantic_status_v2::context_unavailable);
          const pipeline_status_v2 status = next_membership.apply_authenticated_receipt(
              receipt, receipt_context, transaction.inclusion_height, round_anchor,
              &next_summary.verifications);
          if (status != pipeline_status_v2::accepted
              && status != pipeline_status_v2::idempotent_duplicate)
            return reject(semantic_status_v2::invalid_receipt);
          ++next_summary.receipt_records;
          break;
        }
        case record_type_v2::service_payment_proof:
        {
          if (!transaction.coinbase || transaction.payment == nullptr)
            return reject(semantic_status_v2::payment_proof_wrong_carrier);
          if (next_summary.payment_proof_records != 0)
            return reject(semantic_status_v2::duplicate_payment_proof);
          const service_payment_context_v2 &payment = *transaction.payment;
          if (payment.nettype != nettype_ || payment.genesis_hash != genesis_hash_
              || payment.parameter_set_hash != parameter_set_hash_
              || payment.height != transaction.inclusion_height)
            return reject(semantic_status_v2::invalid_payment_proof);
          scoped_payment_proof_v2 proof{};
          if (decode_payment_proof_record_v2(record, payment, proof)
              != record_codec_status_v2::accepted)
            return reject(semantic_status_v2::invalid_payment_proof);
          ++next_summary.payment_proof_records;
          break;
        }
        default:
          return reject(semantic_status_v2::invalid_record);
      }
    }

    lifecycle_ = std::move(next_lifecycle);
    membership_ = std::move(next_membership);
    summary = next_summary;
    return semantic_status_v2::accepted;
  }

  const lifecycle_registry_v2 &semantic_state_v2::lifecycle() const { return lifecycle_; }
  const membership_pipeline_v2 &semantic_state_v2::membership() const { return membership_; }
  pipeline_status_v2 semantic_state_v2::freeze_membership(
      uint64_t epoch, uint64_t height, const crypto::hash &anchor_hash)
  {
    std::vector<frozen_member_v2> authorized_members;
    const std::vector<identity_descriptor_v2> descriptors = lifecycle_.descriptors_for_epoch(epoch);
    authorized_members.reserve(descriptors.size());
    for (const identity_descriptor_v2 &descriptor : descriptors)
    {
      frozen_member_v2 member{};
      member.service_public_key = descriptor.service_public_key;
      member.identity_id = descriptor.identity_id;
      member.operator_authorization_public_key = descriptor.operator_authorization_public_key;
      member.descriptor_hash = hash_identity_descriptor_v2(
          nettype_, genesis_hash_, parameter_set_hash_, descriptor);
      member.reward_binding_hash = hash_reward_binding_v2(
          nettype_, genesis_hash_, parameter_set_hash_, descriptor.reward_address);
      member.endpoint_descriptor_hash = descriptor.endpoint_descriptor_hash;
      member.sequence = descriptor.sequence;
      authorized_members.push_back(member);
    }
    return membership_.freeze_membership(epoch, height, anchor_hash, authorized_members);
  }
  pipeline_status_v2 semantic_state_v2::close_qualification(uint64_t epoch, uint64_t height)
  {
    return membership_.close_qualification(epoch, height);
  }
  crypto::hash semantic_state_v2::state_hash() const
  {
    return combined_state_hash(lifecycle_.state_hash(), membership_.state_hash());
  }
} // namespace epose
} // namespace qwertycoin
