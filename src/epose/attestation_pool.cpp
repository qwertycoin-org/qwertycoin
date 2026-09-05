// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/attestation_pool.h"

#include <algorithm>
#include <cstring>

#include "epose/service_epoch.h"

namespace
{
  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned int shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_hash(std::string &out, const crypto::hash &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  void append_public_key(std::string &out, const crypto::public_key &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  bool public_key_equal(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
  }

  bool attestation_less(
      const qwertycoin::epose::service_attestation &left,
      const qwertycoin::epose::service_attestation &right)
  {
    if (left.epoch != right.epoch)
      return left.epoch < right.epoch;
    const int subject_cmp = std::memcmp(&left.subject_public_key, &right.subject_public_key, sizeof(left.subject_public_key));
    if (subject_cmp != 0)
      return subject_cmp < 0;
    const int verifier_cmp = std::memcmp(&left.verifier_public_key, &right.verifier_public_key, sizeof(left.verifier_public_key));
    if (verifier_cmp != 0)
      return verifier_cmp < 0;
    return std::memcmp(&left.challenge_hash, &right.challenge_hash, sizeof(left.challenge_hash)) < 0;
  }

  bool active_registration_exists(
      const std::vector<qwertycoin::epose::service_node_identity> &registered_nodes,
      const crypto::public_key &service_public_key,
      uint64_t epoch)
  {
    return std::find_if(registered_nodes.begin(), registered_nodes.end(), [&](const qwertycoin::epose::service_node_identity &identity) {
      return public_key_equal(identity.service_public_key, service_public_key)
          && qwertycoin::epose::identity_active_in_epoch(identity, epoch);
    }) != registered_nodes.end();
  }

  bool registration_epochs_overlap(
      const qwertycoin::epose::service_node_identity &left,
      const qwertycoin::epose::service_node_identity &right)
  {
    return left.registration_epoch < right.expiry_epoch && right.registration_epoch < left.expiry_epoch;
  }

  bool identity_less(
      const qwertycoin::epose::service_node_identity &left,
      const qwertycoin::epose::service_node_identity &right)
  {
    if (left.registration_epoch != right.registration_epoch)
      return left.registration_epoch < right.registration_epoch;
    if (left.expiry_epoch != right.expiry_epoch)
      return left.expiry_epoch < right.expiry_epoch;
    return std::memcmp(&left.service_public_key, &right.service_public_key, sizeof(left.service_public_key)) < 0;
  }

  bool selected_verifier(
      const std::vector<qwertycoin::epose::service_node_identity> &registered_nodes,
      const qwertycoin::epose::service_attestation &attestation,
      cryptonote::network_type nettype,
      const crypto::hash &epoch_seed)
  {
    const auto assignments = qwertycoin::epose::select_verifiers(
        registered_nodes,
        attestation.subject_public_key,
        nettype,
        attestation.epoch,
        epoch_seed);

    return std::find_if(assignments.begin(), assignments.end(), [&](const qwertycoin::epose::verifier_assignment &assignment) {
      return public_key_equal(assignment.verifier_public_key, attestation.verifier_public_key);
    }) != assignments.end();
  }
}

namespace qwertycoin
{
namespace epose
{
  attestation_pool::attestation_pool(size_t max_entries, uint8_t admission_leading_zero_bits)
    : max_entries_(max_entries),
      admission_leading_zero_bits_(admission_leading_zero_bits)
  {
  }

  std::string attestation_pool::key_for(const service_attestation &attestation)
  {
    std::string key;
    key.reserve(sizeof(uint64_t) + sizeof(crypto::public_key) * 2 + sizeof(crypto::hash));
    append_u64_le(key, attestation.epoch);
    append_public_key(key, attestation.subject_public_key);
    append_public_key(key, attestation.verifier_public_key);
    append_hash(key, attestation.challenge_hash);
    return key;
  }

  std::string attestation_pool::key_for_registration(const service_node_identity &identity)
  {
    std::string key;
    key.reserve(sizeof(uint64_t) + sizeof(crypto::public_key));
    append_u64_le(key, identity.registration_epoch);
    append_public_key(key, identity.service_public_key);
    return key;
  }

  bool attestation_pool::contains_pool_registration_conflict(const service_node_identity &identity) const
  {
    return std::find_if(registration_entries_.begin(), registration_entries_.end(), [&](const auto &entry) {
      const service_node_identity &registered = entry.second.identity;
      return (public_key_equal(registered.service_public_key, identity.service_public_key)
          || registered.endpoint_commitment == identity.endpoint_commitment)
          && registration_epochs_overlap(registered, identity);
    }) != registration_entries_.end();
  }

  bool attestation_pool::contains_chain_registration(
      const std::vector<service_node_identity> &registered_nodes,
      const service_node_identity &identity) const
  {
    return std::find_if(registered_nodes.begin(), registered_nodes.end(), [&](const service_node_identity &registered) {
      return (public_key_equal(registered.service_public_key, identity.service_public_key)
          || registered.endpoint_commitment == identity.endpoint_commitment)
          && registration_epochs_overlap(registered, identity);
    }) != registered_nodes.end();
  }

  bool attestation_pool::contains_chain_vote(
      const std::vector<service_attestation> &chain_attestations,
      const service_attestation &attestation) const
  {
    return std::find_if(chain_attestations.begin(), chain_attestations.end(), [&](const service_attestation &existing) {
      return existing.epoch == attestation.epoch
          && public_key_equal(existing.subject_public_key, attestation.subject_public_key)
          && public_key_equal(existing.verifier_public_key, attestation.verifier_public_key);
    }) != chain_attestations.end();
  }

  bool attestation_pool::validate(
      const service_attestation &attestation,
      const std::vector<service_node_identity> &registered_nodes,
      const std::vector<service_attestation> &chain_attestations,
      uint64_t current_epoch,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash) const
  {
    if (attestation.epoch < current_epoch)
      return false;
    if (attestation.epoch > current_epoch)
      return false;
    if (!verify_attestation_signature(attestation, nettype))
      return false;
    if (contains_chain_vote(chain_attestations, attestation))
      return false;
    if (!active_registration_exists(registered_nodes, attestation.subject_public_key, attestation.epoch)
        || !active_registration_exists(registered_nodes, attestation.verifier_public_key, attestation.epoch))
      return false;

    const crypto::hash epoch_seed = calculate_epoch_seed(nettype, attestation.epoch, previous_epoch_hash);
    const crypto::hash expected_challenge = calculate_challenge_hash(
        epoch_seed,
        attestation.subject_public_key,
        attestation.verifier_public_key,
        attestation.epoch,
        0);
    const crypto::hash expected_response = calculate_response_hash(
        expected_challenge,
        attestation.observed_tip_hash,
        attestation.subject_public_key,
        attestation.verifier_public_key,
        attestation.epoch);

    return selected_verifier(registered_nodes, attestation, nettype, epoch_seed)
        && attestation.challenge_hash == expected_challenge
        && attestation.response_hash == expected_response;
  }

  bool attestation_pool::validate_registration(
      const service_node_identity &identity,
      const std::vector<service_node_identity> &registered_nodes,
      uint64_t current_epoch,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash) const
  {
    if (identity.registration_epoch != current_epoch)
      return false;
    if (identity.expiry_epoch <= identity.registration_epoch
        || identity.expiry_epoch - identity.registration_epoch > EPOSE_REGISTRATION_TTL_EPOCHS)
      return false;
    if (contains_chain_registration(registered_nodes, identity))
      return false;
    return verify_registration_signature(identity, nettype)
        && verify_admission_proof(identity, nettype, previous_epoch_hash, admission_leading_zero_bits_);
  }

  attestation_pool_result attestation_pool::add_registration(
      const service_node_identity &identity,
      const std::vector<service_node_identity> &registered_nodes,
      uint64_t current_epoch,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash)
  {
    if (identity.registration_epoch < current_epoch)
      return attestation_pool_result::stale_epoch;
    if (identity.registration_epoch > current_epoch)
      return attestation_pool_result::future_epoch;

    const std::string key = key_for_registration(identity);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (registration_entries_.find(key) != registration_entries_.end()
          || contains_pool_registration_conflict(identity))
        return attestation_pool_result::duplicate;
      if (entries_.size() + registration_entries_.size() >= max_entries_)
        return attestation_pool_result::full;
    }

    if (!validate_registration(identity, registered_nodes, current_epoch, nettype, previous_epoch_hash))
      return attestation_pool_result::invalid;

    std::lock_guard<std::mutex> lock(mutex_);
    if (registration_entries_.find(key) != registration_entries_.end()
        || contains_pool_registration_conflict(identity))
      return attestation_pool_result::duplicate;
    if (entries_.size() + registration_entries_.size() >= max_entries_)
      return attestation_pool_result::full;

    registration_entries_.emplace(key, registration_entry{identity, current_epoch});
    return attestation_pool_result::accepted;
  }

  attestation_pool_result attestation_pool::add(
      const service_attestation &attestation,
      const std::vector<service_node_identity> &registered_nodes,
      const std::vector<service_attestation> &chain_attestations,
      uint64_t current_epoch,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash)
  {
    if (attestation.epoch < current_epoch)
      return attestation_pool_result::stale_epoch;
    if (attestation.epoch > current_epoch)
      return attestation_pool_result::future_epoch;

    const std::string key = key_for(attestation);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (entries_.find(key) != entries_.end())
        return attestation_pool_result::duplicate;
      if (entries_.size() + registration_entries_.size() >= max_entries_)
        return attestation_pool_result::full;
    }

    if (!validate(attestation, registered_nodes, chain_attestations, current_epoch, nettype, previous_epoch_hash))
      return attestation_pool_result::invalid;

    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.find(key) != entries_.end())
      return attestation_pool_result::duplicate;
    if (entries_.size() + registration_entries_.size() >= max_entries_)
      return attestation_pool_result::full;

    entries_.emplace(key, entry{attestation, current_epoch});
    return attestation_pool_result::accepted;
  }

