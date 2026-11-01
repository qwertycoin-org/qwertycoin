// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/lifecycle_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_node.h"
#include "epose/state_image_v2.h"
#include "epose/transport_profile_v2.h"
#include "epose/hash_transcript.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

namespace
{
  using qwertycoin::epose::transcript::append_bytes;
  using qwertycoin::epose::transcript::append_u8;
  using qwertycoin::epose::transcript::append_u64_le;
  using qwertycoin::epose::transcript::append_network;
  using qwertycoin::epose::transcript::hash_blob;
  using qwertycoin::epose::transcript::bytes_equal;
  using qwertycoin::epose::transcript::bytes_less;
  using qwertycoin::epose::identity_descriptor_v2;
  using qwertycoin::epose::lifecycle_action_v2;
  using qwertycoin::epose::lifecycle_record_v2;

  template <typename T>
  std::string index_key(const T &value)
  {
    return {reinterpret_cast<const char *>(&value), sizeof(value)};
  }

  bool has_key(const std::vector<crypto::hash> &keys, const crypto::hash &key)
  {
    return std::binary_search(keys.begin(), keys.end(), key, bytes_less<crypto::hash>);
  }

  crypto::hash retired_service_key(const crypto::public_key &key)
  {
    std::string blob("QWC_EPOSE_RETIRED_SERVICE_KEY_V2");
    append_bytes(blob, key);
    return hash_blob(blob);
  }

  crypto::hash hash_retired_keys(const std::vector<crypto::hash> &keys)
  {
    std::string blob("QWC_EPOSE_RETIRED_KEY_VECTOR_V2");
    append_u64_le(blob, keys.size());
    for (const crypto::hash &key : keys)
      append_bytes(blob, key);
    return hash_blob(blob);
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
    return (descriptor.version == qwertycoin::epose::EPOSE_DESCRIPTOR_VERSION_V2
            || descriptor.version == 2)
        && descriptor.identity_id != crypto::null_hash
        && valid_key(descriptor.service_public_key)
        && valid_key(descriptor.operator_authorization_public_key)
        && valid_key(descriptor.reward_address.m_view_public_key)
        && valid_key(descriptor.reward_address.m_spend_public_key)
        && descriptor.endpoint_descriptor_hash != crypto::null_hash
        && descriptor.expiry_epoch >= descriptor.effective_epoch
        && (descriptor.version != 1
            || (descriptor.endpoint_transport == 1
                && descriptor.dial_capabilities == 1
                && descriptor.dns_address_families == 0))
        && (descriptor.version != 2
            || qwertycoin::epose::valid_transport_profile_v2(
                descriptor.endpoint_transport, descriptor.dial_capabilities,
                descriptor.dns_address_families));
  }

