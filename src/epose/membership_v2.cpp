// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/membership_v2.h"
#include "epose/state_image_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/service_receipt_v2.h"
#include "epose/service_node.h"
#include "epose/hash_transcript.h"
#include "epose/checked_math.h"
#include "epose/transport_committee_v2.h"
#include "epose/transport_profile_v2.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>

namespace
{
  using qwertycoin::epose::admission_lease_v2;
  using qwertycoin::epose::frozen_member_v2;
  using qwertycoin::epose::checked_add;

  bool archived_epoch_retained(uint64_t tip, uint64_t epoch, uint64_t epoch_length)
  {
    const uint64_t window = QWC_CHAINLOCK_STORE_RETENTION_BLOCKS / epoch_length
        + (QWC_CHAINLOCK_STORE_RETENTION_BLOCKS % epoch_length != 0) + 1;
    return tip >= epoch && tip - epoch <= window;
  }

  bool retention_elapsed(uint64_t current_epoch, uint64_t epoch)
  {
    return current_epoch > epoch
        && current_epoch - epoch > qwertycoin::epose::EPOSE_STATE_RETENTION_EPOCHS;
  }

  bool checked_mul(uint64_t left, uint64_t right, uint64_t &out)
  {
    if (left != 0 && right > std::numeric_limits<uint64_t>::max() / left)
      return false;
    out = left * right;
    return true;
  }

  using namespace qwertycoin::epose::transcript;

  template <typename T>
  std::string index_key(const T &value)
  {
    return {reinterpret_cast<const char *>(&value), sizeof(value)};
  }

  bool valid_public_key(const crypto::public_key &key)
  {
    return crypto::check_key(key);
  }

  bool same_member_key(const frozen_member_v2 &left, const frozen_member_v2 &right)
  {
    return bytes_equal(left.service_public_key, right.service_public_key);
  }

  bool same_member(const frozen_member_v2 &left, const frozen_member_v2 &right)
  {
    return same_member_key(left, right)
        && bytes_equal(left.identity_id, right.identity_id)
        && bytes_equal(left.operator_authorization_public_key, right.operator_authorization_public_key)
        && bytes_equal(left.descriptor_hash, right.descriptor_hash)
        && bytes_equal(left.reward_binding_hash, right.reward_binding_hash)
        && bytes_equal(left.endpoint_descriptor_hash, right.endpoint_descriptor_hash)
        && left.sequence == right.sequence
        && left.descriptor_version == right.descriptor_version
        && left.endpoint_transport == right.endpoint_transport
        && left.dial_capabilities == right.dial_capabilities
        && left.dns_address_families == right.dns_address_families;
  }

  bool same_lease(const admission_lease_v2 &left, const admission_lease_v2 &right)
  {
    return left.target_epoch == right.target_epoch
        && same_member(left.member, right.member)
        && bytes_equal(left.lease_hash, right.lease_hash);
  }

  bool member_less(const frozen_member_v2 &left, const frozen_member_v2 &right)
  {
    if (bytes_less(left.service_public_key, right.service_public_key))
      return true;
    if (bytes_less(right.service_public_key, left.service_public_key))
      return false;
    return left.sequence < right.sequence;
  }

  void append_member(std::string &blob, const frozen_member_v2 &member)
  {
    append_bytes(blob, member.service_public_key);
    append_bytes(blob, member.identity_id);
    append_bytes(blob, member.operator_authorization_public_key);
    append_bytes(blob, member.descriptor_hash);
    append_bytes(blob, member.reward_binding_hash);
    append_bytes(blob, member.endpoint_descriptor_hash);
    append_u64_le(blob, member.sequence);
    if (member.descriptor_version == 2)
    {
      append_u8(blob, member.endpoint_transport);
      append_u8(blob, member.dial_capabilities);
      append_u8(blob, member.dns_address_families);
    }
  }

  int leading_zero_bits(const crypto::hash &hash)
  {
    int count = 0;
    const auto *bytes = reinterpret_cast<const unsigned char *>(&hash);
    for (size_t index = 0; index < sizeof(hash); ++index)
    {
      const unsigned char byte = bytes[index];
      for (int bit = 7; bit >= 0; --bit)
      {
        if ((byte & (1u << bit)) != 0)
          return count;
        ++count;
      }
    }
    return count;
  }

  bool contains_member(const qwertycoin::epose::membership_snapshot_v2 &snapshot, const crypto::public_key &key)
  {
    return std::binary_search(snapshot.members.begin(), snapshot.members.end(), frozen_member_v2{key}, [](const frozen_member_v2 &member, const frozen_member_v2 &needle) {
      return bytes_less(member.service_public_key, needle.service_public_key);
    });
  }
}

namespace qwertycoin
{
namespace epose
{
  bool admission_policy_v2::valid() const
  {
    return algorithm == admission_work_algorithm_v2::randomx
        && leading_zero_bits > 0
        && context_epoch_offset == 1;
  }

  crypto::hash calculate_admission_work_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context)
  {
    std::string blob("QWC_EPOSE_ADMISSION_WORK_V2");
    append_network(blob, context.nettype);
    append_bytes(blob, context.genesis_hash);
    append_bytes(blob, context.parameter_set_hash);
    append_u64_le(blob, context.height);
    append_bytes(blob, context.block_hash);
    append_member(blob, lease.member);
    append_u64_le(blob, lease.target_epoch);
    append_u8(blob, lease.work_algorithm);
    append_u8(blob, lease.leading_zero_bits);
    append_u64_le(blob, lease.admission_context_height);
    append_bytes(blob, lease.admission_context_hash);
    append_u64_le(blob, lease.nonce);
    crypto::hash work_hash{};
    if (context.nettype == cryptonote::UNDEFINED || context.block_hash == crypto::null_hash)
      return work_hash;
    crypto::rx_slow_hash(context.block_hash.data, blob.data(), blob.size(), work_hash.data);
    return work_hash;
  }

