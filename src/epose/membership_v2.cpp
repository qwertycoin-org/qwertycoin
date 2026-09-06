// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/membership_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/service_receipt_v2.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <string>

namespace
{
  using qwertycoin::epose::admission_lease_v2;
  using qwertycoin::epose::frozen_member_v2;

  bool checked_add(uint64_t left, uint64_t right, uint64_t &out)
  {
    if (left > std::numeric_limits<uint64_t>::max() - right)
      return false;
    out = left + right;
    return true;
  }

  bool checked_mul(uint64_t left, uint64_t right, uint64_t &out)
  {
    if (left != 0 && right > std::numeric_limits<uint64_t>::max() / left)
      return false;
    out = left * right;
    return true;
  }

  template <typename T>
  bool bytes_equal(const T &left, const T &right)
  {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
  }

  template <typename T>
  bool bytes_less(const T &left, const T &right)
  {
    return std::memcmp(&left, &right, sizeof(T)) < 0;
  }

  void append_u8(std::string &out, uint8_t value)
  {
    out.push_back(static_cast<char>(value));
  }

  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
  }

  void append_network(std::string &out, cryptonote::network_type nettype)
  {
    const auto &network_id = cryptonote::get_config(nettype).NETWORK_ID;
    for (const auto byte : network_id)
      append_u8(out, byte);
  }

  crypto::hash hash_blob(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
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
        && left.sequence == right.sequence;
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
        && threshold <= committee_size
        && round_count > 0
        && rounds_required > 0
        && rounds_required <= round_count
        && service_kind != 0
        && round_offsets.size() == round_count
        && !round_offsets.empty()
        && round_offsets.front() == 0
        && std::adjacent_find(round_offsets.begin(), round_offsets.end(),
            [](uint64_t left, uint64_t right) { return left >= right; }) == round_offsets.end();
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

  bool membership_pipeline_v2::valid() const
  {
    return valid_;
  }

  pipeline_status_v2 membership_pipeline_v2::apply_admission(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context,
      uint64_t inclusion_height,
      verification_counters_v2 *counters)
  {
    if (!valid_)
      return pipeline_status_v2::invalid_configuration;
    if (context.nettype != nettype_ || context.genesis_hash != genesis_hash_
        || context.parameter_set_hash != parameter_set_hash_
        || context.height >= inclusion_height)
      return pipeline_status_v2::invalid_member;
    if (inclusion_height < timing_.activation_height)
      return pipeline_status_v2::before_activation;
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
    if (snapshots_.count(lease.target_epoch) != 0)
      return pipeline_status_v2::too_late;
    if (!validate_admission_lease_v2(lease, context, admission_policy_, counters))
      return pipeline_status_v2::invalid_member;

    for (const stored_lease &stored : leases_)
    {
      if (stored.lease.target_epoch == lease.target_epoch
          && stored.lease.member.identity_id != lease.member.identity_id
          && bytes_equal(
              stored.lease.member.service_public_key,
              lease.member.service_public_key))
        return pipeline_status_v2::conflicting_record;
    }

    for (stored_lease &stored : leases_)
    {
      if (stored.lease.target_epoch != lease.target_epoch
          || stored.lease.member.identity_id != lease.member.identity_id)
        continue;
      if (same_lease(stored.lease, lease))
        return pipeline_status_v2::idempotent_duplicate;
      if (lease.member.sequence > stored.lease.member.sequence)
      {
        stored = {lease, inclusion_height};
        return pipeline_status_v2::accepted;
      }
      return pipeline_status_v2::conflicting_record;
    }

    leases_.push_back({lease, inclusion_height});
    return pipeline_status_v2::accepted;
  }

  pipeline_status_v2 membership_pipeline_v2::freeze_membership(
      uint64_t epoch,
      uint64_t height,
      const crypto::hash &anchor_hash)
  {
    return freeze_membership_impl(epoch, height, anchor_hash, nullptr);
  }

  pipeline_status_v2 membership_pipeline_v2::freeze_membership(
      uint64_t epoch,
      uint64_t height,
      const crypto::hash &anchor_hash,
      const std::vector<frozen_member_v2> &authorized_members)
  {
    return freeze_membership_impl(epoch, height, anchor_hash, &authorized_members);
  }

  pipeline_status_v2 membership_pipeline_v2::freeze_membership_impl(
      uint64_t epoch,
      uint64_t height,
      const crypto::hash &anchor_hash,
      const std::vector<frozen_member_v2> *authorized_members)
  {
    if (!valid_)
      return pipeline_status_v2::invalid_configuration;
    if (snapshots_.count(epoch) != 0)
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
    for (const stored_lease &stored : leases_)
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
    }
    std::sort(frozen.members.begin(), frozen.members.end(), member_less);

    std::string blob("QWC_EPOSE_SNAPSHOT_V2");
    append_network(blob, nettype_);
    append_bytes(blob, genesis_hash_);
    append_bytes(blob, parameter_set_hash_);
    append_u64_le(blob, frozen.epoch);
    append_u64_le(blob, frozen.cutoff_height);
    append_u64_le(blob, frozen.anchor_height);
    append_bytes(blob, frozen.anchor_hash);
    append_u64_le(blob, frozen.members.size());
    for (const frozen_member_v2 &member : frozen.members)
      append_member(blob, member);
    frozen.snapshot_hash = hash_blob(blob);
    snapshots_.emplace(epoch, std::move(frozen));
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
    if (!valid_ || frozen == nullptr || round >= policy_.round_count
        || round_anchor_hash == crypto::null_hash
        || (round == 0 && round_anchor_hash != frozen->anchor_hash)
        || !contains_member(*frozen, subject_public_key))
      return result;
    if (frozen->members.size() <= policy_.committee_size)
      return result;

    for (const frozen_member_v2 &candidate : frozen->members)
    {
      if (bytes_equal(candidate.service_public_key, subject_public_key))
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
    if (challenge.service_kind != policy_.service_kind)
      return pipeline_status_v2::invalid_service_kind;
    if (qualifications_.count(challenge.epoch) != 0)
      return pipeline_status_v2::qualification_already_closed;
    const membership_snapshot_v2 *frozen = snapshot(challenge.epoch);
    if (frozen == nullptr)
      return pipeline_status_v2::snapshot_missing;
    if (challenge.snapshot_hash != frozen->snapshot_hash
        || canonical_round_anchor_hash == crypto::null_hash
        || challenge.anchor_hash != canonical_round_anchor_hash
        || (challenge.round == 0 && challenge.anchor_hash != frozen->anchor_hash))
      return pipeline_status_v2::receipt_not_prevalidated;
    uint64_t start = 0;
    uint64_t deadline = 0;
    if (!timing_.epoch_start(challenge.epoch, start)
        || !timing_.evidence_deadline(challenge.epoch, deadline)
        || challenge.round >= policy_.round_count)
      return pipeline_status_v2::invalid_epoch;
    uint64_t round_start = 0;
    if (!checked_add(start, policy_.round_offsets[challenge.round], round_start))
      return pipeline_status_v2::invalid_epoch;
    uint64_t first_inclusion = round_start;
    // Round zero is anchored before the service epoch. Later rounds use a
    // block inside the epoch as their unpredictable anchor, so a receipt
    // cannot be included in that same block.
    if (challenge.round != 0
        && !checked_add(round_start, 1, first_inclusion))
      return pipeline_status_v2::invalid_epoch;
    uint64_t round_end = deadline;
    if (challenge.round + 1 < policy_.round_count)
    {
      uint64_t next_round_start = 0;
      if (!checked_add(start, policy_.round_offsets[challenge.round + 1], next_round_start)
          || next_round_start == 0)
        return pipeline_status_v2::invalid_epoch;
      round_end = next_round_start - 1;
    }
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

    const crypto::hash receipt_hash = hash_authenticated_service_receipt_v2(receipt, context);
    for (const stored_receipt &stored : receipts_)
    {
      if (stored.epoch != challenge.epoch || stored.round != challenge.round
          || stored.service_kind != challenge.service_kind
          || !bytes_equal(stored.subject_public_key, challenge.subject_public_key)
          || !bytes_equal(stored.verifier_public_key, challenge.verifier_public_key))
        continue;
      if (!bytes_equal(stored.receipt_hash, receipt_hash))
        return pipeline_status_v2::receipt_slot_conflict;
      // receipt_hash is the verifier's signing message. It intentionally does
      // not contain verifier_signature, so an equal hash alone cannot make a
      // wire record an authenticated idempotent duplicate.
      if (!validate_authenticated_service_receipt_v2(receipt, context, counters))
        return pipeline_status_v2::receipt_not_prevalidated;
      return pipeline_status_v2::idempotent_duplicate;
    }
    if (!validate_authenticated_service_receipt_v2(receipt, context, counters))
      return pipeline_status_v2::receipt_not_prevalidated;
    receipts_.push_back({challenge.epoch, challenge.round, challenge.service_kind,
        challenge.subject_public_key, challenge.verifier_public_key, receipt_hash, inclusion_height});
    return pipeline_status_v2::accepted;
  }

  pipeline_status_v2 membership_pipeline_v2::close_qualification(uint64_t epoch, uint64_t height)
  {
    if (!valid_)
      return pipeline_status_v2::invalid_configuration;
    if (qualifications_.count(epoch) != 0)
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

    if (frozen->members.size() > policy_.committee_size)
    {
      for (const frozen_member_v2 &subject : frozen->members)
      {
        uint64_t passing_rounds = 0;
        for (uint64_t round = 0; round < policy_.round_count; ++round)
        {
          std::set<std::string> voters;
          for (const stored_receipt &stored : receipts_)
          {
            if (stored.epoch == epoch
                && stored.round == round
                && bytes_equal(stored.subject_public_key, subject.service_public_key))
              voters.emplace(reinterpret_cast<const char *>(&stored.verifier_public_key), sizeof(stored.verifier_public_key));
          }
          if (voters.size() >= policy_.threshold)
            ++passing_rounds;
        }
        if (passing_rounds >= policy_.rounds_required)
          closed.qualified_nodes.push_back(subject.service_public_key);
      }
    }

    std::string blob("QWC_EPOSE_QUALIFICATION_V2");
    append_network(blob, nettype_);
    append_bytes(blob, genesis_hash_);
    append_bytes(blob, parameter_set_hash_);
    append_bytes(blob, closed.snapshot_hash);
    append_u64_le(blob, closed.epoch);
    append_u64_le(blob, closed.closed_height);
    append_u64_le(blob, closed.qualified_nodes.size());
    for (const crypto::public_key &key : closed.qualified_nodes)
      append_bytes(blob, key);
    closed.qualification_hash = hash_blob(blob);
    qualifications_.emplace(epoch, std::move(closed));
    return pipeline_status_v2::accepted;
  }

  const membership_snapshot_v2 *membership_pipeline_v2::snapshot(uint64_t epoch) const
  {
    const auto found = snapshots_.find(epoch);
    return found == snapshots_.end() ? nullptr : &found->second;
  }

  const qualification_set_v2 *membership_pipeline_v2::qualification(uint64_t epoch) const
  {
    const auto found = qualifications_.find(epoch);
    return found == qualifications_.end() ? nullptr : &found->second;
  }

  crypto::hash membership_pipeline_v2::state_hash() const
  {
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
    append_u8(blob, policy_.service_kind);
    append_u64_le(blob, policy_.round_offsets.size());
    for (const uint64_t offset : policy_.round_offsets)
      append_u64_le(blob, offset);
    std::vector<stored_lease> leases = leases_;
    std::sort(leases.begin(), leases.end(), [](const stored_lease &left, const stored_lease &right) {
      if (left.lease.target_epoch != right.lease.target_epoch)
        return left.lease.target_epoch < right.lease.target_epoch;
      return member_less(left.lease.member, right.lease.member);
    });
    append_u64_le(blob, leases.size());
    for (const stored_lease &stored : leases)
    {
      append_u64_le(blob, stored.lease.target_epoch);
      append_member(blob, stored.lease.member);
      append_bytes(blob, stored.lease.lease_hash);
      append_u64_le(blob, stored.inclusion_height);
    }
    append_u64_le(blob, snapshots_.size());
    for (const auto &entry : snapshots_)
      append_bytes(blob, entry.second.snapshot_hash);
    std::vector<stored_receipt> receipts = receipts_;
    std::sort(receipts.begin(), receipts.end(), [](const stored_receipt &left, const stored_receipt &right) {
      const auto &a = left;
      const auto &b = right;
      if (a.epoch != b.epoch)
        return a.epoch < b.epoch;
      if (a.round != b.round)
        return a.round < b.round;
      if (bytes_less(a.subject_public_key, b.subject_public_key))
        return true;
      if (bytes_less(b.subject_public_key, a.subject_public_key))
        return false;
      return bytes_less(a.verifier_public_key, b.verifier_public_key);
    });
    append_u64_le(blob, receipts.size());
    for (const stored_receipt &stored : receipts)
    {
      append_u64_le(blob, stored.epoch);
      append_u64_le(blob, stored.round);
      append_u8(blob, stored.service_kind);
      append_bytes(blob, stored.subject_public_key);
      append_bytes(blob, stored.verifier_public_key);
      append_bytes(blob, stored.receipt_hash);
      append_u64_le(blob, stored.inclusion_height);
    }
    append_u64_le(blob, qualifications_.size());
    for (const auto &entry : qualifications_)
      append_bytes(blob, entry.second.qualification_hash);
    return hash_blob(blob);
  }
} // namespace epose
} // namespace qwertycoin