  std::vector<service_attestation> attestation_pool::select_for_template(
      const std::vector<service_node_identity> &registered_nodes,
      const std::vector<service_attestation> &chain_attestations,
      uint64_t current_epoch,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash,
      size_t max_count) const
  {
    std::vector<service_attestation> selected;
    if (max_count == 0)
      return selected;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      selected.reserve(std::min(max_count, entries_.size()));
      for (const auto &item : entries_)
      {
        if (validate(item.second.attestation, registered_nodes, chain_attestations, current_epoch, nettype, previous_epoch_hash))
          selected.push_back(item.second.attestation);
      }
    }

    std::sort(selected.begin(), selected.end(), attestation_less);
    if (selected.size() > max_count)
      selected.resize(max_count);
    return selected;
  }

  std::vector<service_node_identity> attestation_pool::select_registrations_for_template(
      const std::vector<service_node_identity> &registered_nodes,
      uint64_t current_epoch,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash,
      size_t max_count) const
  {
    std::vector<service_node_identity> selected;
    if (max_count == 0)
      return selected;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      selected.reserve(std::min(max_count, registration_entries_.size()));
      for (const auto &item : registration_entries_)
      {
        if (validate_registration(item.second.identity, registered_nodes, current_epoch, nettype, previous_epoch_hash))
          selected.push_back(item.second.identity);
      }
    }

    std::sort(selected.begin(), selected.end(), identity_less);
    if (selected.size() > max_count)
      selected.resize(max_count);
    return selected;
  }

  void attestation_pool::prune(uint64_t current_epoch)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end();)
    {
      if (it->second.attestation.epoch + EPOSE_STATE_RETENTION_EPOCHS < current_epoch)
        it = entries_.erase(it);
      else
        ++it;
    }
    for (auto it = registration_entries_.begin(); it != registration_entries_.end();)
    {
      if (it->second.identity.registration_epoch < current_epoch)
        it = registration_entries_.erase(it);
      else
        ++it;
    }
  }

  attestation_pool_stats attestation_pool::stats(uint64_t current_epoch) const
  {
    attestation_pool_stats stats{};
    std::lock_guard<std::mutex> lock(mutex_);
    stats.registrations = registration_entries_.size();
    stats.attestations = entries_.size();
    stats.total = stats.registrations + stats.attestations;
    for (const auto &item : registration_entries_)
    {
      if (item.second.identity.registration_epoch == current_epoch)
        ++stats.current_epoch;
      else if (current_epoch > 0 && item.second.identity.registration_epoch == current_epoch - 1)
        ++stats.previous_epoch;
    }
    for (const auto &item : entries_)
    {
      if (item.second.attestation.epoch == current_epoch)
        ++stats.current_epoch;
      else if (current_epoch > 0 && item.second.attestation.epoch == current_epoch - 1)
        ++stats.previous_epoch;
    }
    return stats;
  }
} // namespace epose
} // namespace qwertycoin