  crypto::hash calculate_admission_lease_hash_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context)
  {
    std::string blob("QWC_EPOSE_ADMISSION_LEASE_V2");
    append_network(blob, context.nettype);
    append_bytes(blob, context.genesis_hash);
    append_bytes(blob, context.parameter_set_hash);
    append_member(blob, lease.member);
    append_u64_le(blob, lease.target_epoch);
    append_u8(blob, lease.work_algorithm);
    append_u8(blob, lease.leading_zero_bits);
    append_u64_le(blob, lease.admission_context_height);
    append_bytes(blob, lease.admission_context_hash);
    append_u64_le(blob, lease.nonce);
    append_bytes(blob, lease.work_hash);
    return hash_blob(blob);
  }

  bool admission_work_meets_target_v2(const crypto::hash &work_hash, uint8_t required_bits)
  {
    return required_bits > 0 && leading_zero_bits(work_hash) >= required_bits;
  }

  bool validate_admission_lease_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      verification_counters_v2 *counters)
  {
    if (!policy.valid() || context.nettype == cryptonote::UNDEFINED
        || context.genesis_hash == crypto::null_hash
        || context.parameter_set_hash == crypto::null_hash
        || context.block_hash == crypto::null_hash
        || !valid_public_key(lease.member.service_public_key)
        || lease.member.identity_id == crypto::null_hash
        || !valid_public_key(lease.member.operator_authorization_public_key)
        || bytes_equal(lease.member.service_public_key, lease.member.operator_authorization_public_key)
        || derive_identity_id_v2(context.nettype, context.genesis_hash, context.parameter_set_hash,
              lease.member.operator_authorization_public_key) != lease.member.identity_id
        || lease.member.descriptor_hash == crypto::null_hash
        || lease.member.reward_binding_hash == crypto::null_hash
        || lease.member.endpoint_descriptor_hash == crypto::null_hash
        || (lease.member.descriptor_version != 1
            && lease.member.descriptor_version != 2)
        || (lease.member.descriptor_version == 1
            && (lease.member.endpoint_transport != 1
                || lease.member.dial_capabilities != 1
                || lease.member.dns_address_families != 0))
        || (lease.member.descriptor_version == 2
            && !valid_transport_profile_v2(
                lease.member.endpoint_transport, lease.member.dial_capabilities,
                lease.member.dns_address_families))
        || lease.target_epoch == 0
        || lease.work_algorithm != static_cast<uint8_t>(policy.algorithm)
        || lease.leading_zero_bits != policy.leading_zero_bits
        || lease.admission_context_height != context.height
        || lease.admission_context_hash != context.block_hash)
      return false;
    if (counters != nullptr)
      ++counters->randomx;
    const crypto::hash expected_work = calculate_admission_work_v2(lease, context);
    return expected_work == lease.work_hash
        && admission_work_meets_target_v2(expected_work, policy.leading_zero_bits)
        && calculate_admission_lease_hash_v2(lease, context) == lease.lease_hash;
  }

  bool epoch_timing_v2::valid() const
  {
    return epoch_length > 0
        && anchor_depth > 0
        && anchor_depth < epoch_length
        && activation_height % epoch_length == 0;
  }

  bool epoch_timing_v2::activation_epoch(uint64_t &epoch) const
  {
    if (!valid())
      return false;
    epoch = activation_height / epoch_length;
    return true;
  }

  bool epoch_timing_v2::first_service_epoch(uint64_t &epoch) const
  {
    uint64_t activation = 0;
    return activation_epoch(activation) && checked_add(activation, 1, epoch);
  }

  bool epoch_timing_v2::first_payout_height(uint64_t &height) const
  {
    uint64_t service_epoch = 0;
    uint64_t payout_epoch = 0;
    return first_service_epoch(service_epoch)
        && checked_add(service_epoch, 1, payout_epoch)
        && epoch_start(payout_epoch, height);
  }

  bool epoch_timing_v2::epoch_start(uint64_t epoch, uint64_t &height) const
  {
    return valid() && checked_mul(epoch, epoch_length, height);
  }

  bool epoch_timing_v2::epoch_of(uint64_t height, uint64_t &epoch) const
  {
    if (!valid())
      return false;
    epoch = height / epoch_length;
    return true;
  }

  bool epoch_timing_v2::epoch_end(uint64_t epoch, uint64_t &height) const
  {
    uint64_t start = 0;
    return epoch_start(epoch, start) && checked_add(start, epoch_length - 1, height);
  }

  bool epoch_timing_v2::enrollment_cutoff(uint64_t epoch, uint64_t &height) const
  {
    uint64_t start = 0;
    if (!epoch_start(epoch, start) || start <= anchor_depth)
      return false;
    height = start - anchor_depth - 1;
    return true;
  }

  bool epoch_timing_v2::committee_anchor(uint64_t epoch, uint64_t &height) const
  {
    uint64_t start = 0;
    if (!epoch_start(epoch, start) || start < anchor_depth)
      return false;
    height = start - anchor_depth;
    return true;
  }

  bool epoch_timing_v2::evidence_deadline(uint64_t epoch, uint64_t &height) const
  {
    uint64_t end = 0;
    if (!epoch_end(epoch, end) || end < anchor_depth)
      return false;
    height = end - anchor_depth;
    return true;
  }

  bool committee_policy_v2::valid() const
  {
    return committee_size > 0
        && threshold > 0
        && threshold == required_receipts_for_committee_size_v2(committee_size)
        && round_count > 0
        && rounds_required > 0
        && rounds_required <= round_count
        && min_compatible_committee > 0
        && service_kind != 0
        && max_active_population >= committee_size
        && round_offsets.size() == round_count
        && !round_offsets.empty()
        && round_offsets.front() == 0
        && std::adjacent_find(round_offsets.begin(), round_offsets.end(),
            [](uint64_t left, uint64_t right) { return left >= right; }) == round_offsets.end();
  }

  size_t required_receipts_for_committee_size_v2(size_t actual_committee_size)
  {
    return actual_committee_size - actual_committee_size / 3;
  }

  bool receipt_inclusion_window_v2(
      const epoch_timing_v2 &timing,
      const committee_policy_v2 &policy,
      const uint64_t epoch,
      const uint64_t round,
      uint64_t &first_height,
      uint64_t &last_height)
  {
    first_height = 0;
    last_height = 0;
    if (!timing.valid() || !policy.valid()
        || round >= policy.round_count
        || round >= policy.round_offsets.size())
      return false;

    uint64_t epoch_start = 0;
    uint64_t evidence_deadline = 0;
    if (!timing.epoch_start(epoch, epoch_start)
        || !timing.evidence_deadline(epoch, evidence_deadline)
        || epoch_start > std::numeric_limits<uint64_t>::max()
            - policy.round_offsets[round])
      return false;

    first_height = epoch_start + policy.round_offsets[round];
    if (round != 0)
    {
      if (first_height == std::numeric_limits<uint64_t>::max())
        return false;
      ++first_height;
    }

    last_height = evidence_deadline;
    if (round + 1 < policy.round_count)
    {
      if (round + 1 >= policy.round_offsets.size()
          || epoch_start > std::numeric_limits<uint64_t>::max()
              - policy.round_offsets[round + 1])
        return false;
      const uint64_t next_round_anchor =
          epoch_start + policy.round_offsets[round + 1];
      if (next_round_anchor == 0)
        return false;
      last_height = next_round_anchor - 1;
    }
    return first_height <= last_height;
  }

  membership_pipeline_v2::membership_pipeline_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const epoch_timing_v2 &timing,
      const admission_policy_v2 &admission_policy,
      const committee_policy_v2 &policy)
    : nettype_(nettype),
      genesis_hash_(genesis_hash),
      parameter_set_hash_(parameter_set_hash),
      timing_(timing),
      admission_policy_(admission_policy),
      policy_(policy),
      valid_(timing.valid()
          && admission_policy.valid()
          && policy.valid()
          && policy.round_offsets.back() < timing.epoch_length - timing.anchor_depth
          && genesis_hash != crypto::null_hash
          && parameter_set_hash != crypto::null_hash)
  {
  }

  state_memory_usage_v2 membership_pipeline_v2::memory_usage() const
  {
    state_memory_usage_v2 usage{};
    const auto count = [&](const auto &epochs) {
      usage.map_value_bytes += epochs.size() * sizeof(typename std::decay_t<decltype(epochs)>::value_type);
      for (const auto &entry : epochs)
      {
        const epoch_bucket &selected = *entry.second;
        usage.leases += selected.leases.size();
        usage.receipts += selected.receipts.size();
        usage.snapshots += selected.snapshot.has_value();
        usage.qualifications += selected.qualification.has_value();
        usage.vector_payload_bytes += selected.leases.capacity() * sizeof(stored_lease)
            + selected.receipts.capacity() * sizeof(stored_receipt)
            + selected.snapshot_lease_hashes.capacity() * sizeof(crypto::hash);
        if (selected.snapshot)
          usage.vector_payload_bytes += selected.snapshot->members.capacity() * sizeof(frozen_member_v2);
        if (selected.qualification)
          usage.vector_payload_bytes += selected.qualification->qualified_nodes.capacity() * sizeof(crypto::public_key);
      }
    };
    count(epochs_);
    count(historical_epochs_);
    usage.map_value_bytes += carrier_epochs_.size() * sizeof(decltype(carrier_epochs_)::value_type);
    return usage;
  }

  membership_pipeline_v2::undo_journal membership_pipeline_v2::checkpoint() const
  {
    undo_journal undo{};
    undo.prior_receipt_count = receipt_count_;
    undo.prior_oldest_retained_epoch = oldest_retained_epoch_;
    undo.prior_archive_tip_epoch = archive_tip_epoch_;
    undo.prior_next_prune_epoch = next_prune_epoch_;
    undo.prior_window_active = window_active_;
    return undo;
  }

  void membership_pipeline_v2::rollback(const undo_journal &undo) noexcept
  {
    for (auto it = undo.pruned_buckets.rbegin(); it != undo.pruned_buckets.rend(); ++it)
      epochs_[it->epoch] = it->prior;
    for (auto it = undo.inserted_qualifications.rbegin();
        it != undo.inserted_qualifications.rend(); ++it)
      epochs_.at(*it)->qualification = boost::none;
    for (auto it = undo.appended_receipts.rbegin();
        it != undo.appended_receipts.rend(); ++it)
    {
      epoch_bucket &selected = *epochs_.at(it->epoch);
      while (selected.receipts.size() > it->size)
      {
        const stored_receipt &last = selected.receipts.back();
        const std::string subject = index_key(last.subject_public_key);
        selected.receipt_by_slot.erase(std::make_tuple(last.round, subject,
            index_key(last.verifier_public_key)));
        const auto coverage = selected.receipt_coverage.find({last.round, subject});
        if (coverage != selected.receipt_coverage.end()
            && --coverage->second == 0)
          selected.receipt_coverage.erase(coverage);
        selected.receipts.pop_back();
      }
      selected.receipt_transcript_valid = false;
      selected.digest_cache.reset();
    }
    for (auto it = undo.inserted_snapshots.rbegin();
        it != undo.inserted_snapshots.rend(); ++it)
      epochs_.at(*it)->snapshot = boost::none;
    for (auto it = undo.replaced_leases.rbegin();
        it != undo.replaced_leases.rend(); ++it)
    {
      epoch_bucket &selected = *epochs_.at(it->epoch);
      selected.lease_by_service.erase(index_key(
          selected.leases[it->index].lease.member.service_public_key));
      selected.leases[it->index] = {it->lease, it->inclusion_height};
      selected.lease_by_service[index_key(it->lease.member.service_public_key)] = it->index;
      selected.lease_transcript_valid = false;
      selected.digest_cache.reset();
    }
    for (auto it = undo.appended_leases.rbegin();
        it != undo.appended_leases.rend(); ++it)
    {
      epoch_bucket &selected = *epochs_.at(it->epoch);
      while (selected.leases.size() > it->size)
      {
        const admission_lease_v2 &last = selected.leases.back().lease;
        selected.lease_by_identity.erase(index_key(last.member.identity_id));
        selected.lease_by_service.erase(index_key(last.member.service_public_key));
        selected.leases.pop_back();
      }
      selected.lease_transcript_valid = false;
      selected.digest_cache.reset();
    }
    for (auto it = undo.created_buckets.rbegin();
        it != undo.created_buckets.rend(); ++it)
      epochs_.erase(*it);
    for (auto it = undo.buckets.rbegin(); it != undo.buckets.rend(); ++it)
      epochs_[it->epoch] = it->prior;
    for (auto it = undo.archived_buckets.rbegin(); it != undo.archived_buckets.rend(); ++it)
    {
      if (it->prior)
        historical_epochs_[it->epoch] = it->prior;
      else
        historical_epochs_.erase(it->epoch);
    }
    for (auto it = undo.evicted_carriers.rbegin(); it != undo.evicted_carriers.rend(); ++it)
      carrier_epochs_.insert(*it);
    for (auto it = undo.inserted_carriers.rbegin(); it != undo.inserted_carriers.rend(); ++it)
      carrier_epochs_.erase(*it);
    receipt_count_ = undo.prior_receipt_count;
    oldest_retained_epoch_ = undo.prior_oldest_retained_epoch;
    archive_tip_epoch_ = undo.prior_archive_tip_epoch;
    next_prune_epoch_ = undo.prior_next_prune_epoch;
    window_active_ = undo.prior_window_active;
    state_hash_cache_.reset();
  }

  bool membership_pipeline_v2::valid() const
  {
    return valid_;
  }

  void membership_pipeline_v2::activate_transport_window()
  {
    window_active_ = true;
    state_hash_cache_.reset();
  }

  const epoch_timing_v2 &membership_pipeline_v2::timing() const
  {
    return timing_;
  }

  const committee_policy_v2 &membership_pipeline_v2::committee_policy() const
  {
    return policy_;
  }

  const membership_pipeline_v2::epoch_bucket *membership_pipeline_v2::bucket(
      uint64_t epoch) const
  {
    const auto found = epochs_.find(epoch);
    return found == epochs_.end() ? nullptr : found->second.get();
  }

  const membership_pipeline_v2::epoch_bucket *membership_pipeline_v2::recorded_bucket(
      uint64_t epoch) const
  {
    const epoch_bucket *selected = bucket(epoch);
    if (selected != nullptr)
      return selected;
    const auto found = historical_epochs_.find(epoch);
    return found == historical_epochs_.end() ? nullptr : found->second.get();
  }

  void membership_pipeline_v2::record_receipt_size(uint64_t epoch, undo_journal *undo)
  {
    if (undo != nullptr)
    {
      const epoch_bucket *selected = bucket(epoch);
      if (selected != nullptr)
        undo->appended_receipts.push_back({epoch, selected->receipts.size()});
    }
  }

  membership_pipeline_v2::epoch_bucket &membership_pipeline_v2::mutable_bucket(
      uint64_t epoch, undo_journal *undo)
  {
    auto &selected = epochs_[epoch];
    if (!selected)
    {
      if (undo != nullptr)
        undo->created_buckets.push_back(epoch);
      selected = std::make_shared<epoch_bucket>();
    }
    else if (!selected.unique())
      selected = std::make_shared<epoch_bucket>(*selected);
    selected->digest_cache.reset();
    state_hash_cache_.reset();
    return *selected;
  }

  std::vector<const membership_pipeline_v2::stored_lease *>
  membership_pipeline_v2::sorted_leases(const epoch_bucket &selected) const
  {
    std::vector<const stored_lease *> sorted;
    sorted.reserve(selected.leases.size());
    for (const stored_lease &stored : selected.leases)
      sorted.push_back(&stored);
    std::sort(sorted.begin(), sorted.end(),
        [](const stored_lease *left, const stored_lease *right) {
          return member_less(left->lease.member, right->lease.member);
        });
    return sorted;
  }

  const std::string &membership_pipeline_v2::lease_transcript(
      const epoch_bucket &selected) const
  {
    if (selected.lease_transcript_valid)
      return selected.lease_transcript;
    const auto sorted = sorted_leases(selected);
    selected.lease_transcript.clear();
    for (const stored_lease *stored : sorted)
    {
      append_u64_le(selected.lease_transcript, stored->lease.target_epoch);
      append_member(selected.lease_transcript, stored->lease.member);
      append_bytes(selected.lease_transcript, stored->lease.lease_hash);
      append_u64_le(selected.lease_transcript, stored->inclusion_height);
    }
    selected.lease_transcript_valid = true;
    return selected.lease_transcript;
  }

  std::vector<const membership_pipeline_v2::stored_receipt *>
  membership_pipeline_v2::sorted_receipts(const epoch_bucket &selected) const
  {
    std::vector<const stored_receipt *> sorted;
    sorted.reserve(selected.receipts.size());
    for (const stored_receipt &stored : selected.receipts)
      sorted.push_back(&stored);
    std::sort(sorted.begin(), sorted.end(),
        [](const stored_receipt *left, const stored_receipt *right) {
          if (left->round != right->round)
            return left->round < right->round;
          if (bytes_less(left->subject_public_key, right->subject_public_key))
            return true;
          if (bytes_less(right->subject_public_key, left->subject_public_key))
            return false;
          return bytes_less(left->verifier_public_key, right->verifier_public_key);
        });
    return sorted;
  }

  const std::string &membership_pipeline_v2::receipt_transcript(
      const epoch_bucket &selected) const
  {
    if (selected.receipt_transcript_valid)
      return selected.receipt_transcript;
    const auto sorted = sorted_receipts(selected);
    selected.receipt_transcript.clear();
    for (const stored_receipt *stored : sorted)
    {
      append_u64_le(selected.receipt_transcript, stored->epoch);
      append_u64_le(selected.receipt_transcript, stored->round);
      append_u8(selected.receipt_transcript, stored->service_kind);
      append_bytes(selected.receipt_transcript, stored->subject_public_key);
      append_bytes(selected.receipt_transcript, stored->verifier_public_key);
      append_bytes(selected.receipt_transcript, stored->receipt_hash);
      append_u64_le(selected.receipt_transcript, stored->inclusion_height);
    }
    selected.receipt_transcript_valid = true;
    return selected.receipt_transcript;
  }

  pipeline_status_v2 membership_pipeline_v2::apply_admission(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context,
      uint64_t inclusion_height,
      verification_counters_v2 *counters,
      undo_journal *undo)
  {
    if (undo == nullptr || !valid_)
      return pipeline_status_v2::invalid_configuration;
    if (context.nettype != nettype_ || context.genesis_hash != genesis_hash_
        || context.parameter_set_hash != parameter_set_hash_
        || context.height >= inclusion_height)
      return pipeline_status_v2::invalid_member;
    if (inclusion_height < timing_.activation_height)
      return pipeline_status_v2::before_activation;
    if (read_epoch(lease.target_epoch) != pipeline_status_v2::accepted)
      return pipeline_status_v2::outside_retained_window;
    uint64_t first_service = 0;
    uint64_t cutoff = 0;
    uint64_t expected_context_height = 0;
    if (!timing_.first_service_epoch(first_service)
        || lease.target_epoch < first_service
        || lease.target_epoch < admission_policy_.context_epoch_offset
        || !timing_.epoch_start(lease.target_epoch - admission_policy_.context_epoch_offset,
              expected_context_height)
        || context.height != expected_context_height
        || !timing_.enrollment_cutoff(lease.target_epoch, cutoff))
      return pipeline_status_v2::invalid_epoch;
    if (inclusion_height > cutoff)
      return pipeline_status_v2::too_late;
    if (!valid_public_key(lease.member.service_public_key)
        || !valid_public_key(lease.member.operator_authorization_public_key)
        || bytes_equal(lease.member.service_public_key, lease.member.operator_authorization_public_key)
        || lease.member.identity_id == crypto::null_hash)
      return pipeline_status_v2::invalid_member;
    const epoch_bucket *selected = bucket(lease.target_epoch);
    if (selected != nullptr && selected->snapshot.has_value())
      return pipeline_status_v2::too_late;
    if (!validate_admission_lease_v2(lease, context, admission_policy_, counters))
      return pipeline_status_v2::invalid_member;

    const std::string identity_key = index_key(lease.member.identity_id);
    const std::string service_key = index_key(lease.member.service_public_key);
    if (selected != nullptr)
    {
      const auto service = selected->lease_by_service.find(service_key);
      if (service != selected->lease_by_service.end()
          && selected->leases[service->second].lease.member.identity_id
              != lease.member.identity_id)
        return pipeline_status_v2::conflicting_record;
      const auto identity = selected->lease_by_identity.find(identity_key);
      if (identity != selected->lease_by_identity.end())
      {
        const stored_lease &stored = selected->leases[identity->second];
        if (same_lease(stored.lease, lease))
          return pipeline_status_v2::idempotent_duplicate;
        if (lease.member.sequence > stored.lease.member.sequence)
        {
          if (undo != nullptr)
            undo->replaced_leases.push_back({lease.target_epoch, identity->second,
                stored.lease, stored.inclusion_height});
          epoch_bucket &updated = mutable_bucket(lease.target_epoch, undo);
          updated.lease_by_service.erase(index_key(stored.lease.member.service_public_key));
          updated.leases[identity->second] = {lease, inclusion_height};
          updated.lease_by_service[service_key] = identity->second;
          updated.lease_transcript_valid = false;
          return pipeline_status_v2::accepted;
        }
        return pipeline_status_v2::conflicting_record;
      }
    }

    const size_t epoch_population = selected == nullptr ? 0 : selected->leases.size();
    if (epoch_population >= policy_.max_active_population)
      return pipeline_status_v2::population_limit_exceeded;

    if (undo != nullptr)
      undo->appended_leases.push_back({lease.target_epoch, epoch_population});
    epoch_bucket &updated = mutable_bucket(lease.target_epoch, undo);
    const size_t index = updated.leases.size();
    updated.leases.push_back({lease, inclusion_height});
    updated.lease_by_identity.emplace(identity_key, index);
    updated.lease_by_service.emplace(service_key, index);
    updated.lease_transcript_valid = false;
    return pipeline_status_v2::accepted;
  }

  pipeline_status_v2 membership_pipeline_v2::freeze_membership(
      uint64_t epoch,
      uint64_t height,
      const crypto::hash &anchor_hash,
      undo_journal *undo)
  {
    return freeze_membership_impl(epoch, height, anchor_hash, nullptr, undo);
  }

  pipeline_status_v2 membership_pipeline_v2::freeze_membership(
      uint64_t epoch,
      uint64_t height,
      const crypto::hash &anchor_hash,
      const std::vector<frozen_member_v2> &authorized_members,
      undo_journal *undo)
  {
    return freeze_membership_impl(epoch, height, anchor_hash, &authorized_members, undo);
  }

  crypto::hash membership_pipeline_v2::snapshot_hash(
      const membership_snapshot_v2 &snapshot) const
  {
    std::string blob("QWC_EPOSE_SNAPSHOT_V2");
    append_network(blob, nettype_);
    append_bytes(blob, genesis_hash_);
    append_bytes(blob, parameter_set_hash_);
    append_u64_le(blob, snapshot.epoch);
    append_u64_le(blob, snapshot.cutoff_height);
    append_u64_le(blob, snapshot.anchor_height);
    append_bytes(blob, snapshot.anchor_hash);
    append_u64_le(blob, snapshot.members.size());
    for (const frozen_member_v2 &member : snapshot.members)
      append_member(blob, member);
    if (snapshot.transport_aware)
      append_u8(blob, 1);
    return hash_blob(blob);
  }

  crypto::hash membership_pipeline_v2::qualification_hash(
      const qualification_set_v2 &qualification) const
  {
    std::string blob("QWC_EPOSE_QUALIFICATION_V2");
    append_network(blob, nettype_);
    append_bytes(blob, genesis_hash_);
    append_bytes(blob, parameter_set_hash_);
    append_bytes(blob, qualification.snapshot_hash);
    append_u64_le(blob, qualification.epoch);
    append_u64_le(blob, qualification.closed_height);
    append_u64_le(blob, qualification.qualified_nodes.size());
    for (const crypto::public_key &key : qualification.qualified_nodes)
      append_bytes(blob, key);
    return hash_blob(blob);
  }

  pipeline_status_v2 membership_pipeline_v2::freeze_membership_impl(
      uint64_t epoch,
      uint64_t height,
      const crypto::hash &anchor_hash,
      const std::vector<frozen_member_v2> *authorized_members,
      undo_journal *undo)
  {
    if (undo == nullptr || !valid_)
      return pipeline_status_v2::invalid_configuration;
    if (read_epoch(epoch) != pipeline_status_v2::accepted)
      return pipeline_status_v2::outside_retained_window;
    const epoch_bucket *selected = bucket(epoch);
    if (selected != nullptr && selected->snapshot.has_value())
      return pipeline_status_v2::snapshot_already_frozen;

    uint64_t cutoff = 0;
    uint64_t expected_anchor = 0;
    uint64_t first_service = 0;
    if (!timing_.first_service_epoch(first_service)
        || epoch < first_service
        || !timing_.enrollment_cutoff(epoch, cutoff)
        || !timing_.committee_anchor(epoch, expected_anchor))
      return pipeline_status_v2::invalid_epoch;
    if (height != expected_anchor)
      return pipeline_status_v2::wrong_boundary;

    membership_snapshot_v2 frozen{};
    frozen.epoch = epoch;
    frozen.cutoff_height = cutoff;
    frozen.anchor_height = height;
    frozen.anchor_hash = anchor_hash;
    frozen.transport_aware = window_active_;
    if (selected != nullptr)
    {
      for (const stored_lease &stored : selected->leases)
      {
        if (stored.lease.target_epoch != epoch || stored.inclusion_height > cutoff)
          continue;
        if (authorized_members != nullptr)
        {
          const auto authorized = std::find_if(
              authorized_members->begin(), authorized_members->end(),
              [&](const frozen_member_v2 &member) {
                return member.identity_id == stored.lease.member.identity_id;
              });
          if (authorized == authorized_members->end()
              || !same_member(*authorized, stored.lease.member))
            continue;
        }
        const auto duplicate_identity_or_key = std::find_if(
            frozen.members.begin(), frozen.members.end(),
            [&](const frozen_member_v2 &member) {
              return member.identity_id == stored.lease.member.identity_id
                  || bytes_equal(
                      member.service_public_key,
                      stored.lease.member.service_public_key);
            });
        if (duplicate_identity_or_key != frozen.members.end())
          return pipeline_status_v2::conflicting_record;
        frozen.members.push_back(stored.lease.member);
        if (frozen.members.size() > policy_.max_active_population)
          return pipeline_status_v2::population_limit_exceeded;
      }
    }
    std::sort(frozen.members.begin(), frozen.members.end(), member_less);
    std::vector<crypto::hash> selected_leases;
    selected_leases.reserve(frozen.members.size());
    for (const frozen_member_v2 &member : frozen.members)
    {
      const auto found = std::find_if(selected->leases.begin(), selected->leases.end(),
          [&](const stored_lease &lease) { return same_member(lease.lease.member, member); });
      if (found == selected->leases.end())
        return pipeline_status_v2::conflicting_record;
      selected_leases.push_back(found->lease.lease_hash);
    }

    frozen.snapshot_hash = snapshot_hash(frozen);
    epoch_bucket &updated = mutable_bucket(epoch, undo);
    if (undo != nullptr)
      undo->inserted_snapshots.push_back(epoch);
    updated.snapshot = std::move(frozen);
    updated.snapshot_lease_hashes = std::move(selected_leases);
    return pipeline_status_v2::accepted;
  }

  std::vector<verifier_assignment_v2> membership_pipeline_v2::committee(
      uint64_t epoch,
      uint64_t round,
      const crypto::public_key &subject_public_key,
      const crypto::hash &round_anchor_hash) const
  {
    std::vector<verifier_assignment_v2> result;
    const membership_snapshot_v2 *frozen = snapshot(epoch);
    if (!valid_ || frozen == nullptr || frozen->pruned || round >= policy_.round_count
        || round_anchor_hash == crypto::null_hash
        || (round == 0 && round_anchor_hash != frozen->anchor_hash)
        || !contains_member(*frozen, subject_public_key))
      return result;
    if (frozen->members.size() <= 1)
      return result;

    const auto subject = std::find_if(frozen->members.begin(), frozen->members.end(),
        [&](const frozen_member_v2 &member) {
          return bytes_equal(member.service_public_key, subject_public_key);
        });

    for (const frozen_member_v2 &candidate : frozen->members)
    {
      if (bytes_equal(candidate.service_public_key, subject_public_key))
        continue;
      if (frozen->transport_aware && !transport_compatible_v2(candidate, *subject))
        continue;
      std::string blob("QWC_EPOSE_SELECT_V2");
      append_network(blob, nettype_);
      append_bytes(blob, genesis_hash_);
      append_bytes(blob, parameter_set_hash_);
      append_bytes(blob, frozen->snapshot_hash);
      append_u64_le(blob, epoch);
      append_u64_le(blob, round);
      append_bytes(blob, round_anchor_hash);
      append_bytes(blob, subject_public_key);
      append_bytes(blob, candidate.service_public_key);
      result.push_back({candidate.service_public_key, hash_blob(blob)});
    }

    std::sort(result.begin(), result.end(), [](const verifier_assignment_v2 &left, const verifier_assignment_v2 &right) {
      if (bytes_less(left.selection_score, right.selection_score))
        return true;
      if (bytes_less(right.selection_score, left.selection_score))
        return false;
      return bytes_less(left.verifier_public_key, right.verifier_public_key);
    });
    if (result.size() > policy_.committee_size)
      result.resize(policy_.committee_size);
    return result;
  }

  pipeline_status_v2 membership_pipeline_v2::apply_authenticated_receipt(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context,
      uint64_t inclusion_height,
      const crypto::hash &canonical_round_anchor_hash,
      verification_counters_v2 *counters)
  {
    if (!valid_)
      return pipeline_status_v2::invalid_configuration;
    if (context.nettype != nettype_ || context.genesis_hash != genesis_hash_
        || context.parameter_set_hash != parameter_set_hash_)
      return pipeline_status_v2::receipt_not_prevalidated;
    const service_challenge_v2 &challenge = receipt.challenge;
    if (read_epoch(challenge.epoch) != pipeline_status_v2::accepted)
      return pipeline_status_v2::outside_retained_window;
    if (challenge.service_kind != policy_.service_kind)
      return pipeline_status_v2::invalid_service_kind;
    const membership_snapshot_v2 *frozen = snapshot(challenge.epoch);
    if (frozen == nullptr)
      return pipeline_status_v2::snapshot_missing;
    if (challenge.snapshot_hash != frozen->snapshot_hash
        || canonical_round_anchor_hash == crypto::null_hash
        || challenge.anchor_hash != canonical_round_anchor_hash
        || (challenge.round == 0 && challenge.anchor_hash != frozen->anchor_hash))
      return pipeline_status_v2::receipt_not_prevalidated;
    uint64_t first_inclusion = 0;
    uint64_t round_end = 0;
    if (!receipt_inclusion_window_v2(
            timing_, policy_, challenge.epoch, challenge.round,
            first_inclusion, round_end))
      return pipeline_status_v2::invalid_epoch;
    if (inclusion_height < first_inclusion)
      return pipeline_status_v2::invalid_epoch;
    if (inclusion_height > round_end)
      return pipeline_status_v2::too_late;
    const auto subject = std::find_if(frozen->members.begin(), frozen->members.end(), [&](const frozen_member_v2 &member) {
      return bytes_equal(member.service_public_key, challenge.subject_public_key);
    });
    if (subject == frozen->members.end())
      return pipeline_status_v2::subject_not_in_snapshot;
    if (challenge.endpoint_descriptor_hash != subject->endpoint_descriptor_hash)
      return pipeline_status_v2::receipt_not_prevalidated;
    if (!contains_member(*frozen, challenge.verifier_public_key))
      return pipeline_status_v2::verifier_not_in_snapshot;

    const std::vector<verifier_assignment_v2> selected = committee(
        challenge.epoch, challenge.round, challenge.subject_public_key, challenge.anchor_hash);
    if (std::find_if(selected.begin(), selected.end(), [&](const verifier_assignment_v2 &assignment) {
          return bytes_equal(assignment.verifier_public_key, challenge.verifier_public_key);
        }) == selected.end())
      return pipeline_status_v2::verifier_not_selected;

    // Authenticate before classifying a same-slot race. Relay policy may
    // safely treat an authentic late variant as expected, but an attacker
    // cannot gain that classification by merely claiming a confirmed slot.
    if (!validate_authenticated_service_receipt_v2(receipt, context, counters))
      return pipeline_status_v2::receipt_not_prevalidated;

    const crypto::hash receipt_hash = hash_authenticated_service_receipt_v2(receipt, context);
    const epoch_bucket *selected_bucket = bucket(challenge.epoch);
    const std::string subject_key = index_key(challenge.subject_public_key);
    const std::string verifier_key = index_key(challenge.verifier_public_key);
    const auto slot_key = std::make_tuple(challenge.round, subject_key, verifier_key);
    if (selected_bucket != nullptr)
    {
      const auto slot = selected_bucket->receipt_by_slot.find(slot_key);
      if (slot != selected_bucket->receipt_by_slot.end())
      {
        const stored_receipt &stored = selected_bucket->receipts[slot->second];
        return bytes_equal(stored.receipt_hash, receipt_hash)
            ? pipeline_status_v2::idempotent_duplicate
            : pipeline_status_v2::receipt_slot_conflict;
      }
    }
    // A closed qualification set is immutable, but the relay layer still
    // needs an authenticated classification for late transport races. Keep
    // this check after canonical-context, membership, committee and signature
    // validation (and after confirmed-slot duplicate detection) so callers
    // may discard a genuine late receipt without treating an unauthenticated
    // claim as benign.
    if (selected_bucket != nullptr && selected_bucket->qualification.has_value())
      return pipeline_status_v2::qualification_already_closed;
    epoch_bucket &updated = mutable_bucket(challenge.epoch, nullptr);
    const size_t index = updated.receipts.size();
    updated.receipts.push_back({challenge.epoch, challenge.round, challenge.service_kind,
        challenge.subject_public_key, challenge.verifier_public_key, receipt_hash, inclusion_height});
    updated.receipt_by_slot.emplace(slot_key, index);
    ++updated.receipt_coverage[{challenge.round, subject_key}];
    ++receipt_count_;
    updated.receipt_transcript_valid = false;
    return pipeline_status_v2::accepted;
  }

  pipeline_status_v2 membership_pipeline_v2::close_qualification(
      uint64_t epoch, uint64_t height, undo_journal *undo)
  {
    if (undo == nullptr || !valid_)
      return pipeline_status_v2::invalid_configuration;
    if (read_epoch(epoch) != pipeline_status_v2::accepted)
      return pipeline_status_v2::outside_retained_window;
    const epoch_bucket *selected_bucket = bucket(epoch);
    if (selected_bucket != nullptr && selected_bucket->qualification.has_value())
      return pipeline_status_v2::qualification_already_closed;
    const membership_snapshot_v2 *frozen = snapshot(epoch);
    if (frozen == nullptr)
      return pipeline_status_v2::snapshot_missing;
    uint64_t deadline = 0;
    if (!timing_.evidence_deadline(epoch, deadline))
      return pipeline_status_v2::invalid_epoch;
    if (height != deadline)
      return pipeline_status_v2::wrong_boundary;

    qualification_set_v2 closed{};
    closed.epoch = epoch;
    closed.closed_height = height;
    closed.snapshot_hash = frozen->snapshot_hash;

    if (frozen->members.size() > 1)
    {
      for (const frozen_member_v2 &subject : frozen->members)
      {
        const size_t actual_committee_size = compatible_committee_size_v2(
            *frozen, subject, policy_.committee_size);
        if (!compatible_committee_judged_v2(
                *frozen, actual_committee_size, policy_.min_compatible_committee))
          continue;
        const size_t required_receipts =
            required_receipts_for_committee_size_v2(actual_committee_size);
        uint64_t passing_rounds = 0;
        for (uint64_t round = 0; round < policy_.round_count; ++round)
        {
          const auto coverage_key = std::make_pair(
              round, index_key(subject.service_public_key));
          size_t voters = 0;
          if (selected_bucket != nullptr)
          {
            const auto coverage = selected_bucket->receipt_coverage.find(coverage_key);
            if (coverage != selected_bucket->receipt_coverage.end())
              voters = coverage->second;
          }
          if (voters >= required_receipts)
            ++passing_rounds;
        }
        if (passing_rounds >= policy_.rounds_required)
          closed.qualified_nodes.push_back(subject.service_public_key);
      }
    }

    closed.qualification_hash = qualification_hash(closed);
    epoch_bucket &updated = mutable_bucket(epoch, undo);
    if (undo != nullptr)
      undo->inserted_qualifications.push_back(epoch);
    updated.qualification = std::move(closed);
    return pipeline_status_v2::accepted;
  }

  const membership_snapshot_v2 *membership_pipeline_v2::snapshot(uint64_t epoch) const
  {
    const epoch_bucket *selected = recorded_bucket(epoch);
    return selected != nullptr && selected->snapshot.has_value() && !selected->snapshot->pruned
        ? &*selected->snapshot : nullptr;
  }

  const qualification_set_v2 *membership_pipeline_v2::qualification(uint64_t epoch) const
  {
    const epoch_bucket *selected = recorded_bucket(epoch);
    return selected != nullptr && selected->qualification.has_value() && !selected->qualification->pruned
        ? &*selected->qualification : nullptr;
  }

  bool membership_pipeline_v2::qualification_pruned(uint64_t epoch) const
  {
    const epoch_bucket *selected = recorded_bucket(epoch);
    return selected != nullptr && selected->qualification.has_value()
        && selected->qualification->pruned;
  }

  bool membership_pipeline_v2::get_snapshot(uint64_t epoch,
      membership_snapshot_v2 &result, bool allow_reconstruct) const
  {
    const auto *stored = snapshot(epoch);
    if (stored == nullptr)
      return allow_reconstruct && reconstruct_epoch(epoch, result, nullptr);
    result = *stored;
    return true;
  }

  bool membership_pipeline_v2::get_qualification(uint64_t epoch,
      qualification_set_v2 &result, bool allow_reconstruct) const
  {
    const auto *stored = qualification(epoch);
    if (stored == nullptr)
    {
      membership_snapshot_v2 restored;
      return allow_reconstruct && reconstruct_epoch(epoch, restored, &result);
    }
    result = *stored;
    return true;
  }

  bool membership_pipeline_v2::reconstruct_epoch(uint64_t epoch,
      membership_snapshot_v2 &restored_snapshot,
      qualification_set_v2 *restored_qualification) const
  {
    const epoch_bucket *selected = recorded_bucket(epoch);
    if (selected == nullptr || !selected->snapshot.has_value())
      return false;
    membership_snapshot_v2 rebuilt = *selected->snapshot;
    rebuilt.members.clear();
    for (const crypto::hash &hash : selected->snapshot_lease_hashes)
    {
      const auto found = std::find_if(selected->leases.begin(), selected->leases.end(),
          [&](const stored_lease &lease) { return lease.lease.lease_hash == hash; });
      if (found == selected->leases.end() || found->inclusion_height > rebuilt.cutoff_height)
        return false;
      rebuilt.members.push_back(found->lease.member);
    }
    std::sort(rebuilt.members.begin(), rebuilt.members.end(), member_less);
    rebuilt.pruned = false;
    if (snapshot_hash(rebuilt) != selected->snapshot->snapshot_hash)
      return false;
    if (restored_qualification != nullptr)
    {
      if (!selected->qualification.has_value())
        return false;
      membership_pipeline_v2 temporary(nettype_, genesis_hash_, parameter_set_hash_,
          timing_, admission_policy_, policy_);
      auto copy = std::make_shared<epoch_bucket>(*selected);
      copy->snapshot = rebuilt;
      copy->qualification = boost::none;
      temporary.epochs_.emplace(epoch, std::move(copy));
      auto undo = temporary.checkpoint();
      if (temporary.close_qualification(epoch, selected->qualification->closed_height, &undo)
          != pipeline_status_v2::accepted)
        return false;
      const auto *closed = temporary.qualification(epoch);
      if (closed == nullptr || closed->qualification_hash != selected->qualification->qualification_hash)
        return false;
      *restored_qualification = *closed;
    }
    restored_snapshot = std::move(rebuilt);
    return true;
  }

  bool membership_pipeline_v2::epoch_recorded(uint64_t epoch, bool require_qualification) const
  {
    const epoch_bucket *selected = recorded_bucket(epoch);
    return selected != nullptr && selected->snapshot.has_value()
        && (!require_qualification || selected->qualification.has_value());
  }

  bool membership_pipeline_v2::retain_carrier(uint64_t epoch, undo_journal *undo)
  {
    if (carrier_epochs_.insert(epoch).second && undo != nullptr)
      undo->inserted_carriers.push_back(epoch);
    return true;
  }

  bool membership_pipeline_v2::carrier_retained(uint64_t epoch) const
  {
    return carrier_epochs_.count(epoch) != 0;
  }

  membership_pipeline_v2::prune_targets membership_pipeline_v2::prune_targets_of(
      uint64_t epoch, const epoch_bucket &selected) const
  {
    const bool carrier = carrier_retained(epoch);
    const bool next_carrier = epoch != std::numeric_limits<uint64_t>::max()
        && carrier_retained(epoch + 1);
    return {
        selected.snapshot && !selected.snapshot->pruned && !carrier,
        selected.qualification && !selected.qualification->pruned && !carrier && !next_carrier};
  }

  bool membership_pipeline_v2::needs_pruning(uint64_t current_epoch) const
  {
    const auto needs = [&](const auto &epochs) {
      for (auto entry = epochs.lower_bound(next_prune_epoch_);
          entry != epochs.end(); ++entry)
      {
        if (!retention_elapsed(current_epoch, entry->first))
          break;
        const prune_targets targets = prune_targets_of(entry->first, *entry->second);
        if (targets.snapshot || targets.qualification)
          return true;
      }
      return false;
    };
    return needs(epochs_) || needs(historical_epochs_);
  }

  void membership_pipeline_v2::prune_history(uint64_t current_epoch, undo_journal *undo)
  {
    archive_tip_epoch_ = std::max(archive_tip_epoch_, current_epoch);
    const auto prune = [&](auto &epochs, bool archived) {
      for (auto entry = epochs.lower_bound(next_prune_epoch_);
          entry != epochs.end(); ++entry)
      {
        if (!retention_elapsed(current_epoch, entry->first))
          break;
        const prune_targets targets = prune_targets_of(entry->first, *entry->second);
        if (!targets.snapshot && !targets.qualification)
          continue;
        if (undo != nullptr)
        {
          if (archived)
            undo->archived_buckets.push_back({entry->first, entry->second});
          else
            undo->pruned_buckets.push_back({entry->first, entry->second});
        }
        entry->second = std::make_shared<epoch_bucket>(*entry->second);
        if (targets.snapshot)
        {
          std::vector<frozen_member_v2>().swap(entry->second->snapshot->members);
          entry->second->snapshot->pruned = true;
        }
        if (targets.qualification)
        {
          std::vector<crypto::public_key>().swap(entry->second->qualification->qualified_nodes);
          entry->second->qualification->pruned = true;
        }
      }
    };
    prune(epochs_, false);
    prune(historical_epochs_, true);
    if (current_epoch > EPOSE_STATE_RETENTION_EPOCHS)
      next_prune_epoch_ = current_epoch - EPOSE_STATE_RETENTION_EPOCHS;
  }

  pipeline_status_v2 membership_pipeline_v2::read_epoch(uint64_t epoch) const
  {
    return window_active_ && epoch < oldest_retained_epoch_
        ? pipeline_status_v2::outside_retained_window
        : pipeline_status_v2::accepted;
  }

  uint64_t membership_pipeline_v2::oldest_retained_epoch() const
  {
    return oldest_retained_epoch_;
  }

  bool membership_pipeline_v2::window_active() const
  {
    return window_active_;
  }

  pipeline_status_v2 membership_pipeline_v2::epoch_transcript(
      uint64_t epoch, std::string &out) const
  {
    out.clear();
    const pipeline_status_v2 status = read_epoch(epoch);
    if (status != pipeline_status_v2::accepted)
      return status;
    const epoch_bucket *selected = bucket(epoch);
    append_u64_le(out, epoch);
    append_u64_le(out, selected == nullptr ? 0 : selected->leases.size());
    if (selected != nullptr)
      out.append(lease_transcript(*selected));
    append_u8(out, selected != nullptr && selected->snapshot.has_value());
    if (selected != nullptr && selected->snapshot.has_value())
      append_bytes(out, selected->snapshot->snapshot_hash);
    append_u64_le(out, selected == nullptr ? 0 : selected->receipts.size());
    if (selected != nullptr)
      out.append(receipt_transcript(*selected));
    append_u8(out, selected != nullptr && selected->qualification.has_value());
    if (selected != nullptr && selected->qualification.has_value())
      append_bytes(out, selected->qualification->qualification_hash);
    return pipeline_status_v2::accepted;
  }

  pipeline_status_v2 membership_pipeline_v2::epoch_digest(
      uint64_t epoch, crypto::hash &digest) const
  {
    digest = crypto::null_hash;
    const epoch_bucket *selected = bucket(epoch);
    if (selected != nullptr && selected->digest_cache.has_value())
    {
      digest = *selected->digest_cache;
      return pipeline_status_v2::accepted;
    }
    std::string transcript;
    const pipeline_status_v2 status = epoch_transcript(epoch, transcript);
    if (status != pipeline_status_v2::accepted)
      return status;
    std::string blob("QWC_EPOSE_EPOCH_DIGEST_V2");
    append_u64_le(blob, epoch);
    blob.append(transcript);
    digest = hash_blob(blob);
    if (selected != nullptr)
      selected->digest_cache = digest;
    return pipeline_status_v2::accepted;
  }

  bool membership_pipeline_v2::fold_epoch(
      uint64_t epoch, uint64_t tip_epoch, crypto::hash &digest, undo_journal *undo)
  {
    digest = crypto::null_hash;
    if (undo == nullptr || epoch != oldest_retained_epoch_ || tip_epoch < epoch
        || oldest_retained_epoch_ == std::numeric_limits<uint64_t>::max())
      return false;
    const epoch_bucket *selected = bucket(epoch);
    uint64_t first_service_epoch = 0;
    if (!timing_.first_service_epoch(first_service_epoch)
        || (selected != nullptr && epoch >= first_service_epoch
            && !selected->qualification.has_value())
        || epoch_digest(epoch, digest) != pipeline_status_v2::accepted)
      return false;
    if (selected != nullptr)
    {
      if (selected->receipts.size() > receipt_count_)
        return false;
      if (undo != nullptr)
        undo->buckets.push_back({epoch, epochs_.at(epoch)});
      undo->archived_buckets.push_back({epoch, nullptr});
      historical_epochs_[epoch] = epochs_.at(epoch);
      receipt_count_ -= selected->receipts.size();
      epochs_.erase(epoch);
    }
    ++oldest_retained_epoch_;
    archive_tip_epoch_ = std::max(archive_tip_epoch_, tip_epoch);
    for (auto it = historical_epochs_.begin(); it != historical_epochs_.end();)
    {
      if (archived_epoch_retained(archive_tip_epoch_, it->first, timing_.epoch_length))
        break;
      undo->archived_buckets.push_back({it->first, it->second});
      it = historical_epochs_.erase(it);
    }
    for (auto it = carrier_epochs_.begin(); it != carrier_epochs_.end();)
    {
      if (archived_epoch_retained(archive_tip_epoch_, *it, timing_.epoch_length))
        break;
      undo->evicted_carriers.push_back(*it);
      it = carrier_epochs_.erase(it);
    }
    window_active_ = true;
    state_hash_cache_.reset();
    return true;
  }

  size_t membership_pipeline_v2::receipt_count() const
  {
    return receipt_count_;
  }

  size_t membership_pipeline_v2::state_bytes() const
  {
    size_t bytes = sizeof(*this);
    for (const auto &entry : epochs_)
    {
      const epoch_bucket &selected = *entry.second;
      bytes += sizeof(entry) + 3 * sizeof(void *) + sizeof(selected);
      bytes += selected.leases.capacity() * sizeof(stored_lease);
      bytes += selected.receipts.capacity() * sizeof(stored_receipt);
      if (selected.snapshot.has_value())
        bytes += selected.snapshot->members.capacity() * sizeof(frozen_member_v2);
      if (selected.qualification.has_value())
        bytes += selected.qualification->qualified_nodes.capacity()
            * sizeof(crypto::public_key);
      bytes += selected.lease_transcript.capacity();
      bytes += selected.receipt_transcript.capacity();
      for (const auto &indexed : selected.lease_by_identity)
        bytes += sizeof(indexed) + 3 * sizeof(void *) + indexed.first.capacity();
      for (const auto &indexed : selected.lease_by_service)
        bytes += sizeof(indexed) + 3 * sizeof(void *) + indexed.first.capacity();
      for (const auto &indexed : selected.receipt_by_slot)
        bytes += sizeof(indexed) + 3 * sizeof(void *)
            + std::get<1>(indexed.first).capacity()
            + std::get<2>(indexed.first).capacity();
      for (const auto &indexed : selected.receipt_coverage)
        bytes += sizeof(indexed) + 3 * sizeof(void *)
            + indexed.first.second.capacity();
    }
    return bytes;
  }

  bool membership_pipeline_v2::contains_admission(const admission_lease_v2 &lease) const
  {
    const epoch_bucket *selected = bucket(lease.target_epoch);
    if (selected == nullptr)
      return false;
    const auto found = selected->lease_by_identity.find(index_key(lease.member.identity_id));
    return found != selected->lease_by_identity.end()
        && same_lease(selected->leases[found->second].lease, lease);
  }

  bool membership_pipeline_v2::has_admission(
      const crypto::hash &identity_id, uint64_t target_epoch) const
  {
    const epoch_bucket *selected = bucket(target_epoch);
    return selected != nullptr
        && selected->lease_by_identity.count(index_key(identity_id)) != 0;
  }

  bool membership_pipeline_v2::has_receipt_slot(
      uint64_t epoch,
      uint64_t round,
      const crypto::public_key &subject_public_key,
      const crypto::public_key &verifier_public_key) const
  {
    const epoch_bucket *selected = bucket(epoch);
    return selected != nullptr
        && selected->receipt_by_slot.count(std::make_tuple(round,
            index_key(subject_public_key), index_key(verifier_public_key))) != 0;
  }

  std::vector<size_t> membership_pipeline_v2::receipt_coverage(
      const uint64_t epoch,
      const crypto::public_key &subject_public_key) const
  {
    std::vector<size_t> coverage(policy_.round_count, 0);
    const epoch_bucket *selected = bucket(epoch);
    if (selected == nullptr)
      return coverage;
    const std::string subject_key = index_key(subject_public_key);
    for (uint64_t round = 0; round < policy_.round_count; ++round)
    {
      const auto found = selected->receipt_coverage.find({round, subject_key});
      if (found != selected->receipt_coverage.end())
        coverage[round] = found->second;
    }
    return coverage;
  }

  bool membership_pipeline_v2::write_image(state_image_writer_v2 &writer) const
  {
    writer.byte(window_active_ ? 1 : 0);
    writer.number(oldest_retained_epoch_);
    writer.number(epochs_.size());
    const auto write_member = [&](const frozen_member_v2 &member) {
      writer.fixed(member.service_public_key);
      writer.fixed(member.identity_id);
      writer.fixed(member.operator_authorization_public_key);
      writer.fixed(member.descriptor_hash);
      writer.fixed(member.reward_binding_hash);
      writer.fixed(member.endpoint_descriptor_hash);
      writer.number(member.sequence);
      if (window_active_)
      {
        writer.byte(member.descriptor_version);
        writer.byte(member.endpoint_transport);
        writer.byte(member.dial_capabilities);
        writer.byte(member.dns_address_families);
      }
    };
    const auto write_bucket = [&](uint64_t epoch, const epoch_bucket &bucket) {
      membership_snapshot_v2 restored_snapshot;
      qualification_set_v2 restored_qualification;
      if ((bucket.snapshot && bucket.snapshot->pruned)
          || (bucket.qualification && bucket.qualification->pruned))
      {
        if (!reconstruct_epoch(epoch, restored_snapshot,
                bucket.qualification && bucket.qualification->pruned
                    ? &restored_qualification : nullptr))
          return false;
      }
      writer.number(epoch);
      writer.number(bucket.leases.size());
      for (const stored_lease *stored : sorted_leases(bucket))
      {
        const admission_lease_v2 &lease = stored->lease;
        write_member(lease.member);
        writer.number(lease.target_epoch);
        writer.byte(lease.work_algorithm);
        writer.byte(lease.leading_zero_bits);
        writer.number(lease.admission_context_height);
        writer.fixed(lease.admission_context_hash);
        writer.number(lease.nonce);
        writer.fixed(lease.work_hash);
        writer.fixed(lease.lease_hash);
        writer.number(stored->inclusion_height);
      }
      writer.byte(bucket.snapshot.has_value() ? 1 : 0);
      if (bucket.snapshot.has_value())
      {
        const membership_snapshot_v2 &snapshot = bucket.snapshot->pruned
            ? restored_snapshot : *bucket.snapshot;
        writer.number(snapshot.epoch);
        writer.number(snapshot.cutoff_height);
        writer.number(snapshot.anchor_height);
        writer.fixed(snapshot.anchor_hash);
        writer.number(snapshot.members.size());
        for (const frozen_member_v2 &member : snapshot.members)
          write_member(member);
        writer.fixed(snapshot.snapshot_hash);
        if (window_active_)
          writer.byte(snapshot.transport_aware ? 1 : 0);
      }
      writer.number(bucket.receipts.size());
      for (const stored_receipt *receipt : sorted_receipts(bucket))
      {
        writer.number(receipt->epoch);
        writer.number(receipt->round);
        writer.byte(receipt->service_kind);
        writer.fixed(receipt->subject_public_key);
        writer.fixed(receipt->verifier_public_key);
        writer.fixed(receipt->receipt_hash);
        writer.number(receipt->inclusion_height);
      }
      writer.byte(bucket.qualification.has_value() ? 1 : 0);
      if (bucket.qualification.has_value())
      {
        const qualification_set_v2 &qualification = bucket.qualification->pruned
            ? restored_qualification : *bucket.qualification;
        writer.number(qualification.epoch);
        writer.number(qualification.closed_height);
        writer.fixed(qualification.snapshot_hash);
        writer.number(qualification.qualified_nodes.size());
        for (const crypto::public_key &key : qualification.qualified_nodes)
          writer.fixed(key);
        writer.fixed(qualification.qualification_hash);
      }
      return true;
    };
    for (const auto &entry : epochs_)
      if (!write_bucket(entry.first, *entry.second))
        return false;
    const auto retained_epoch = [&](uint64_t epoch) {
      return archived_epoch_retained(archive_tip_epoch_, epoch,
          timing_.epoch_length);
    };
    const size_t archived_count = std::count_if(historical_epochs_.begin(),
        historical_epochs_.end(), [&](const auto &entry) {
          return retained_epoch(entry.first);
        });
    const size_t carrier_count = std::count_if(carrier_epochs_.begin(),
        carrier_epochs_.end(), retained_epoch);
    bool extended = archived_count != 0 || carrier_count != 0;
    for (const auto &entry : epochs_)
      extended = extended || (entry.second->snapshot && entry.second->snapshot->pruned)
          || (entry.second->qualification && entry.second->qualification->pruned);
    if (!extended)
      return true;
    writer.bytes("CLH1");
    writer.number(archived_count);
    for (const auto &entry : historical_epochs_)
      if (retained_epoch(entry.first)
          && !write_bucket(entry.first, *entry.second))
        return false;
    const auto write_pruned = [&](const auto &epochs, bool historical) {
      writer.number(historical ? archived_count : epochs.size());
      for (const auto &entry : epochs)
      {
        if (historical && !retained_epoch(entry.first))
          continue;
        writer.number(entry.first);
        writer.byte(entry.second->snapshot && entry.second->snapshot->pruned ? 1 : 0);
        writer.byte(entry.second->qualification && entry.second->qualification->pruned ? 1 : 0);
      }
    };
    write_pruned(epochs_, false);
    write_pruned(historical_epochs_, true);
    writer.number(carrier_count);
    for (const uint64_t epoch : carrier_epochs_)
      if (retained_epoch(epoch))
        writer.number(epoch);
    return true;
  }

  bool membership_pipeline_v2::read_image(state_image_reader_v2 &reader, uint64_t tip_epoch)
  {
    membership_pipeline_v2 parsed(nettype_, genesis_hash_, parameter_set_hash_,
        timing_, admission_policy_, policy_);
    uint8_t active = 0;
    uint64_t count = 0;
    if (!reader.byte(active) || active > 1
        || !reader.number(parsed.oldest_retained_epoch_)
        || !reader.count(count, 8 + 8 + 1 + 8 + 1))
      return false;
    parsed.window_active_ = active != 0;
    const auto read_member = [&](frozen_member_v2 &member) {
      const bool base = reader.fixed(member.service_public_key)
          && reader.fixed(member.identity_id)
          && reader.fixed(member.operator_authorization_public_key)
          && reader.fixed(member.descriptor_hash)
          && reader.fixed(member.reward_binding_hash)
          && reader.fixed(member.endpoint_descriptor_hash)
          && reader.number(member.sequence);
      return base && (!parsed.window_active_
          || (reader.byte(member.descriptor_version)
              && reader.byte(member.endpoint_transport)
              && reader.byte(member.dial_capabilities)
              && reader.byte(member.dns_address_families)));
    };
    const auto read_bucket = [&](uint64_t &epoch,
        std::shared_ptr<epoch_bucket> &bucket, bool active_bucket) {
      uint64_t leases = 0;
      if (!reader.number(epoch)
          || !reader.count(leases, 322 + 8))
        return false;
      bucket = std::make_shared<epoch_bucket>();
      for (uint64_t lease_index = 0; lease_index < leases; ++lease_index)
      {
        stored_lease stored{};
        admission_lease_v2 &lease = stored.lease;
        if (!read_member(lease.member)
            || !reader.number(lease.target_epoch)
            || !reader.byte(lease.work_algorithm)
            || !reader.byte(lease.leading_zero_bits)
            || !reader.number(lease.admission_context_height)
            || !reader.fixed(lease.admission_context_hash)
            || !reader.number(lease.nonce)
            || !reader.fixed(lease.work_hash)
            || !reader.fixed(lease.lease_hash)
            || !reader.number(stored.inclusion_height)
            || lease.target_epoch != epoch)
          return false;
        const size_t position = bucket->leases.size();
        if (!bucket->lease_by_identity.emplace(index_key(lease.member.identity_id), position).second
            || !bucket->lease_by_service.emplace(index_key(lease.member.service_public_key), position).second)
          return false;
        bucket->leases.push_back(std::move(stored));
      }
      uint8_t has_snapshot = 0;
      if (!reader.byte(has_snapshot) || has_snapshot > 1)
        return false;
      if (has_snapshot)
      {
        membership_snapshot_v2 snapshot{};
        uint64_t members = 0;
        if (!reader.number(snapshot.epoch)
            || !reader.number(snapshot.cutoff_height)
            || !reader.number(snapshot.anchor_height)
            || !reader.fixed(snapshot.anchor_hash)
            || !reader.count(members, 6 * sizeof(crypto::hash) + 8)
            || snapshot.epoch != epoch)
          return false;
        for (uint64_t member_index = 0; member_index < members; ++member_index)
        {
          frozen_member_v2 member{};
          if (!read_member(member))
            return false;
          snapshot.members.push_back(member);
        }
        uint8_t transport_aware = 0;
        if (!reader.fixed(snapshot.snapshot_hash)
            || (parsed.window_active_
                && (!reader.byte(transport_aware) || transport_aware > 1)))
          return false;
        snapshot.transport_aware = transport_aware != 0;
        if (snapshot.snapshot_hash != parsed.snapshot_hash(snapshot))
          return false;
        bucket->snapshot = std::move(snapshot);
      }
      uint64_t receipts = 0;
      if (!reader.count(receipts, 8 + 8 + 1 + 3 * sizeof(crypto::hash) + 8)
          || (active_bucket && receipts > std::numeric_limits<size_t>::max() - parsed.receipt_count_))
        return false;
      for (uint64_t receipt_index = 0; receipt_index < receipts; ++receipt_index)
      {
        stored_receipt receipt{};
        if (!reader.number(receipt.epoch)
            || !reader.number(receipt.round)
            || !reader.byte(receipt.service_kind)
            || !reader.fixed(receipt.subject_public_key)
            || !reader.fixed(receipt.verifier_public_key)
            || !reader.fixed(receipt.receipt_hash)
            || !reader.number(receipt.inclusion_height)
            || receipt.epoch != epoch)
          return false;
        const auto key = std::make_tuple(receipt.round,
            index_key(receipt.subject_public_key),
            index_key(receipt.verifier_public_key));
        if (!bucket->receipt_by_slot.emplace(key, bucket->receipts.size()).second)
          return false;
        ++bucket->receipt_coverage[{receipt.round,
            index_key(receipt.subject_public_key)}];
        bucket->receipts.push_back(receipt);
      }
      if (active_bucket)
        parsed.receipt_count_ += static_cast<size_t>(receipts);
      uint8_t has_qualification = 0;
      if (!reader.byte(has_qualification) || has_qualification > 1)
        return false;
      if (has_qualification)
      {
        qualification_set_v2 qualification{};
        uint64_t qualified = 0;
        if (!reader.number(qualification.epoch)
            || !reader.number(qualification.closed_height)
            || !reader.fixed(qualification.snapshot_hash)
            || !reader.count(qualified, sizeof(crypto::public_key))
            || qualification.epoch != epoch)
          return false;
        for (uint64_t member_index = 0; member_index < qualified; ++member_index)
        {
          crypto::public_key key{};
          if (!reader.fixed(key))
            return false;
          qualification.qualified_nodes.push_back(key);
        }
        if (!reader.fixed(qualification.qualification_hash)
            || !bucket->snapshot.has_value()
            || qualification.snapshot_hash != bucket->snapshot->snapshot_hash
            || qualification.qualification_hash != parsed.qualification_hash(qualification))
          return false;
        bucket->qualification = std::move(qualification);
      }
      if (bucket->snapshot)
      {
        for (const frozen_member_v2 &member : bucket->snapshot->members)
        {
          const auto found = std::find_if(bucket->leases.begin(), bucket->leases.end(),
              [&](const stored_lease &lease) { return same_member(lease.lease.member, member); });
          if (found == bucket->leases.end())
            return false;
          bucket->snapshot_lease_hashes.push_back(found->lease.lease_hash);
        }
      }
      return true;
    };
    uint64_t previous_epoch = 0;
    for (uint64_t index = 0; index < count; ++index)
    {
      uint64_t epoch = 0;
      std::shared_ptr<epoch_bucket> bucket;
      if (!read_bucket(epoch, bucket, true)
          || (index != 0 && epoch <= previous_epoch)
          || (parsed.window_active_ && epoch < parsed.oldest_retained_epoch_))
        return false;
      previous_epoch = epoch;
      parsed.epochs_.emplace(epoch, std::move(bucket));
    }
    if (reader.remaining() != 0)
    {
      std::string marker;
      uint64_t historical_count = 0;
      if (!reader.bytes(marker) || marker != "CLH1"
          || !reader.count(historical_count, 8 + 8 + 1 + 8 + 1))
        return false;
      previous_epoch = 0;
      for (uint64_t index = 0; index < historical_count; ++index)
      {
        uint64_t epoch = 0;
        std::shared_ptr<epoch_bucket> bucket;
        if (!read_bucket(epoch, bucket, false)
            || (index != 0 && epoch <= previous_epoch)
            || epoch >= parsed.oldest_retained_epoch_
            || parsed.epochs_.count(epoch) != 0)
          return false;
        previous_epoch = epoch;
        parsed.historical_epochs_.emplace(epoch, std::move(bucket));
      }
      const auto read_pruned = [&](auto &epochs) {
        uint64_t flags_count = 0;
        if (!reader.number(flags_count) || flags_count != epochs.size())
          return false;
        for (auto &entry : epochs)
        {
          uint64_t epoch = 0;
          uint8_t snapshot_pruned = 0;
          uint8_t qualification_pruned = 0;
          if (!reader.number(epoch) || epoch != entry.first
              || !reader.byte(snapshot_pruned) || snapshot_pruned > 1
              || !reader.byte(qualification_pruned) || qualification_pruned > 1
              || (snapshot_pruned && !entry.second->snapshot)
              || (qualification_pruned && !entry.second->qualification))
            return false;
          if (snapshot_pruned)
          {
            entry.second->snapshot->members.clear();
            entry.second->snapshot->pruned = true;
          }
          if (qualification_pruned)
          {
            entry.second->qualification->qualified_nodes.clear();
            entry.second->qualification->pruned = true;
          }
        }
        return true;
      };
      uint64_t carriers = 0;
      if (!read_pruned(parsed.epochs_) || !read_pruned(parsed.historical_epochs_)
          || !reader.count(carriers, 8))
        return false;
      previous_epoch = 0;
      for (uint64_t index = 0; index < carriers; ++index)
      {
        uint64_t epoch = 0;
        if (!reader.number(epoch) || epoch == 0
            || (index != 0 && epoch <= previous_epoch))
          return false;
        previous_epoch = epoch;
        parsed.carrier_epochs_.insert(epoch);
      }
    }
    parsed.archive_tip_epoch_ = std::max(tip_epoch, parsed.oldest_retained_epoch_);
    for (const auto &entry : parsed.historical_epochs_)
      if (!archived_epoch_retained(parsed.archive_tip_epoch_, entry.first,
              parsed.timing_.epoch_length))
        return false;
    for (const uint64_t epoch : parsed.carrier_epochs_)
      if (!archived_epoch_retained(parsed.archive_tip_epoch_, epoch,
              parsed.timing_.epoch_length))
        return false;
    *this = std::move(parsed);
    return true;
  }

  crypto::hash membership_pipeline_v2::state_hash() const
  {
    if (state_hash_cache_.has_value())
      return *state_hash_cache_;
    std::string blob("QWC_EPOSE_PIPELINE_STATE_V2");
    append_network(blob, nettype_);
    append_bytes(blob, genesis_hash_);
    append_bytes(blob, parameter_set_hash_);
    append_u64_le(blob, timing_.activation_height);
    append_u64_le(blob, timing_.epoch_length);
    append_u64_le(blob, timing_.anchor_depth);
    append_u64_le(blob, policy_.committee_size);
    append_u64_le(blob, policy_.threshold);
    append_u64_le(blob, policy_.round_count);
    append_u64_le(blob, policy_.rounds_required);
    if (window_active_)
      append_u64_le(blob, policy_.min_compatible_committee);
    append_u8(blob, policy_.service_kind);
    append_u64_le(blob, policy_.round_offsets.size());
    for (const uint64_t offset : policy_.round_offsets)
      append_u64_le(blob, offset);

    size_t lease_count = 0;
    size_t snapshot_count = 0;
    size_t qualification_count = 0;
    for (const auto &entry : epochs_)
    {
      const epoch_bucket &selected = *entry.second;
      lease_count += selected.leases.size();
      snapshot_count += selected.snapshot.has_value();
      qualification_count += selected.qualification.has_value();
    }

    append_u64_le(blob, lease_count);
    for (const auto &entry : epochs_)
      blob.append(lease_transcript(*entry.second));
    append_u64_le(blob, snapshot_count);
    for (const auto &entry : epochs_)
      if (entry.second->snapshot.has_value())
        append_bytes(blob, entry.second->snapshot->snapshot_hash);
    append_u64_le(blob, receipt_count_);
    for (const auto &entry : epochs_)
      blob.append(receipt_transcript(*entry.second));
    append_u64_le(blob, qualification_count);
    for (const auto &entry : epochs_)
      if (entry.second->qualification.has_value())
        append_bytes(blob, entry.second->qualification->qualification_hash);
    state_hash_cache_ = hash_blob(blob);
    return *state_hash_cache_;
  }
} // namespace epose
} // namespace qwertycoin
