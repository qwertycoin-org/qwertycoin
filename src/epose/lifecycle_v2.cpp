// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/lifecycle_v2.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace
{
  using qwertycoin::epose::identity_descriptor_v2;
  using qwertycoin::epose::lifecycle_action_v2;
  using qwertycoin::epose::lifecycle_record_v2;

  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
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

  void append_network(std::string &out, cryptonote::network_type nettype)
  {
    const auto &network_id = cryptonote::get_config(nettype).NETWORK_ID;
    for (const auto byte : network_id)
      append_u8(out, byte);
  }

  crypto::hash fast_hash(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  bool hash_equal(const crypto::hash &left, const crypto::hash &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
  }

  bool hash_less(const crypto::hash &left, const crypto::hash &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  }

  bool valid_key(const crypto::public_key &key)
  {
    return key != crypto::null_pkey && crypto::check_key(key);
  }

  bool valid_action(lifecycle_action_v2 action)
  {
    return action >= lifecycle_action_v2::register_identity
        && action <= lifecycle_action_v2::recover_service_key;
  }

  bool descriptor_shape_valid(const identity_descriptor_v2 &descriptor)
  {
    return descriptor.version == qwertycoin::epose::EPOSE_DESCRIPTOR_VERSION_V2
        && descriptor.identity_id != crypto::null_hash
        && valid_key(descriptor.service_public_key)
        && valid_key(descriptor.operator_authorization_public_key)
        && valid_key(descriptor.reward_address.m_view_public_key)
        && valid_key(descriptor.reward_address.m_spend_public_key)
        && descriptor.endpoint_descriptor_hash != crypto::null_hash
        && descriptor.expiry_epoch >= descriptor.effective_epoch;
  }

  bool descriptor_static_equal(const identity_descriptor_v2 &left, const identity_descriptor_v2 &right)
  {
    return left.identity_id == right.identity_id
        && left.service_public_key == right.service_public_key
        && left.operator_authorization_public_key == right.operator_authorization_public_key
        && left.reward_address.m_view_public_key == right.reward_address.m_view_public_key
        && left.reward_address.m_spend_public_key == right.reward_address.m_spend_public_key
        && left.endpoint_descriptor_hash == right.endpoint_descriptor_hash;
  }

  bool secret_matches(const crypto::secret_key &secret, const crypto::public_key &expected)
  {
    crypto::public_key actual{};
    return crypto::secret_key_to_public_key(secret, actual) && actual == expected;
  }
}

namespace qwertycoin
{
namespace epose
{
  crypto::hash derive_identity_id_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const crypto::public_key &operator_authorization_public_key)
  {
    std::string blob("QWC_EPOSE_IDENTITY_V2");
    append_network(blob, nettype);
    append_bytes(blob, genesis_hash);
    append_bytes(blob, parameter_set_hash);
    append_bytes(blob, operator_authorization_public_key);
    return fast_hash(blob);
  }

  crypto::hash hash_identity_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const identity_descriptor_v2 &descriptor)
  {
    std::string blob("QWC_EPOSE_DESCRIPTOR_V2");
    append_network(blob, nettype);
    append_bytes(blob, genesis_hash);
    append_bytes(blob, parameter_set_hash);
    append_u8(blob, descriptor.version);
    append_bytes(blob, descriptor.identity_id);
    append_bytes(blob, descriptor.service_public_key);
    append_bytes(blob, descriptor.operator_authorization_public_key);
    append_bytes(blob, descriptor.reward_address.m_view_public_key);
    append_bytes(blob, descriptor.reward_address.m_spend_public_key);
    append_bytes(blob, descriptor.endpoint_descriptor_hash);
    append_u64_le(blob, descriptor.sequence);
    append_u64_le(blob, descriptor.effective_epoch);
    append_u64_le(blob, descriptor.expiry_epoch);
    return fast_hash(blob);
  }

  crypto::hash hash_reward_binding_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const cryptonote::account_public_address &reward_address)
  {
    if (nettype == cryptonote::UNDEFINED || genesis_hash == crypto::null_hash
        || parameter_set_hash == crypto::null_hash
        || !valid_key(reward_address.m_view_public_key)
        || !valid_key(reward_address.m_spend_public_key))
      return crypto::null_hash;
    std::string blob("QWC_EPOSE_REWARD_BINDING_V2");
    append_network(blob, nettype);
    append_bytes(blob, genesis_hash);
    append_bytes(blob, parameter_set_hash);
    append_bytes(blob, reward_address.m_view_public_key);
    append_bytes(blob, reward_address.m_spend_public_key);
    return fast_hash(blob);
  }

  crypto::hash hash_lifecycle_record_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const lifecycle_record_v2 &record)
  {
    std::string blob("QWC_EPOSE_LIFECYCLE_V2");
    append_network(blob, nettype);
    append_bytes(blob, genesis_hash);
    append_bytes(blob, parameter_set_hash);
    append_u8(blob, static_cast<uint8_t>(record.action));
    append_bytes(blob, record.previous_descriptor_hash);
    append_bytes(blob, hash_identity_descriptor_v2(nettype, genesis_hash, parameter_set_hash, record.next_descriptor));
    return fast_hash(blob);
  }

  bool sign_lifecycle_record_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      lifecycle_record_v2 &record,
      const crypto::secret_key &operator_authorization_secret_key,
      const crypto::secret_key &service_secret_key)
  {
    if (!secret_matches(operator_authorization_secret_key, record.next_descriptor.operator_authorization_public_key)
        || !secret_matches(service_secret_key, record.next_descriptor.service_public_key))
      return false;
    const crypto::hash message = hash_lifecycle_record_v2(nettype, genesis_hash, parameter_set_hash, record);
    crypto::generate_signature(message, record.next_descriptor.operator_authorization_public_key,
        operator_authorization_secret_key, record.operator_signature);
    crypto::generate_signature(message, record.next_descriptor.service_public_key,
        service_secret_key, record.service_signature);
    return true;
  }

  lifecycle_status_v2 validate_lifecycle_record_structure_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const lifecycle_record_v2 &record)
  {
    if (nettype == cryptonote::UNDEFINED || genesis_hash == crypto::null_hash
        || parameter_set_hash == crypto::null_hash)
      return lifecycle_status_v2::invalid_context;
    if (!valid_action(record.action))
      return lifecycle_status_v2::invalid_action;
    const identity_descriptor_v2 &next = record.next_descriptor;
    if (!descriptor_shape_valid(next)
        || next.identity_id != derive_identity_id_v2(
            nettype, genesis_hash, parameter_set_hash, next.operator_authorization_public_key))
      return lifecycle_status_v2::invalid_descriptor;
    if (next.service_public_key == next.operator_authorization_public_key)
      return lifecycle_status_v2::nonseparated_authorities;
    return lifecycle_status_v2::accepted;
  }

  lifecycle_status_v2 validate_lifecycle_record_authorization_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const lifecycle_record_v2 &record,
      verification_counters_v2 *counters)
  {
    const lifecycle_status_v2 structure = validate_lifecycle_record_structure_v2(
        nettype, genesis_hash, parameter_set_hash, record);
    if (structure != lifecycle_status_v2::accepted)
      return structure;
    const identity_descriptor_v2 &next = record.next_descriptor;
    const crypto::hash message = hash_lifecycle_record_v2(
        nettype, genesis_hash, parameter_set_hash, record);
    if (counters != nullptr)
      ++counters->signatures;
    if (!crypto::check_signature(message, next.operator_authorization_public_key, record.operator_signature))
      return lifecycle_status_v2::invalid_operator_signature;
    if (counters != nullptr)
      ++counters->signatures;
    if (!crypto::check_signature(message, next.service_public_key, record.service_signature))
      return lifecycle_status_v2::invalid_service_signature;
    return lifecycle_status_v2::accepted;
  }

  lifecycle_registry_v2::lifecycle_registry_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash)
    : nettype_(nettype), genesis_hash_(genesis_hash), parameter_set_hash_(parameter_set_hash)
  {
    valid_ = nettype != cryptonote::UNDEFINED
        && genesis_hash != crypto::null_hash
        && parameter_set_hash != crypto::null_hash;
  }

  bool lifecycle_registry_v2::valid() const
  {
    return valid_;
  }

  lifecycle_status_v2 lifecycle_registry_v2::apply(
      const lifecycle_record_v2 &record,
      uint64_t inclusion_epoch,
      uint64_t minimum_effective_epoch,
      verification_counters_v2 *counters)
  {
    if (!valid_)
      return lifecycle_status_v2::invalid_context;
    const lifecycle_status_v2 structure = validate_lifecycle_record_structure_v2(
        nettype_, genesis_hash_, parameter_set_hash_, record);
    if (structure != lifecycle_status_v2::accepted)
      return structure;
    const identity_descriptor_v2 &next = record.next_descriptor;
    if (next.effective_epoch < minimum_effective_epoch || next.effective_epoch < inclusion_epoch)
      return lifecycle_status_v2::retroactive_change;

    const crypto::hash message = hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, record);

    auto history_it = std::find_if(histories_.begin(), histories_.end(), [&](const identity_history &history) {
      return hash_equal(history.identity_id, next.identity_id);
    });
    bool idempotent_duplicate = false;
    if (history_it != histories_.end())
    {
      const auto duplicate = std::find_if(history_it->records.begin(), history_it->records.end(), [&](const lifecycle_record_v2 &stored) {
        return message == hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, stored);
      });
      idempotent_duplicate = duplicate != history_it->records.end();
    }
    const bool registering = record.action == lifecycle_action_v2::register_identity;
    if (!idempotent_duplicate && registering)
    {
      if (history_it != histories_.end())
        return lifecycle_status_v2::identity_already_exists;
      if (record.previous_descriptor_hash != crypto::null_hash || next.sequence != 0)
        return lifecycle_status_v2::invalid_transition;
    }
    else if (!idempotent_duplicate)
    {
      if (history_it == histories_.end())
        return lifecycle_status_v2::identity_not_found;
      const identity_descriptor_v2 &current = history_it->records.back().next_descriptor;
      const crypto::hash current_hash = hash_identity_descriptor_v2(nettype_, genesis_hash_, parameter_set_hash_, current);
      if (record.previous_descriptor_hash != current_hash)
        return lifecycle_status_v2::wrong_previous_descriptor;
      if (current.sequence == std::numeric_limits<uint64_t>::max() || next.sequence != current.sequence + 1)
        return lifecycle_status_v2::wrong_sequence;
      if (next.effective_epoch <= current.effective_epoch)
        return lifecycle_status_v2::overlapping_change;
      if (next.operator_authorization_public_key != current.operator_authorization_public_key)
        return lifecycle_status_v2::invalid_transition;
      if (history_it->records.back().action == lifecycle_action_v2::deregister_identity)
        return lifecycle_status_v2::invalid_transition;

      switch (record.action)
      {
        case lifecycle_action_v2::renew_lease:
          if (!descriptor_static_equal(next, current) || next.expiry_epoch <= current.expiry_epoch)
            return lifecycle_status_v2::invalid_transition;
          break;
        case lifecycle_action_v2::update_descriptor:
          if (next.service_public_key != current.service_public_key)
            return lifecycle_status_v2::invalid_transition;
          break;
        case lifecycle_action_v2::recover_service_key:
          if (next.service_public_key == current.service_public_key)
            return lifecycle_status_v2::invalid_transition;
          break;
        case lifecycle_action_v2::deregister_identity:
          if (!descriptor_static_equal(next, current) || next.expiry_epoch != next.effective_epoch)
            return lifecycle_status_v2::invalid_transition;
          break;
        case lifecycle_action_v2::register_identity:
          return lifecycle_status_v2::invalid_transition;
      }
    }

    if (!idempotent_duplicate && record.action != lifecycle_action_v2::deregister_identity)
    {
      const uint64_t proposed_start = next.effective_epoch;
      const uint64_t proposed_end = next.expiry_epoch;
      for (const identity_history &history : histories_)
      {
        if (hash_equal(history.identity_id, next.identity_id))
          continue;
        for (size_t index = 0; index < history.records.size(); ++index)
        {
          const lifecycle_record_v2 &existing_record = history.records[index];
          if (existing_record.action == lifecycle_action_v2::deregister_identity)
            continue;
          const identity_descriptor_v2 &existing = existing_record.next_descriptor;
          if (existing.service_public_key != next.service_public_key)
            continue;
          uint64_t existing_end = existing.expiry_epoch;
          if (index + 1 < history.records.size())
          {
            const uint64_t next_start = history.records[index + 1].next_descriptor.effective_epoch;
            if (next_start == 0)
              return lifecycle_status_v2::invalid_transition;
            existing_end = std::min(existing_end, next_start - 1);
          }
          if (proposed_start <= existing_end && existing.effective_epoch <= proposed_end)
            return lifecycle_status_v2::service_key_in_use;
        }
      }
    }

    const lifecycle_status_v2 authorization = validate_lifecycle_record_authorization_v2(
        nettype_, genesis_hash_, parameter_set_hash_, record, counters);
    if (authorization != lifecycle_status_v2::accepted)
      return authorization;
    if (idempotent_duplicate)
      return lifecycle_status_v2::idempotent_duplicate;

    if (registering)
    {
      identity_history history{};
      history.identity_id = next.identity_id;
      history.records.push_back(record);
      histories_.push_back(std::move(history));
    }
    else
      history_it->records.push_back(record);
    return lifecycle_status_v2::accepted;
  }

  const identity_descriptor_v2 *lifecycle_registry_v2::latest(const crypto::hash &identity_id) const
  {
    const auto found = std::find_if(histories_.begin(), histories_.end(), [&](const identity_history &history) {
      return hash_equal(history.identity_id, identity_id);
    });
    return found == histories_.end() || found->records.empty() ? nullptr : &found->records.back().next_descriptor;
  }

  const identity_descriptor_v2 *lifecycle_registry_v2::descriptor_for_epoch(
      const crypto::hash &identity_id,
      uint64_t epoch) const
  {
    const auto found = std::find_if(histories_.begin(), histories_.end(), [&](const identity_history &history) {
      return hash_equal(history.identity_id, identity_id);
    });
    if (found == histories_.end())
      return nullptr;
    const lifecycle_record_v2 *selected = nullptr;
    for (const auto &record : found->records)
    {
      const identity_descriptor_v2 &descriptor = record.next_descriptor;
      if (descriptor.effective_epoch <= epoch)
        selected = &record;
      else
        break;
    }
    if (!selected || epoch > selected->next_descriptor.expiry_epoch
        || selected->action == lifecycle_action_v2::deregister_identity)
      return nullptr;
    return &selected->next_descriptor;
  }

  std::vector<identity_descriptor_v2> lifecycle_registry_v2::descriptors_for_epoch(uint64_t epoch) const
  {
    std::vector<identity_descriptor_v2> descriptors;
    descriptors.reserve(histories_.size());
    for (const identity_history &history : histories_)
    {
      const identity_descriptor_v2 *descriptor = descriptor_for_epoch(history.identity_id, epoch);
      if (descriptor != nullptr)
        descriptors.push_back(*descriptor);
    }
    std::sort(descriptors.begin(), descriptors.end(), [](const identity_descriptor_v2 &left, const identity_descriptor_v2 &right) {
      return hash_less(left.identity_id, right.identity_id);
    });
    return descriptors;
  }

  crypto::hash lifecycle_registry_v2::state_hash() const
  {
    std::vector<const identity_history *> sorted;
    sorted.reserve(histories_.size());
    for (const auto &history : histories_)
      sorted.push_back(&history);
    std::sort(sorted.begin(), sorted.end(), [](const identity_history *left, const identity_history *right) {
      return hash_less(left->identity_id, right->identity_id);
    });
    std::string blob("QWC_EPOSE_LIFECYCLE_STATE_V2");
    append_network(blob, nettype_);
    append_bytes(blob, genesis_hash_);
    append_bytes(blob, parameter_set_hash_);
    append_u64_le(blob, sorted.size());
    for (const identity_history *history : sorted)
    {
      append_bytes(blob, history->identity_id);
      append_u64_le(blob, history->records.size());
      for (const auto &record : history->records)
        append_bytes(blob, hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, record));
    }
    return fast_hash(blob);
  }
} // namespace epose
} // namespace qwertycoin
