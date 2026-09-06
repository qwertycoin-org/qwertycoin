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
    scan_limit_exceeded
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

  resource_status_v2 validate_rpc_page_v2(
      uint64_t offset,
      uint64_t limit,
      uint64_t max_limit,
      uint64_t available_records,
      uint64_t max_scan,
      uint64_t &end_offset);
} // namespace epose
} // namespace qwertycoin