  bool descriptor_static_equal(const identity_descriptor_v2 &left, const identity_descriptor_v2 &right)
  {
    return left.identity_id == right.identity_id
        && left.service_public_key == right.service_public_key
        && left.operator_authorization_public_key == right.operator_authorization_public_key
        && left.reward_address.m_view_public_key == right.reward_address.m_view_public_key
        && left.reward_address.m_spend_public_key == right.reward_address.m_spend_public_key
        && left.endpoint_descriptor_hash == right.endpoint_descriptor_hash
        && left.endpoint_transport == right.endpoint_transport
        && left.dial_capabilities == right.dial_capabilities
        && left.dns_address_families == right.dns_address_families;
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
    return hash_blob(blob);
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
    if (descriptor.version == 2)
    {
      append_u8(blob, descriptor.endpoint_transport);
      append_u8(blob, descriptor.dial_capabilities);
      append_u8(blob, descriptor.dns_address_families);
    }
    return hash_blob(blob);
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
    return hash_blob(blob);
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
    return hash_blob(blob);
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

  state_memory_usage_v2 lifecycle_registry_v2::memory_usage() const
  {
    state_memory_usage_v2 usage{};
    usage.identities = histories_.size();
    usage.vector_payload_bytes = histories_.capacity() * sizeof(std::shared_ptr<identity_history>);
    for (const auto &history : histories_)
    {
      usage.lifecycle_records += history->records.size();
      usage.vector_payload_bytes += sizeof(identity_history)
          + history->records.capacity() * sizeof(lifecycle_record_v2);
    }
    return usage;
  }

  lifecycle_registry_v2::undo_journal lifecycle_registry_v2::checkpoint() const
  {
    undo_journal undo{};
    undo.prior_retired_key_accumulator = retired_key_accumulator_;
    undo.prior_window_active = window_active_;
    undo.prior_retained_epochs = retained_epochs_;
    return undo;
  }

  void lifecycle_registry_v2::rollback(const undo_journal &undo) noexcept
  {
    for (auto it = undo.inserted_services.rbegin(); it != undo.inserted_services.rend(); ++it)
    {
      const auto indexed = service_key_histories_.find(it->key);
      if (indexed != service_key_histories_.end())
      {
        indexed->second.erase(it->index);
        if (indexed->second.empty())
          service_key_histories_.erase(indexed);
      }
    }
    for (auto it = undo.record_sizes.rbegin(); it != undo.record_sizes.rend(); ++it)
      histories_[it->index]->records.resize(it->records_size);
    for (auto it = undo.registered_indices.rbegin(); it != undo.registered_indices.rend(); ++it)
    {
      identity_index_.erase(index_key(histories_[*it]->identity_id));
      histories_.pop_back();
    }
    for (auto frame = undo.folds.rbegin(); frame != undo.folds.rend(); ++frame)
    {
      for (auto it = frame->removed.rbegin(); it != frame->removed.rend(); ++it)
      {
        const size_t last = histories_.size();
        if (it->index != last)
        {
          histories_.push_back(histories_[it->index]);
          move_history_indexes(it->index, last);
          histories_[it->index] = it->history;
        }
        else
          histories_.push_back(it->history);
        add_history_indexes(it->index);
      }
      for (const auto &entry : frame->trimmed)
        histories_[entry.first] = entry.second;
      for (const auto &entry : frame->removed_services)
        service_key_histories_[entry.key].insert(entry.index);
      for (const crypto::hash &key : frame->retired_keys)
      {
        const auto found = std::lower_bound(retired_key_hashes_.begin(),
            retired_key_hashes_.end(), key, bytes_less<crypto::hash>);
        if (found != retired_key_hashes_.end() && bytes_equal(*found, key))
          retired_key_hashes_.erase(found);
      }
    }
    retired_key_accumulator_ = undo.prior_retired_key_accumulator;
    window_active_ = undo.prior_window_active;
    retained_epochs_ = undo.prior_retained_epochs;
    state_hash_cache_.reset();
  }

  bool lifecycle_registry_v2::valid() const
  {
    return valid_;
  }

  bool lifecycle_registry_v2::contains(const lifecycle_record_v2 &record) const
  {
    const auto message = hash_lifecycle_record_v2(
        nettype_, genesis_hash_, parameter_set_hash_, record);
    const auto found = identity_index_.find(index_key(record.next_descriptor.identity_id));
    if (found != identity_index_.end())
      for (const auto &stored : histories_[found->second]->records)
        if (message == hash_lifecycle_record_v2(
                nettype_, genesis_hash_, parameter_set_hash_, stored))
          return true;
    return false;
  }

  lifecycle_status_v2 lifecycle_registry_v2::apply(
      const lifecycle_record_v2 &record,
      uint64_t inclusion_epoch,
      uint64_t minimum_effective_epoch,
      verification_counters_v2 *counters,
      undo_journal *undo)
  {
    if (undo == nullptr || !valid_)
      return lifecycle_status_v2::invalid_context;
    const lifecycle_status_v2 structure = validate_lifecycle_record_structure_v2(
        nettype_, genesis_hash_, parameter_set_hash_, record);
    if (structure != lifecycle_status_v2::accepted)
      return structure;
    const identity_descriptor_v2 &next = record.next_descriptor;
    if (next.effective_epoch < minimum_effective_epoch || next.effective_epoch < inclusion_epoch)
      return lifecycle_status_v2::retroactive_change;
    if (window_active_)
    {
      if (next.expiry_epoch - next.effective_epoch > EPOSE_REGISTRATION_TTL_EPOCHS)
        return lifecycle_status_v2::invalid_descriptor;
      const uint64_t maximum_effective_epoch = inclusion_epoch
          > std::numeric_limits<uint64_t>::max() - retained_epochs_
          ? std::numeric_limits<uint64_t>::max()
          : inclusion_epoch + retained_epochs_;
      if (next.effective_epoch > maximum_effective_epoch)
        return lifecycle_status_v2::invalid_transition;
    }

    const crypto::hash message = hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, record);

    const auto identity = identity_index_.find(index_key(next.identity_id));
    const size_t history_index = identity == identity_index_.end()
        ? histories_.size() : identity->second;
    const identity_history *history = history_index == histories_.size()
        ? nullptr : histories_[history_index].get();
    bool idempotent_duplicate = false;
    if (history != nullptr)
    {
      const auto duplicate = std::find_if(history->records.begin(), history->records.end(), [&](const lifecycle_record_v2 &stored) {
        return message == hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, stored);
      });
      idempotent_duplicate = duplicate != history->records.end();
    }
    const bool registering = record.action == lifecycle_action_v2::register_identity;
    if (!idempotent_duplicate && registering)
    {
      if (history != nullptr || has_key(retired_key_hashes_, next.identity_id))
        return lifecycle_status_v2::identity_already_exists;
      if (window_active_ && has_key(retired_key_hashes_, retired_service_key(next.service_public_key)))
        return lifecycle_status_v2::service_key_in_use;
      if (record.previous_descriptor_hash != crypto::null_hash || next.sequence != 0)
        return lifecycle_status_v2::invalid_transition;
    }
    else if (!idempotent_duplicate)
    {
      if (history == nullptr)
        return lifecycle_status_v2::identity_not_found;
      const identity_descriptor_v2 &current = history->records.back().next_descriptor;
      const crypto::hash current_hash = hash_identity_descriptor_v2(nettype_, genesis_hash_, parameter_set_hash_, current);
      if (record.previous_descriptor_hash != current_hash)
        return lifecycle_status_v2::wrong_previous_descriptor;
      if (current.sequence == std::numeric_limits<uint64_t>::max() || next.sequence != current.sequence + 1)
        return lifecycle_status_v2::wrong_sequence;
      if (next.effective_epoch <= current.effective_epoch)
        return lifecycle_status_v2::overlapping_change;
      if (next.operator_authorization_public_key != current.operator_authorization_public_key)
        return lifecycle_status_v2::invalid_transition;
      if (history->records.back().action == lifecycle_action_v2::deregister_identity)
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
      const auto indexed_histories = service_key_histories_.find(
          index_key(next.service_public_key));
      if (indexed_histories != service_key_histories_.end())
      {
        for (const size_t indexed_history : indexed_histories->second)
        {
          const identity_history &existing_history = *histories_[indexed_history];
          if (bytes_equal(existing_history.identity_id, next.identity_id))
            continue;
          if (window_active_ && registering
              && existing_history.records.back().action == lifecycle_action_v2::deregister_identity)
            return lifecycle_status_v2::service_key_in_use;
          for (size_t index = 0; index < existing_history.records.size(); ++index)
          {
            const lifecycle_record_v2 &existing_record = existing_history.records[index];
            if (existing_record.action == lifecycle_action_v2::deregister_identity)
              continue;
            const identity_descriptor_v2 &existing = existing_record.next_descriptor;
            if (existing.service_public_key != next.service_public_key)
              continue;
            uint64_t existing_end = existing.expiry_epoch;
            if (index + 1 < existing_history.records.size())
            {
              const uint64_t next_start = existing_history.records[index + 1].next_descriptor.effective_epoch;
              existing_end = std::min(existing_end, next_start);
            }
            if (proposed_start < existing_end && existing.effective_epoch < proposed_end)
              return lifecycle_status_v2::service_key_in_use;
          }
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
      auto added = std::make_shared<identity_history>();
      added->identity_id = next.identity_id;
      added->records.push_back(record);
      if (undo != nullptr)
        undo->registered_indices.reserve(undo->registered_indices.size() + 1);
      histories_.push_back(std::move(added));
      if (undo != nullptr)
        undo->registered_indices.push_back(histories_.size() - 1);
      identity_index_.emplace(index_key(next.identity_id), histories_.size() - 1);
    }
    else
    {
      auto &updated = histories_[history_index];
      if (undo != nullptr)
        undo->record_sizes.push_back({history_index, updated->records.size()});
      if (!updated.unique())
        updated = std::make_shared<identity_history>(*updated);
      updated->records.push_back(record);
    }
    if (record.action != lifecycle_action_v2::deregister_identity)
    {
      const std::string service_key = index_key(next.service_public_key);
      std::set<size_t> &indexed = service_key_histories_[service_key];
      const size_t accepted_history = registering ? histories_.size() - 1 : history_index;
      if (indexed.count(accepted_history) == 0)
      {
        if (undo != nullptr)
          undo->inserted_services.push_back({service_key, accepted_history});
        indexed.insert(accepted_history);
      }
    }
    state_hash_cache_.reset();
    return lifecycle_status_v2::accepted;
  }

  const identity_descriptor_v2 *lifecycle_registry_v2::latest(const crypto::hash &identity_id) const
  {
    const lifecycle_record_v2 *record = latest_record(identity_id);
    return record == nullptr ? nullptr : &record->next_descriptor;
  }

  const lifecycle_record_v2 *lifecycle_registry_v2::latest_record(const crypto::hash &identity_id) const
  {
    const auto found = identity_index_.find(index_key(identity_id));
    return found == identity_index_.end()
        ? nullptr : &histories_[found->second]->records.back();
  }

  const identity_descriptor_v2 *lifecycle_registry_v2::descriptor_for_epoch(
      const crypto::hash &identity_id,
      uint64_t epoch) const
  {
    const auto found = identity_index_.find(index_key(identity_id));
    if (found == identity_index_.end())
      return nullptr;
    const auto &records = histories_[found->second]->records;
    const auto after = std::upper_bound(records.begin(), records.end(), epoch,
        [](uint64_t requested, const lifecycle_record_v2 &record) {
          return requested < record.next_descriptor.effective_epoch;
        });
    if (after == records.begin())
      return nullptr;
    const lifecycle_record_v2 &selected = *std::prev(after);
    if (epoch >= selected.next_descriptor.expiry_epoch
        || selected.action == lifecycle_action_v2::deregister_identity)
      return nullptr;
    return &selected.next_descriptor;
  }

  std::vector<identity_descriptor_v2> lifecycle_registry_v2::descriptors_for_epoch(uint64_t epoch) const
  {
    std::vector<identity_descriptor_v2> descriptors;
    descriptors.reserve(histories_.size());
    for (const auto &history : histories_)
    {
      const identity_descriptor_v2 *descriptor = descriptor_for_epoch(history->identity_id, epoch);
      if (descriptor != nullptr)
        descriptors.push_back(*descriptor);
    }
    std::sort(descriptors.begin(), descriptors.end(), [](const identity_descriptor_v2 &left, const identity_descriptor_v2 &right) {
      return bytes_less(left.identity_id, right.identity_id);
    });
    return descriptors;
  }

  size_t lifecycle_registry_v2::state_bytes() const
  {
    size_t bytes = sizeof(*this) + histories_.capacity()
        * sizeof(std::shared_ptr<identity_history>);
    for (const auto &history : histories_)
      bytes += sizeof(identity_history)
          + history->records.capacity() * sizeof(lifecycle_record_v2);
    for (const auto &indexed : identity_index_)
      bytes += sizeof(indexed) + 3 * sizeof(void *)
          + indexed.first.capacity();
    for (const auto &indexed : service_key_histories_)
      bytes += sizeof(indexed) + 3 * sizeof(void *)
          + indexed.first.capacity()
          + indexed.second.size() * (sizeof(size_t) + 3 * sizeof(void *));
    bytes += retired_key_hashes_.capacity() * sizeof(crypto::hash);
    return bytes;
  }

  void lifecycle_registry_v2::activate_window(uint64_t retained_epochs, undo_journal *undo)
  {
    if (undo == nullptr || retained_epochs == 0)
      return;
    window_active_ = true;
    retained_epochs_ = retained_epochs;
    retired_key_accumulator_ = hash_retired_keys(retired_key_hashes_);
    state_hash_cache_.reset();
  }

  bool lifecycle_registry_v2::should_retire(const identity_history &history,
      uint64_t epoch, uint64_t oldest) const
  {
    const lifecycle_record_v2 &last = history.records.back();
    return last.next_descriptor.effective_epoch <= epoch
        && (last.action == lifecycle_action_v2::deregister_identity
            || (window_active_ && last.next_descriptor.expiry_epoch <= oldest));
  }

  std::vector<crypto::hash> lifecycle_registry_v2::fold_retired(uint64_t epoch, undo_journal *undo)
  {
    if (undo == nullptr)
      return {};
    undo->folds.emplace_back();
    undo_journal::fold_change *change = &undo->folds.back();
    std::vector<const identity_history *> retired;
    std::vector<size_t> trimmed;
    const uint64_t oldest = epoch == std::numeric_limits<uint64_t>::max()
        ? epoch : epoch + 1;
    for (size_t history_index = 0; history_index < histories_.size(); ++history_index)
    {
      const auto &history = histories_[history_index];
      if (should_retire(*history, epoch, oldest))
        retired.push_back(history.get());
      else if (window_active_ && history->records.size() > 1)
        trimmed.push_back(history_index);
    }
    std::sort(retired.begin(), retired.end(), [](const identity_history *left, const identity_history *right) {
      return bytes_less(left->identity_id, right->identity_id);
    });
    std::vector<crypto::hash> digests;
    digests.reserve(retired.size());
    std::vector<crypto::hash> retiring_keys;
    for (const identity_history *history : retired)
    {
      std::string blob("QWC_EPOSE_RETIRED_IDENTITY_V2");
      append_bytes(blob, history->identity_id);
      append_u64_le(blob, history->records.size());
      for (const auto &record : history->records)
        append_bytes(blob, hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, record));
      digests.push_back(hash_blob(blob));
      if (history->records.back().action == lifecycle_action_v2::deregister_identity)
      {
        retiring_keys.push_back(history->identity_id);
        if (window_active_)
          for (const auto &record : history->records)
            retiring_keys.push_back(retired_service_key(record.next_descriptor.service_public_key));
      }
    }
    if (!retiring_keys.empty())
    {
      std::sort(retiring_keys.begin(), retiring_keys.end(), bytes_less<crypto::hash>);
      retiring_keys.erase(std::unique(retiring_keys.begin(), retiring_keys.end(), bytes_equal<crypto::hash>),
          retiring_keys.end());
      std::set_difference(retiring_keys.begin(), retiring_keys.end(),
          retired_key_hashes_.begin(), retired_key_hashes_.end(),
          std::back_inserter(change->retired_keys), bytes_less<crypto::hash>);
      if (!change->retired_keys.empty())
      {
        retired_key_hashes_.insert(retired_key_hashes_.end(),
            change->retired_keys.begin(), change->retired_keys.end());
        std::sort(retired_key_hashes_.begin(), retired_key_hashes_.end(), bytes_less<crypto::hash>);
        retired_key_hashes_.erase(std::unique(retired_key_hashes_.begin(),
            retired_key_hashes_.end(), bytes_equal<crypto::hash>), retired_key_hashes_.end());
        std::vector<crypto::hash>(retired_key_hashes_).swap(retired_key_hashes_);
      }
    }
    std::sort(trimmed.begin(), trimmed.end(), [&](size_t left, size_t right) {
      return bytes_less(histories_[left]->identity_id, histories_[right]->identity_id);
    });
    for (const size_t history_index : trimmed)
    {
      const identity_history *history = histories_[history_index].get();
      const auto &records = history->records;
      const auto first_retained = std::lower_bound(records.begin(), records.end(), oldest,
          [](const lifecycle_record_v2 &record, uint64_t boundary) {
            return record.next_descriptor.effective_epoch < boundary;
          });
      if (first_retained == records.begin())
        continue;
      const size_t trim_count = static_cast<size_t>(first_retained - records.begin() - 1);
      if (trim_count == 0)
        continue;
      if (change != nullptr)
        change->trimmed.push_back({history_index, histories_[history_index]});
      std::set<std::string> retained_services;
      for (size_t index = trim_count; index < records.size(); ++index)
        if (records[index].action != lifecycle_action_v2::deregister_identity)
          retained_services.insert(index_key(records[index].next_descriptor.service_public_key));
      for (size_t index = 0; index < trim_count; ++index)
      {
        if (records[index].action == lifecycle_action_v2::deregister_identity)
          continue;
        const std::string key = index_key(records[index].next_descriptor.service_public_key);
        if (retained_services.count(key) == 0)
        {
          auto indexed = service_key_histories_.find(key);
          if (indexed != service_key_histories_.end()
              && indexed->second.count(history_index) != 0)
          {
            if (change != nullptr)
              change->removed_services.push_back({key, history_index});
            indexed->second.erase(history_index);
            if (indexed->second.empty())
              service_key_histories_.erase(indexed);
          }
        }
      }
      std::string blob("QWC_EPOSE_TRIMMED_LIFECYCLE_V2");
      append_bytes(blob, history->identity_id);
      append_u64_le(blob, trim_count);
      for (size_t index = 0; index < trim_count; ++index)
        append_bytes(blob, hash_lifecycle_record_v2(nettype_, genesis_hash_, parameter_set_hash_, records[index]));
      digests.push_back(hash_blob(blob));
      auto &mutable_history = histories_[history_index];
      if (!mutable_history.unique())
        mutable_history = std::make_shared<identity_history>(*mutable_history);
      mutable_history->records.erase(mutable_history->records.begin(),
          mutable_history->records.begin() + trim_count);
      std::vector<lifecycle_record_v2>(mutable_history->records).swap(mutable_history->records);
    }
    for (const identity_history *history : retired)
    {
      const auto found = identity_index_.find(index_key(history->identity_id));
      if (found == identity_index_.end())
        continue;
      const size_t index = found->second;
      if (change != nullptr)
        change->removed.push_back({index, histories_[index]});
      remove_history_indexes(index);
      const size_t last = histories_.size() - 1;
      if (index != last)
      {
        histories_[index] = histories_[last];
        move_history_indexes(last, index);
      }
      histories_.pop_back();
    }
    if (!change->retired_keys.empty())
      retired_key_accumulator_ = hash_retired_keys(retired_key_hashes_);
    state_hash_cache_.reset();
    return digests;
  }

