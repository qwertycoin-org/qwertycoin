// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/semantic_batch_v2.h"

#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"

namespace
{
  bool read_u64_le_at(const std::string &input, size_t offset, uint64_t &value)
  {
    if (offset > input.size() || input.size() - offset < 8)
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8)
      value |= static_cast<uint64_t>(static_cast<uint8_t>(input[offset++])) << shift;
    return true;
  }

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

    for (const envelope_record_v2 &record : records)
    {
      switch (static_cast<record_type_v2>(record.type))
      {
        case record_type_v2::identity_descriptor:
        case record_type_v2::descriptor_lifecycle:
        {
          lifecycle_record_v2 lifecycle{};
          if (decode_lifecycle_record_v2(
                  record, nettype_, genesis_hash_, parameter_set_hash_, lifecycle)
              != record_codec_status_v2::accepted)
            return semantic_status_v2::invalid_record;
          const lifecycle_status_v2 status = next_lifecycle.apply(
              lifecycle, inclusion_epoch, minimum_effective_epoch);
          if (status != lifecycle_status_v2::accepted
              && status != lifecycle_status_v2::idempotent_duplicate)
            return semantic_status_v2::invalid_lifecycle;
          ++next_summary.lifecycle_records;
          break;
        }
        case record_type_v2::admission_lease:
        {
          if (record.version != EPOSE_ADMISSION_LEASE_RECORD_VERSION_V2
              || record.payload.size() != EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2)
            return semantic_status_v2::invalid_record;
          constexpr size_t context_height_offset =
              sizeof(crypto::public_key) + sizeof(crypto::hash)
              + sizeof(crypto::public_key) + 3 * sizeof(crypto::hash)
              + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint8_t);
          uint64_t context_height = 0;
          if (!read_u64_le_at(record.payload, context_height_offset, context_height))
            return semantic_status_v2::invalid_record;
          crypto::hash context_hash{};
          if (!contexts.block_hash(context_height, context_hash) || context_hash == crypto::null_hash)
            return semantic_status_v2::context_unavailable;
          const admission_context_v2 admission_context{
              nettype_, genesis_hash_, parameter_set_hash_, context_height, context_hash};
          admission_lease_v2 lease{};
          if (decode_admission_lease_record_v2(
                  record, admission_context, admission_policy_, lease)
              != record_codec_status_v2::accepted)
            return semantic_status_v2::invalid_record;
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
            return semantic_status_v2::lifecycle_binding_mismatch;
          const pipeline_status_v2 status = next_membership.apply_admission(
              lease, admission_context, transaction.inclusion_height);
          if (status != pipeline_status_v2::accepted
              && status != pipeline_status_v2::idempotent_duplicate)
            return semantic_status_v2::invalid_admission;
          ++next_summary.admission_records;
          break;
        }
        case record_type_v2::service_receipt:
        {
          const receipt_context_v2 receipt_context{nettype_, genesis_hash_, parameter_set_hash_};
          authenticated_service_receipt_v2 receipt{};
          if (decode_service_receipt_record_v2(record, receipt_context, receipt)
              != record_codec_status_v2::accepted)
            return semantic_status_v2::invalid_record;
          crypto::hash round_anchor{};
          if (!contexts.round_anchor(
                  receipt.challenge.epoch, receipt.challenge.round, round_anchor)
              || round_anchor == crypto::null_hash)
            return semantic_status_v2::context_unavailable;
          const pipeline_status_v2 status = next_membership.apply_authenticated_receipt(
              receipt, receipt_context, transaction.inclusion_height, round_anchor);
          if (status != pipeline_status_v2::accepted
              && status != pipeline_status_v2::idempotent_duplicate)
            return semantic_status_v2::invalid_receipt;
          ++next_summary.receipt_records;
          break;
        }
        case record_type_v2::service_payment_proof:
        {
          if (!transaction.coinbase || transaction.payment == nullptr)
            return semantic_status_v2::payment_proof_wrong_carrier;
          if (next_summary.payment_proof_records != 0)
            return semantic_status_v2::duplicate_payment_proof;
          const service_payment_context_v2 &payment = *transaction.payment;
          if (payment.nettype != nettype_ || payment.genesis_hash != genesis_hash_
              || payment.parameter_set_hash != parameter_set_hash_
              || payment.height != transaction.inclusion_height)
            return semantic_status_v2::invalid_payment_proof;
          scoped_payment_proof_v2 proof{};
          if (decode_payment_proof_record_v2(record, payment, proof)
              != record_codec_status_v2::accepted)
            return semantic_status_v2::invalid_payment_proof;
          ++next_summary.payment_proof_records;
          break;
        }
        default:
          return semantic_status_v2::invalid_record;
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
    return membership_.freeze_membership(epoch, height, anchor_hash);
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
