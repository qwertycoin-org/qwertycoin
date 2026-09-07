// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "epose/coordinator_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/resource_policy_v2.h"
#include "epose/service_keystore_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct service_enrollment_config_v2
  {
    service_keystore_v2 keystore{};
    cryptonote::account_public_address reward_address{};
    endpoint_transport_v2 endpoint_transport = endpoint_transport_v2::tcp_ipv4;
    std::string endpoint_host;
    uint16_t endpoint_port = 0;
    uint64_t target_epoch = 0;
    uint64_t expiry_epoch = 0;
  };

  enum class service_producer_status_v2
  {
    accepted,
    invalid_configuration,
    invalid_context,
    invalid_endpoint,
    signing_failed,
    admission_search_exhausted,
    cancelled,
    encoding_failed
  };

  struct service_enrollment_v2
  {
    endpoint_descriptor_v2 endpoint{};
    lifecycle_record_v2 lifecycle{};
    admission_lease_v2 admission{};
    std::vector<envelope_record_v2> records;
  };

  // Builds the canonical first registration and admission for one target
  // epoch. The caller must supply the exact canonical block hash at the
  // policy-derived historical context height. RandomX is never bypassed.
  service_producer_status_v2 build_initial_service_enrollment_v2(
      const consensus_parameters_v2 &parameters,
      const service_enrollment_config_v2 &configuration,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel = nullptr);
} // namespace epose
} // namespace qwertycoin