  void lifecycle_registry_v2::remove_history_indexes(size_t index)
  {
    const identity_history &history = *histories_[index];
    identity_index_.erase(index_key(history.identity_id));
    for (const lifecycle_record_v2 &record : history.records)
    {
      if (record.action == lifecycle_action_v2::deregister_identity)
        continue;
      const std::string key = index_key(record.next_descriptor.service_public_key);
      auto indexed = service_key_histories_.find(key);
      if (indexed != service_key_histories_.end())
      {
        indexed->second.erase(index);
        if (indexed->second.empty())
          service_key_histories_.erase(indexed);
      }
    }
  }

  void lifecycle_registry_v2::add_history_indexes(size_t index)
  {
    const identity_history &history = *histories_[index];
    identity_index_[index_key(history.identity_id)] = index;
    for (const lifecycle_record_v2 &record : history.records)
      if (record.action != lifecycle_action_v2::deregister_identity)
        service_key_histories_[index_key(record.next_descriptor.service_public_key)].insert(index);
  }

  void lifecycle_registry_v2::move_history_indexes(size_t from, size_t to)
  {
    const identity_history &history = *histories_[to];
    identity_index_[index_key(history.identity_id)] = to;
    for (const lifecycle_record_v2 &record : history.records)
    {
      if (record.action == lifecycle_action_v2::deregister_identity)
        continue;
      std::set<size_t> &indexed = service_key_histories_[index_key(record.next_descriptor.service_public_key)];
      indexed.erase(from);
      indexed.insert(to);
    }
  }

