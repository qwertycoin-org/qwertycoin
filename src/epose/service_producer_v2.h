// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include "epose/relay_pool_v2.h"
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
  bool receipt_launch_allowed_v2(
      uint8_t block_version,
      bool has_previous_attempt,
      std::chrono::steady_clock::duration elapsed,
      std::chrono::steady_clock::duration interval);

  // Non-consensus launch-worker bound.  At the 18-bit launch target this is
  // 64 times the mean search space, leaving a negligible exhaustion tail
  // while the asynchronous worker remains cancellable at epoch/reorg changes.
  constexpr uint64_t EPOSE_V2_LAUNCH_ADMISSION_MAX_NONCE_ATTEMPTS =
      UINT64_C(1) << 24;

  struct service_enrollment_config_v2
  {
    service_keystore_v2 keystore{};
    cryptonote::account_public_address reward_address{};
    endpoint_transport_v2 endpoint_transport = endpoint_transport_v2::tcp_ipv4;
    std::string endpoint_host;
    uint16_t endpoint_port = 0;
    uint8_t endpoint_version = 1;
    uint8_t dial_capabilities = 1;
    uint8_t dns_address_families = 0;
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
    encoding_failed,
    deregistered
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

  service_producer_status_v2 build_service_admission_v2(
      const consensus_parameters_v2 &parameters,
      const identity_descriptor_v2 &descriptor,
      uint64_t target_epoch,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel = nullptr);

  // Public signed work only; bounded parsing on load. The caller must still
  // validate against canonical lifecycle state before relaying restored work.
  bool save_pending_enrollment_v2(const std::string &path,
      const consensus_parameters_v2 &parameters,
      const std::vector<envelope_record_v2> &records);
  bool load_pending_enrollment_v2(const std::string &path,
      const consensus_parameters_v2 &parameters,
      const crypto::hash &identity, uint64_t target_epoch,
      uint64_t inclusion_height, const crypto::hash &context_hash,
      std::vector<envelope_record_v2> &records);

  // Bounded, non-consensus retry state for authenticated receipt production.
  // Slot identities already include epoch, round, subject, verifier and anchor.
  // A context change therefore clears stale work after a round transition or
  // reorg, while a missing canonical receipt becomes retryable after backoff.
  class receipt_retry_tracker_v2
  {
  public:
    struct retry_info
    {
      bool found = false;
      uint64_t next_attempt_ms = 0;
      uint32_t failures = 0;
      bool in_flight = false;
      bool has_envelope = false;
      uint32_t transport_attempts = 0;
    };

    receipt_retry_tracker_v2(
        size_t max_entries = 4096,
        uint64_t base_backoff_ms = 30000,
        uint64_t max_backoff_ms = 120000,
        uint64_t resubmit_ms = 60000,
        uint32_t max_transport_attempts = 8);

    bool begin_context(uint64_t epoch, uint64_t round,
        const crypto::hash &anchor_hash);
    bool can_attempt(const crypto::hash &slot, uint64_t now_ms) const;
    bool start(const crypto::hash &slot, uint64_t now_ms);
    void failed(const crypto::hash &slot, uint64_t now_ms);
    void submitted(const crypto::hash &slot, uint64_t now_ms,
        const std::string &envelope = {});
    bool transport_retry(const crypto::hash &slot, uint64_t now_ms,
        std::string &envelope);
    void canonical(const crypto::hash &slot);
    retry_info status(const crypto::hash &slot) const;
    size_t size() const;

  private:
    struct entry
    {
      crypto::hash slot{};
      uint64_t next_attempt_ms = 0;
      uint32_t failures = 0;
      bool in_flight = false;
      std::string envelope;
      uint32_t transport_attempts = 0;
      uint32_t transport_window_attempts = 0;
    };

    size_t max_entries_ = 0;
    uint64_t base_backoff_ms_ = 0;
    uint64_t max_backoff_ms_ = 0;
    uint64_t resubmit_ms_ = 0;
    uint32_t max_transport_attempts_ = 0;
    uint64_t epoch_ = 0;
    uint64_t round_ = 0;
    crypto::hash anchor_hash_{};
    bool context_set_ = false;
    std::vector<entry> entries_;
  };

  // Per-peer transport limiter for re-delivering an already authenticated
  // envelope. The caller's key must use stable peer identity rather than a
  // connection UUID so reconnects cannot reset the bounded retry budget.
  class relay_delivery_limiter_v2
  {
  public:
    relay_delivery_limiter_v2(
        size_t max_entries = 16384,
        uint64_t interval_ms = 60000,
        uint32_t max_attempts = 8);

    bool allow(const std::string &delivery_key, uint64_t now_ms);
    size_t size() const;

  private:
    struct entry
    {
      std::string key;
      uint64_t next_attempt_ms = 0;
      uint64_t last_attempt_ms = 0;
      uint32_t attempts = 0;
    };

    size_t max_entries_ = 0;
    uint64_t interval_ms_ = 0;
    uint32_t max_attempts_ = 0;
    std::vector<entry> entries_;
  };

  // Origin enrollment stays complete on its producer's retry clock. All other
  // deliveries are keyed per envelope and peer. If an intermediary filters an
  // enrollment prerequisite, withhold the enrollment suffix; never emit lone A
  // after dropping its received L. Receipts are independent of that decision.
  std::vector<std::string> filter_envelope_delivery_v2(relay_delivery_limiter_v2 &limiter,
      const std::vector<std::string> &envelopes, uint64_t peer_id,
      uint64_t now_ms, bool origin);

  // One origin tick, using the same submission and transport callbacks as core.
  // Persistence failure is reported independently of admission and delivery.
  relay_ingress_status_v2 redeliver_pending_enrollment_v2(
      const std::string &path, const consensus_parameters_v2 &parameters,
      const std::vector<std::string> &envelopes, uint64_t height, bool confirmed,
      const std::function<relay_ingress_status_v2(const std::vector<std::string> &,
          std::vector<std::string> &)> &submit,
      const std::function<void(const std::vector<std::string> &)> &relay,
      bool &save_failed, bool &newly_accepted);

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

  service_producer_status_v2 build_service_enrollment_v2(
      const consensus_parameters_v2 &parameters,
      const service_enrollment_config_v2 &configuration,
      const lifecycle_record_v2 *latest_record,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel = nullptr);

  bool service_enrollment_inputs_match_v2(
      uint64_t epoch, const crypto::hash &descriptor_hash,
      const crypto::hash &context_hash, uint64_t pending_epoch,
      const crypto::hash &pending_descriptor_hash,
      const crypto::hash &pending_context_hash);
} // namespace epose
} // namespace qwertycoin
