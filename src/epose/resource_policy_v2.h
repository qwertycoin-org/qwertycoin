// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
  enum class endpoint_transport_v2 : uint8_t
  {
    tcp_ipv4 = 1,
    tcp_ipv6 = 2,
    dns = 3
  };

  struct endpoint_descriptor_v2
  {
    uint8_t version = 1;
    crypto::public_key service_public_key{};
    endpoint_transport_v2 transport = endpoint_transport_v2::tcp_ipv4;
    std::string host;
    uint16_t port = 0;
    uint8_t service_kind = 0;
    uint8_t service_version = 0;
    uint64_t sequence = 0;
    uint64_t expiry_epoch = 0;
    crypto::signature signature{};
  };

  enum class resource_status_v2
  {
    accepted,
    idempotent_duplicate,
    invalid_configuration,
    invalid_descriptor,
    invalid_signature,
    noncanonical_host,
    prohibited_address,
    resolution_empty,
    resolution_limit_exceeded,
    request_too_large,
    response_too_large,
    timeout_too_large,
    concurrency_exceeded,
    peer_limit_exceeded,
    unknown_admission_context,
    context_cache_full,
    invalid_page,
    scan_limit_exceeded,
    relay_item_expired,
    relay_queue_full,
    relay_item_conflict
  };

  crypto::hash hash_endpoint_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const endpoint_descriptor_v2 &descriptor);

  bool sign_endpoint_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      endpoint_descriptor_v2 &descriptor,
      const crypto::secret_key &service_secret_key);

  resource_status_v2 validate_endpoint_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const endpoint_descriptor_v2 &descriptor);

  bool public_probe_address_v2(const std::string &address);
  resource_status_v2 validate_resolved_targets_v2(
      const std::vector<std::string> &addresses,
      size_t max_addresses);

  struct probe_limits_v2
  {
    size_t max_concurrent = 0;
    size_t max_pending_peers = 0;
    size_t max_per_peer = 0;
    size_t max_request_bytes = 0;
    size_t max_response_bytes = 0;
    uint64_t max_timeout_ms = 0;

    bool valid() const;
  };

  class probe_budget_v2
  {
  public:
    explicit probe_budget_v2(const probe_limits_v2 &limits);
    resource_status_v2 acquire(
        const crypto::hash &peer,
        size_t request_bytes,
        size_t response_limit,
        uint64_t timeout_ms);
    void release(const crypto::hash &peer);
    size_t active() const;

  private:
    struct peer_count { crypto::hash peer{}; size_t count = 0; };
    probe_limits_v2 limits_{};
    size_t active_ = 0;
    std::vector<peer_count> peers_;
  };

  class admission_context_cache_v2
  {
  public:
    explicit admission_context_cache_v2(size_t max_contexts);
    resource_status_v2 admit(
        const crypto::hash &context,
        const std::vector<crypto::hash> &currently_allowed);
    size_t size() const;

  private:
    size_t max_contexts_ = 0;
    std::vector<crypto::hash> contexts_;
  };

  enum class relay_class_v2 : uint8_t
  {
    enrollment = 1,
    evidence = 2
  };

  struct relay_item_v2
  {
    crypto::hash id{};
    relay_class_v2 record_class = relay_class_v2::enrollment;
    size_t bytes = 0;
    uint64_t deadline_height = 0;
  };

  struct relay_queue_limits_v2
  {
    size_t max_items = 0;
    size_t max_bytes = 0;
    size_t reserved_enrollment_items = 0;
    size_t reserved_evidence_items = 0;
    size_t reserved_enrollment_bytes = 0;
    size_t reserved_evidence_bytes = 0;

    bool valid() const;
  };

  struct relay_template_limits_v2
  {
    size_t max_items = 0;
    size_t max_bytes = 0;
    size_t reserved_enrollment_items = 0;
    size_t reserved_evidence_items = 0;
    size_t reserved_enrollment_bytes = 0;
    size_t reserved_evidence_bytes = 0;

    bool valid() const;
  };

  class deadline_relay_queue_v2
  {
  public:
    explicit deadline_relay_queue_v2(const relay_queue_limits_v2 &limits);
    resource_status_v2 enqueue(const relay_item_v2 &item, uint64_t current_height);
    void prune_expired(uint64_t current_height);
    resource_status_v2 select_for_template(
        uint64_t current_height,
        const relay_template_limits_v2 &limits,
        std::vector<relay_item_v2> &selected) const;
    bool erase(const crypto::hash &id);
    size_t size() const;
    size_t bytes() const;
    size_t size(relay_class_v2 record_class) const;

  private:
    relay_queue_limits_v2 limits_{};
    size_t bytes_ = 0;
    std::vector<relay_item_v2> items_;
  };


  resource_status_v2 validate_rpc_page_v2(
      uint64_t offset,
      uint64_t limit,
      uint64_t max_limit,
      uint64_t available_records,
      uint64_t max_scan,
      uint64_t &end_offset);
} // namespace epose
} // namespace qwertycoin
