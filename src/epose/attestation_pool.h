// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "epose/service_node.h"

namespace qwertycoin
{
namespace epose
{
  constexpr size_t EPOSE_ATTESTATION_POOL_MAX_ENTRIES = 4096;
  constexpr size_t EPOSE_ATTESTATION_RELAY_MAX_BATCH = 32;

  enum class attestation_pool_result
  {
    accepted,
    duplicate,
    invalid,
    stale_epoch,
    future_epoch,
    full
  };

  struct attestation_pool_stats
  {
    size_t registrations = 0;
    size_t attestations = 0;
    size_t total = 0;
    size_t current_epoch = 0;
    size_t previous_epoch = 0;
  };

  class attestation_pool
  {
  public:
    explicit attestation_pool(
        size_t max_entries = EPOSE_ATTESTATION_POOL_MAX_ENTRIES,
        uint8_t admission_leading_zero_bits = EPOSE_ADMISSION_LEADING_ZERO_BITS);

    attestation_pool_result add_registration(
        const service_node_identity &identity,
        const std::vector<service_node_identity> &registered_nodes,
        uint64_t current_epoch,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash);

    attestation_pool_result add(
        const service_attestation &attestation,
        const std::vector<service_node_identity> &registered_nodes,
        const std::vector<service_attestation> &chain_attestations,
        uint64_t current_epoch,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash);

    std::vector<service_attestation> select_for_template(
        const std::vector<service_node_identity> &registered_nodes,
        const std::vector<service_attestation> &chain_attestations,
        uint64_t current_epoch,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash,
        size_t max_count) const;

    std::vector<service_node_identity> select_registrations_for_template(
        const std::vector<service_node_identity> &registered_nodes,
        uint64_t current_epoch,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash,
        size_t max_count) const;

    void prune(uint64_t current_epoch);
    attestation_pool_stats stats(uint64_t current_epoch) const;

  private:
    struct entry
    {
      service_attestation attestation{};
      uint64_t first_seen_epoch = 0;
    };

    struct registration_entry
    {
      service_node_identity identity{};
      uint64_t first_seen_epoch = 0;
    };

    static std::string key_for(const service_attestation &attestation);
    static std::string key_for_registration(const service_node_identity &identity);
    bool contains_pool_registration_conflict(const service_node_identity &identity) const;
    bool contains_chain_registration(
        const std::vector<service_node_identity> &registered_nodes,
        const service_node_identity &identity) const;
    bool contains_chain_vote(
        const std::vector<service_attestation> &chain_attestations,
        const service_attestation &attestation) const;
    bool validate_registration(
        const service_node_identity &identity,
        const std::vector<service_node_identity> &registered_nodes,
        uint64_t current_epoch,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash) const;
    bool validate(
        const service_attestation &attestation,
        const std::vector<service_node_identity> &registered_nodes,
        const std::vector<service_attestation> &chain_attestations,
        uint64_t current_epoch,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash) const;

    const size_t max_entries_;
    const uint8_t admission_leading_zero_bits_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, entry> entries_;
    std::unordered_map<std::string, registration_entry> registration_entries_;
  };
} // namespace epose
} // namespace qwertycoin
