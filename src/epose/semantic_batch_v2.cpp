// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/semantic_batch_v2.h"
#include "epose/state_image_v2.h"

#include <limits>
#include <iterator>

#include <stdexcept>
#include <boost/scope_exit.hpp>
#include <string>
#include <type_traits>
#include <utility>

#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"
#include "epose/hash_transcript.h"

namespace
{
  using namespace qwertycoin::epose::transcript;

  template<typename T>
  void reserve_append(std::vector<T> &target, const std::vector<T> &source)
  {
    static_assert(std::is_nothrow_move_constructible<T>::value,
        "Journal merge requires nonthrowing fixed-size entries");
    if (source.size() > target.max_size() - target.size())
      throw std::length_error("EPoSE undo journal overflow");
    target.reserve(target.size() + source.size());
  }

  template<typename Visit>
  void visit_journal_vectors(
      qwertycoin::epose::semantic_state_v2::undo_journal &target,
      qwertycoin::epose::semantic_state_v2::undo_journal &source,
      Visit visit)
  {
    visit(target.lifecycle.record_sizes, source.lifecycle.record_sizes);
    visit(target.lifecycle.registered_indices, source.lifecycle.registered_indices);
    visit(target.lifecycle.inserted_services, source.lifecycle.inserted_services);
    visit(target.lifecycle.folds, source.lifecycle.folds);
    visit(target.membership.buckets, source.membership.buckets);
    visit(target.membership.appended_leases, source.membership.appended_leases);
    visit(target.membership.appended_receipts, source.membership.appended_receipts);
    visit(target.membership.created_buckets, source.membership.created_buckets);
    visit(target.membership.replaced_leases, source.membership.replaced_leases);
    visit(target.membership.inserted_snapshots, source.membership.inserted_snapshots);
    visit(target.membership.inserted_qualifications, source.membership.inserted_qualifications);
  }

  // Reserve every destination before changing any logical length. The entries
  // contain only fixed-size values, so the following inserts cannot throw.
  void append_undo(
      qwertycoin::epose::semantic_state_v2::undo_journal &target,
      qwertycoin::epose::semantic_state_v2::undo_journal &source)
  {
    visit_journal_vectors(target, source, [](auto &destination, auto &entries) {
      reserve_append(destination, entries);
    });
    visit_journal_vectors(target, source, [](auto &destination, auto &entries) {
      destination.insert(destination.end(),
          std::make_move_iterator(entries.begin()),
          std::make_move_iterator(entries.end()));
    });
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
      const committee_policy_v2 &committee_policy,
      uint64_t retained_epochs,
      const crypto::hash &window_parameter_hash)
    : nettype_(nettype),
      genesis_hash_(genesis_hash),
      parameter_set_hash_(parameter_set_hash),
      timing_(timing),
      admission_policy_(admission_policy),
      lifecycle_(nettype, genesis_hash, parameter_set_hash),
      membership_(nettype, genesis_hash, parameter_set_hash, timing, admission_policy, committee_policy),
      window_parameter_hash_(window_parameter_hash),
      retained_epochs_(retained_epochs),
      valid_(lifecycle_.valid() && membership_.valid() && retained_epochs > 0)
  {
    if (window_parameter_hash_ == crypto::null_hash)
    {
      std::string blob("QWC_EPOSE_WINDOW_PARAMETERS_V2");
      append_bytes(blob, parameter_set_hash_);
      append_u64_le(blob, retained_epochs_);
      window_parameter_hash_ = crypto::cn_fast_hash(blob.data(), blob.size());
    }
  }

  semantic_state_v2::undo_journal semantic_state_v2::checkpoint() const
  {
    return {lifecycle_.checkpoint(), membership_.checkpoint(), history_root_,
        next_fold_epoch_, open_epoch_, window_active_};
  }