  void lifecycle_registry_v2::rebuild_indexes()
  {
    identity_index_.clear();
    service_key_histories_.clear();
    for (size_t index = 0; index < histories_.size(); ++index)
      add_history_indexes(index);
  }

  std::vector<const lifecycle_registry_v2::identity_history *>
  lifecycle_registry_v2::sorted_histories() const
  {
    std::vector<const identity_history *> sorted;
    sorted.reserve(histories_.size());
    for (const auto &history : histories_)
      sorted.push_back(history.get());
    std::sort(sorted.begin(), sorted.end(), [](const identity_history *left, const identity_history *right) {
      return bytes_less(left->identity_id, right->identity_id);
    });
    return sorted;
  }

  bool lifecycle_registry_v2::write_image(state_image_writer_v2 &writer) const
  {
    writer.number(histories_.size());
    for (const identity_history *history : sorted_histories())
    {
      writer.fixed(history->identity_id);
      writer.number(history->records.size());
      for (const lifecycle_record_v2 &record : history->records)
      {
        envelope_record_v2 encoded{};
        if (encode_applied_lifecycle_record_v2(record, encoded)
                != record_codec_status_v2::accepted)
          return false;
        writer.byte(encoded.type);
        writer.byte(encoded.version);
        writer.bytes(encoded.payload);
      }
    }
    writer.number(retired_key_hashes_.size());
    for (const crypto::hash &key : retired_key_hashes_)
      writer.fixed(key);
    return true;
  }

