// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/semantic_batch_v2.h"

#include <limits>
#include <stdexcept>
#include <boost/scope_exit.hpp>
#include <string>
#include <type_traits>
#include <utility>

#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"

namespace
{
  template<typename T>
  void reserve_append(std::vector<T> &target, const std::vector<T> &source)
  {
    static_assert(std::is_nothrow_copy_constructible<T>::value
        && std::is_nothrow_copy_assignable<T>::value,
        "Journal merge requires nonthrowing fixed-size entries");
    if (source.size() > target.max_size() - target.size())
      throw std::length_error("EPoSE undo journal overflow");
    target.reserve(target.size() + source.size());
  }

  // Reserve every destination before changing any logical length. The entries
  // contain only fixed-size values, so the following inserts cannot throw.
  void append_undo(
      qwertycoin::epose::semantic_state_v2::undo_journal &target,
      const qwertycoin::epose::semantic_state_v2::undo_journal &source)
  {
    reserve_append(target.lifecycle.record_sizes, source.lifecycle.record_sizes);
    reserve_append(target.membership.replaced_leases, source.membership.replaced_leases);
    reserve_append(target.membership.inserted_snapshots, source.membership.inserted_snapshots);
    reserve_append(target.membership.inserted_qualifications, source.membership.inserted_qualifications);
    target.lifecycle.record_sizes.insert(target.lifecycle.record_sizes.end(),
        source.lifecycle.record_sizes.begin(), source.lifecycle.record_sizes.end());
    target.membership.replaced_leases.insert(target.membership.replaced_leases.end(),
        source.membership.replaced_leases.begin(), source.membership.replaced_leases.end());
    target.membership.inserted_snapshots.insert(target.membership.inserted_snapshots.end(),
        source.membership.inserted_snapshots.begin(), source.membership.inserted_snapshots.end());
    target.membership.inserted_qualifications.insert(target.membership.inserted_qualifications.end(),
        source.membership.inserted_qualifications.begin(), source.membership.inserted_qualifications.end());
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

  semantic_state_v2::undo_journal semantic_state_v2::checkpoint() const
  {
    return {lifecycle_.checkpoint(), membership_.checkpoint()};
  }

  void semantic_state_v2::rollback(const undo_journal &undo) noexcept
  {
    membership_.rollback(undo.membership);
    lifecycle_.rollback(undo.lifecycle);
  }

  bool semantic_state_v2::valid() const { return valid_; }

  semantic_status_v2 semantic_state_v2::apply_transaction(
      const std::vector<envelope_record_v2> &records,
      const semantic_transaction_context_v2 &transaction,
      const canonical_context_source_v2 &contexts,
      semantic_apply_summary_v2 &summary,
      undo_journal *block_undo)
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

    // Keep all configuration/height checks above, even for an empty record list.
    if (records.empty())
      return semantic_status_v2::accepted;
    undo_journal undo = checkpoint();
    bool committed = false;
    BOOST_SCOPE_EXIT_ALL(&) { if (!committed) rollback(undo); };

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
          const lifecycle_status_v2 status = lifecycle_.apply(
              lifecycle, inclusion_epoch, minimum_effective_epoch,
              &next_summary.verifications, &undo.lifecycle);
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

          const identity_descriptor_v2 *descriptor = lifecycle_.descriptor_for_epoch(
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
          const pipeline_status_v2 status = membership_.apply_admission(
              lease, admission_context, transaction.inclusion_height,
              &next_summary.verifications, &undo.membership);
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
          const pipeline_status_v2 status = membership_.apply_authenticated_receipt(
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
          if (!transaction.coinbase
              || (transaction.payment == nullptr
                  && transaction.validated_payment == nullptr)
              || (transaction.payment != nullptr
                  && transaction.validated_payment != nullptr))
            return reject(semantic_status_v2::payment_proof_wrong_carrier);
          if (next_summary.payment_proof_records != 0)
            return reject(semantic_status_v2::duplicate_payment_proof);
          const service_payment_context_v2 &payment =
              transaction.validated_payment != nullptr
              ? transaction.validated_payment->context()
              : *transaction.payment;
          if (payment.nettype != nettype_ || payment.genesis_hash != genesis_hash_
              || payment.parameter_set_hash != parameter_set_hash_
              || payment.height != transaction.inclusion_height)
            return reject(semantic_status_v2::invalid_payment_proof);
          if (transaction.validated_payment != nullptr)
          {
            if (!transaction.validated_payment->matches(record))
              return reject(semantic_status_v2::invalid_payment_proof);
          }
          else
          {
            scoped_payment_proof_v2 proof{};
            if (decode_payment_proof_record_v2(
                    record, payment, proof, &next_summary.verifications)
                != record_codec_status_v2::accepted)
              return reject(semantic_status_v2::invalid_payment_proof);
          }
          ++next_summary.payment_proof_records;
          break;
        }
        default:
          return reject(semantic_status_v2::invalid_record);
      }
    }

    if (block_undo != nullptr)
      append_undo(*block_undo, undo);
    committed = true;
    summary = next_summary;
    return semantic_status_v2::accepted;
  }

  const lifecycle_registry_v2 &semantic_state_v2::lifecycle() const { return lifecycle_; }
  const membership_pipeline_v2 &semantic_state_v2::membership() const { return membership_; }
  cryptonote::network_type semantic_state_v2::nettype() const { return nettype_; }
  const crypto::hash &semantic_state_v2::genesis_hash() const { return genesis_hash_; }
  const crypto::hash &semantic_state_v2::parameter_set_hash() const
  {
    return parameter_set_hash_;
  }
  pipeline_status_v2 semantic_state_v2::freeze_membership(
      uint64_t epoch, uint64_t height, const crypto::hash &anchor_hash,
      undo_journal *undo)
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
    return membership_.freeze_membership(
        epoch, height, anchor_hash, authorized_members, undo ? &undo->membership : nullptr);
  }
  pipeline_status_v2 semantic_state_v2::close_qualification(
      uint64_t epoch, uint64_t height, undo_journal *undo)
  {
    return membership_.close_qualification(epoch, height, undo ? &undo->membership : nullptr);
  }
  crypto::hash semantic_state_v2::state_hash() const
  {
    return combined_state_hash(lifecycle_.state_hash(), membership_.state_hash());
  }
} // namespace epose
} // namespace qwertycoin