  void semantic_state_v2::rollback(const undo_journal &undo) noexcept
  {
    membership_.rollback(undo.membership);
    lifecycle_.rollback(undo.lifecycle);
    history_root_ = undo.prior_history_root;
    next_fold_epoch_ = undo.prior_next_fold_epoch;
    open_epoch_ = undo.prior_open_epoch;
    window_active_ = undo.prior_window_active;
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
          if (lifecycle.next_descriptor.version == 2 && !window_active_)
            return reject(semantic_status_v2::invalid_record);

          const lifecycle_status_v2 status = lifecycle_.apply(
              lifecycle, inclusion_epoch, minimum_effective_epoch,
              &next_summary.verifications, &undo.lifecycle);
          if (status != lifecycle_status_v2::accepted
              && status != lifecycle_status_v2::idempotent_duplicate)
          {
            summary.rejected_lifecycle = status;
            return reject(semantic_status_v2::invalid_lifecycle);
          }
          ++next_summary.lifecycle_records;
          break;
        }
        case record_type_v2::admission_lease:
        {
          admission_lease_v2 lease{};
          if (decode_admission_lease_record_structure_v2(record, lease)
              != record_codec_status_v2::accepted)
            return reject(semantic_status_v2::invalid_record);
          if (lease.member.descriptor_version == 2 && !window_active_)
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

          if (membership_.read_epoch(lease.target_epoch)
              != pipeline_status_v2::accepted)
            return reject(semantic_status_v2::outside_retained_window);
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
              || descriptor->endpoint_descriptor_hash != lease.member.endpoint_descriptor_hash
              || descriptor->version != lease.member.descriptor_version
              || descriptor->endpoint_transport != lease.member.endpoint_transport
              || descriptor->dial_capabilities != lease.member.dial_capabilities
              || descriptor->dns_address_families != lease.member.dns_address_families)
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
            return reject(status == pipeline_status_v2::outside_retained_window
                ? semantic_status_v2::outside_retained_window
                : semantic_status_v2::invalid_admission);
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
          membership_.record_receipt_size(receipt.challenge.epoch, &undo.membership);
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
            return reject(status == pipeline_status_v2::outside_retained_window
                ? semantic_status_v2::outside_retained_window
                : semantic_status_v2::invalid_receipt);
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
  membership_pipeline_v2 &semantic_state_v2::preview_membership() { return membership_; }
  cryptonote::network_type semantic_state_v2::nettype() const { return nettype_; }
  const crypto::hash &semantic_state_v2::genesis_hash() const { return genesis_hash_; }
  const crypto::hash &semantic_state_v2::parameter_set_hash() const
  {
    return parameter_set_hash_;
  }
  const crypto::hash &semantic_state_v2::window_parameter_hash() const
  {
    return window_parameter_hash_;
  }
  const crypto::hash &semantic_state_v2::history_root() const
  {
    return history_root_;
  }
  uint64_t semantic_state_v2::retained_epochs() const
  {
    return retained_epochs_;
  }
  uint8_t semantic_state_v2::state_commitment_schema() const
  {
    return window_active_ ? QWC_EPOSE_ROLLING_STATE_SCHEMA : 1;
  }
  size_t semantic_state_v2::state_bytes() const
  {
    return sizeof(*this) - sizeof(lifecycle_) - sizeof(membership_)
        + lifecycle_.state_bytes() + membership_.state_bytes();
  }
  bool semantic_state_v2::advance_window(uint64_t open_epoch, undo_journal *undo)
  {
    if (undo == nullptr || !valid_ || (window_active_ && open_epoch < open_epoch_))
      return false;
    lifecycle_.activate_window(retained_epochs_, &undo->lifecycle);
    membership_.activate_transport_window();
    const uint64_t oldest = open_epoch > retained_epochs_
        ? open_epoch - retained_epochs_ : 0;
    while (next_fold_epoch_ < oldest)
    {
      crypto::hash digest{};
      if (!membership_.fold_epoch(next_fold_epoch_, open_epoch, digest, &undo->membership))
        return false;
      std::string blob("QWC_EPOSE_HISTORY_ROOT_V2");
      append_bytes(blob, history_root_);
      append_bytes(blob, digest);
      history_root_ = crypto::cn_fast_hash(blob.data(), blob.size());
      const std::vector<crypto::hash> retired = lifecycle_.fold_retired(
          next_fold_epoch_, &undo->lifecycle);
      for (const crypto::hash &retired_digest : retired)
      {
        std::string retired_blob("QWC_EPOSE_RETIRED_HISTORY_ROOT_V2");
        append_bytes(retired_blob, history_root_);
        append_bytes(retired_blob, retired_digest);
        history_root_ = crypto::cn_fast_hash(retired_blob.data(), retired_blob.size());
      }
      ++next_fold_epoch_;
    }
    open_epoch_ = open_epoch;
    window_active_ = true;
    return true;
  }
  pipeline_status_v2 semantic_state_v2::freeze_membership(
      uint64_t epoch, uint64_t height, const crypto::hash &anchor_hash,
      undo_journal *undo)
  {
    if (undo == nullptr)
      return pipeline_status_v2::invalid_configuration;
    if (membership_.read_epoch(epoch) != pipeline_status_v2::accepted)
      return pipeline_status_v2::outside_retained_window;
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
      member.descriptor_version = descriptor.version;
      member.endpoint_transport = descriptor.endpoint_transport;
      member.dial_capabilities = descriptor.dial_capabilities;
      member.dns_address_families = descriptor.dns_address_families;
      authorized_members.push_back(member);
    }
    return membership_.freeze_membership(
        epoch, height, anchor_hash, authorized_members, &undo->membership);
  }
  pipeline_status_v2 semantic_state_v2::close_qualification(
      uint64_t epoch, uint64_t height, undo_journal *undo)
  {
    if (undo == nullptr)
      return pipeline_status_v2::invalid_configuration;
    return membership_.close_qualification(epoch, height, &undo->membership);
  }
  bool semantic_state_v2::reconstruct_chainlock_membership(uint64_t epoch,
      membership_snapshot_v2 &snapshot, qualification_set_v2 &qualification) const
  {
    return epoch != 0 && membership_.get_snapshot(epoch, snapshot, true)
        && membership_.get_qualification(epoch - 1, qualification, true);
  }
  bool semantic_state_v2::chainlock_membership_stored(uint64_t epoch) const
  {
    return epoch != 0 && membership_.snapshot(epoch) != nullptr
        && membership_.qualification(epoch - 1) != nullptr;
  }
  bool semantic_state_v2::retain_chainlock_epoch(uint64_t epoch, undo_journal *undo)
  {
    return undo != nullptr && epoch != 0
        && membership_.epoch_recorded(epoch - 1, true)
        && membership_.epoch_recorded(epoch, false)
        && membership_.retain_carrier(epoch, &undo->membership);
  }
  bool semantic_state_v2::carrier_retained(uint64_t epoch) const
  {
    return membership_.carrier_retained(epoch);
  }
  bool semantic_state_v2::membership_history_needs_pruning(uint64_t current_epoch) const
  {
    return membership_.needs_pruning(current_epoch);
  }
  bool semantic_state_v2::prune_membership_history(uint64_t current_epoch, undo_journal *undo)
  {
    if (undo == nullptr)
      return false;
    membership_.prune_history(current_epoch, &undo->membership);
    return true;
  }

  crypto::hash semantic_state_v2::state_hash() const
  {
    if (window_active_)
    {
      std::string blob("QWC_EPOSE_SEMANTIC_STATE_SCHEMA_2");
      append_bytes(blob, window_parameter_hash_);
      append_bytes(blob, history_root_);
      append_u64_le(blob, retained_epochs_);
      append_u64_le(blob, next_fold_epoch_);
      append_u64_le(blob, open_epoch_);
      append_u64_le(blob, open_epoch_ - next_fold_epoch_);
      for (uint64_t epoch = next_fold_epoch_; epoch < open_epoch_; ++epoch)
      {
        crypto::hash digest{};
        if (membership_.epoch_digest(epoch, digest) != pipeline_status_v2::accepted)
          return crypto::null_hash;
        append_bytes(blob, digest);
      }
      std::string open_transcript;
      if (membership_.epoch_transcript(open_epoch_, open_transcript)
          != pipeline_status_v2::accepted)
        return crypto::null_hash;
      append_u64_le(blob, open_transcript.size());
      blob.append(open_transcript);
      append_bytes(blob, lifecycle_.state_hash());
      return crypto::cn_fast_hash(blob.data(), blob.size());
    }
    return combined_state_hash(lifecycle_.state_hash(), membership_.state_hash());
  }

  std::string semantic_state_v2::serialize_state() const
  {
    state_image_writer_v2 writer;
    writer.bytes("QES2");
    writer.fixed(genesis_hash_);
    writer.fixed(parameter_set_hash_);
    writer.fixed(window_parameter_hash_);
    writer.fixed(history_root_);
    writer.number(retained_epochs_);
    writer.number(next_fold_epoch_);
    writer.number(open_epoch_);
    writer.byte(window_active_ ? 1 : 0);
    if (!lifecycle_.write_image(writer))
      return {};
    if (!membership_.write_image(writer))
      return {};
    return writer.data();
  }

  bool semantic_state_v2::restore_state(const std::string &image)
  {
    state_image_reader_v2 reader(image);
    semantic_state_v2 parsed(nettype_, genesis_hash_, parameter_set_hash_,
        timing_, admission_policy_, membership_.committee_policy(),
        retained_epochs_, window_parameter_hash_);
    std::string magic;
    crypto::hash genesis{};
    crypto::hash parameters{};
    crypto::hash window_parameters{};
    uint8_t active = 0;
    if (!reader.bytes(magic) || magic != "QES2"
        || !reader.fixed(genesis) || genesis != genesis_hash_
        || !reader.fixed(parameters) || parameters != parameter_set_hash_
        || !reader.fixed(window_parameters) || window_parameters != window_parameter_hash_
        || !reader.fixed(parsed.history_root_)
        || !reader.number(parsed.retained_epochs_)
        || parsed.retained_epochs_ != retained_epochs_
        || !reader.number(parsed.next_fold_epoch_)
        || !reader.number(parsed.open_epoch_)
        || !reader.byte(active) || active > 1
        || !parsed.lifecycle_.read_image(reader)
        || !parsed.membership_.read_image(reader, parsed.open_epoch_)
        || !reader.finished())
      return false;
    parsed.window_active_ = active != 0;
    if (parsed.window_active_)
    {
      auto discarded = parsed.lifecycle_.checkpoint();
      parsed.lifecycle_.activate_window(retained_epochs_, &discarded);
    }
    if (parsed.next_fold_epoch_ != parsed.membership_.oldest_retained_epoch()
        || parsed.window_active_ != parsed.membership_.window_active()
        || (parsed.window_active_ && parsed.open_epoch_ < parsed.next_fold_epoch_)
        || parsed.serialize_state() != image)
      return false;
    *this = std::move(parsed);
    return true;
  }

} // namespace epose
} // namespace qwertycoin