  bool lifecycle_registry_v2::read_image(state_image_reader_v2 &reader)
  {
    lifecycle_registry_v2 parsed(nettype_, genesis_hash_, parameter_set_hash_);
    uint64_t history_count = 0;
    if (!reader.count(history_count, sizeof(crypto::hash) + 8))
      return false;
    for (uint64_t index = 0; index < history_count; ++index)
    {
      auto history = std::make_shared<identity_history>();
      uint64_t record_count = 0;
      if (!reader.fixed(history->identity_id)
          || history->identity_id == crypto::null_hash
          || !reader.count(record_count, 2 + 8 + EPOSE_LIFECYCLE_PAYLOAD_BYTES_V2)
          || record_count == 0)
        return false;
      for (uint64_t record_index = 0; record_index < record_count; ++record_index)
      {
        envelope_record_v2 encoded{};
        lifecycle_record_v2 record{};
        if (!reader.byte(encoded.type) || !reader.byte(encoded.version)
            || !reader.bytes(encoded.payload)
            || decode_lifecycle_record_structure_v2(encoded, record)
                != record_codec_status_v2::accepted
            || record.next_descriptor.identity_id != history->identity_id)
          return false;
        history->records.push_back(std::move(record));
      }
      parsed.histories_.push_back(std::move(history));
    }
    uint64_t key_count = 0;
    if (!reader.count(key_count, sizeof(crypto::hash)))
      return false;
    for (uint64_t index = 0; index < key_count; ++index)
    {
      crypto::hash key{};
      if (!reader.fixed(key) || key == crypto::null_hash)
        return false;
      parsed.retired_key_hashes_.push_back(key);
    }
    std::sort(parsed.retired_key_hashes_.begin(), parsed.retired_key_hashes_.end(), bytes_less<crypto::hash>);
    if (std::unique(parsed.retired_key_hashes_.begin(),
            parsed.retired_key_hashes_.end(), bytes_equal<crypto::hash>) != parsed.retired_key_hashes_.end())
      return false;
    parsed.retired_key_accumulator_ = hash_retired_keys(parsed.retired_key_hashes_);
    std::vector<crypto::hash>(parsed.retired_key_hashes_).swap(parsed.retired_key_hashes_);
    parsed.rebuild_indexes();
    if (parsed.identity_index_.size() != parsed.histories_.size())
      return false;
    for (const auto &history : parsed.histories_)
      if (has_key(parsed.retired_key_hashes_, history->identity_id))
        return false;
    *this = std::move(parsed);
    return true;
  }

  crypto::hash lifecycle_registry_v2::state_hash() const
  {
    if (state_hash_cache_.has_value())
      return *state_hash_cache_;
    const auto sorted = sorted_histories();
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
    if (!retired_key_hashes_.empty())
    {
      blob.append("QWC_EPOSE_RETIRED_KEYS_V2");
      append_u64_le(blob, retired_key_hashes_.size());
      if (window_active_)
        append_bytes(blob, retired_key_accumulator_);
      else
        for (const crypto::hash &key : retired_key_hashes_)
          append_bytes(blob, key);
    }
    state_hash_cache_ = hash_blob(blob);
    return *state_hash_cache_;
  }
} // namespace epose
} // namespace qwertycoin
