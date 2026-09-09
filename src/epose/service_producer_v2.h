// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <atomic>
#include <cstddef>
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
    struct endpoint_update_v2
    {
      bool present = false;
      endpoint_descriptor_v2 descriptor{};
    };

    endpoint_update_v2 endpoint_update{};
    lifecycle_record_v2 lifecycle{};
    admission_lease_v2 admission{};
    std::vector<envelope_record_v2> records;
  };

  // Bounded, non-consensus retry state for authenticated receipt production.
  // Slot identities already include epoch, round, subject, verifier and anchor.
  // A context change therefore clears stale work after a round transition or
  // reorg, while a missing canonical receipt becomes retryable after backoff.
  class receipt_retry_tracker_v2
  {
  public:
    receipt_retry_tracker_v2(
        size_t max_entries = 4096,
        uint64_t base_backoff_ms = 30000,
        uint64_t max_backoff_ms = 120000,
        uint64_t resubmit_ms = 60000);

    bool begin_context(uint64_t epoch, uint64_t round,
        const crypto::hash &anchor_hash);
    bool can_attempt(const crypto::hash &slot, uint64_t now_ms) const;
    bool start(const crypto::hash &slot, uint64_t now_ms);
    void failed(const crypto::hash &slot, uint64_t now_ms);
    void submitted(const crypto::hash &slot, uint64_t now_ms);
    void canonical(const crypto::hash &slot);
    size_t size() const;

  private:
    struct entry
    {
      crypto::hash slot{};
      uint64_t next_attempt_ms = 0;
      uint32_t failures = 0;
      bool in_flight = false;
    };

    size_t max_entries_ = 0;
    uint64_t base_backoff_ms_ = 0;
    uint64_t max_backoff_ms_ = 0;
    uint64_t resubmit_ms_ = 0;
    uint64_t epoch_ = 0;
    uint64_t round_ = 0;
    crypto::hash anchor_hash_{};
    bool context_set_ = false;
    std::vector<entry> entries_;
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

  service_producer_status_v2 build_service_renewal_enrollment_v2(
      const consensus_parameters_v2 &parameters,
      const service_enrollment_config_v2 &configuration,
      const identity_descriptor_v2 &current_descriptor,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel = nullptr);
} // namespace epose
} // namespace qwertycoin
