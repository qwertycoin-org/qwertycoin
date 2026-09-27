// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_producer_v2.h"

#include <algorithm>
#include <limits>

namespace
{
  bool valid_key_pair(
      const crypto::public_key &public_key,
      const crypto::secret_key &secret_key)
  {
    crypto::public_key derived{};
    return crypto::secret_key_to_public_key(secret_key, derived)
        && derived == public_key;
  }

  qwertycoin::epose::service_producer_status_v2 build_admission(
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const qwertycoin::epose::identity_descriptor_v2 &descriptor,
      const uint64_t target_epoch,
      const crypto::hash &admission_context_hash,
      const uint64_t max_nonce_attempts,
      qwertycoin::epose::admission_lease_v2 &lease,
      qwertycoin::epose::envelope_record_v2 &record,
      const std::atomic<bool> *cancel)
  {
    using namespace qwertycoin::epose;
    lease = {};
    record = {};
    if (!parameters.valid() || max_nonce_attempts == 0 || target_epoch == 0
        || target_epoch < parameters.admission.context_epoch_offset
        || descriptor.effective_epoch > target_epoch
        || descriptor.expiry_epoch <= target_epoch
        || descriptor.identity_id != derive_identity_id_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash,
            descriptor.operator_authorization_public_key)
        || descriptor.service_public_key == descriptor.operator_authorization_public_key
        || !crypto::check_key(descriptor.service_public_key)
        || !crypto::check_key(descriptor.operator_authorization_public_key))
      return service_producer_status_v2::invalid_configuration;
    if (admission_context_hash == crypto::null_hash)
      return service_producer_status_v2::invalid_context;
    uint64_t context_height = 0;
    const uint64_t context_epoch =
        target_epoch - parameters.admission.context_epoch_offset;
    if (!parameters.timing.epoch_start(context_epoch, context_height))
      return service_producer_status_v2::invalid_context;

    lease.member.service_public_key = descriptor.service_public_key;
    lease.member.identity_id = descriptor.identity_id;
    lease.member.operator_authorization_public_key =
        descriptor.operator_authorization_public_key;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, descriptor.reward_address);
    lease.member.endpoint_descriptor_hash = descriptor.endpoint_descriptor_hash;
    lease.member.sequence = descriptor.sequence;
    lease.target_epoch = target_epoch;
    lease.work_algorithm = static_cast<uint8_t>(parameters.admission.algorithm);
    lease.leading_zero_bits = parameters.admission.leading_zero_bits;
    lease.admission_context_height = context_height;
    lease.admission_context_hash = admission_context_hash;
    const admission_context_v2 context{
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, context_height,
        admission_context_hash};

    bool found = false;
    for (uint64_t nonce = 0; nonce < max_nonce_attempts; ++nonce)
    {
      if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
      {
        lease = {};
        return service_producer_status_v2::cancelled;
      }
      lease.nonce = nonce;
      lease.work_hash = calculate_admission_work_v2(lease, context);
      if (admission_work_meets_target_v2(
              lease.work_hash, parameters.admission.leading_zero_bits))
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      lease = {};
      return service_producer_status_v2::admission_search_exhausted;
    }
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    if (!validate_admission_lease_v2(lease, context, parameters.admission))
    {
      lease = {};
      return service_producer_status_v2::signing_failed;
    }
    if (encode_admission_lease_record_v2(
            lease, context, parameters.admission, record)
        != record_codec_status_v2::accepted)
    {
      lease = {};
      record = {};
      return service_producer_status_v2::encoding_failed;
    }
    return service_producer_status_v2::accepted;
  }

  qwertycoin::epose::service_producer_status_v2 build_endpoint(
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const qwertycoin::epose::service_enrollment_config_v2 &configuration,
      const uint64_t sequence,
      qwertycoin::epose::endpoint_descriptor_v2 &endpoint)
  {
    using namespace qwertycoin::epose;
    endpoint = {};
    endpoint.service_public_key = configuration.keystore.service_public_key;
    endpoint.transport = configuration.endpoint_transport;
    endpoint.host = configuration.endpoint_host;
    endpoint.port = configuration.endpoint_port;
    endpoint.service_kind = parameters.committee.service_kind;
    endpoint.service_version = EPOSE_PROTOCOL_VERSION_V2;
    endpoint.sequence = sequence;
    endpoint.expiry_epoch = configuration.expiry_epoch;
    if (!sign_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, endpoint,
            configuration.keystore.service_secret_key)
        || validate_endpoint_descriptor_v2(
               parameters.nettype, parameters.genesis_hash,
               parameters.parameter_set_hash, endpoint)
            != resource_status_v2::accepted)
    {
      endpoint = {};
      return service_producer_status_v2::invalid_endpoint;
    }
    return service_producer_status_v2::accepted;
  }
}

namespace qwertycoin
{
namespace epose
{
  receipt_retry_tracker_v2::receipt_retry_tracker_v2(
      const size_t max_entries,
      const uint64_t base_backoff_ms,
      const uint64_t max_backoff_ms,
      const uint64_t resubmit_ms)
    : max_entries_(max_entries),
      base_backoff_ms_(base_backoff_ms),
      max_backoff_ms_(max_backoff_ms),
      resubmit_ms_(resubmit_ms)
  {
  }

  bool receipt_retry_tracker_v2::begin_context(
      const uint64_t epoch,
      const uint64_t round,
      const crypto::hash &anchor_hash)
  {
    if (max_entries_ == 0 || base_backoff_ms_ == 0
        || max_backoff_ms_ < base_backoff_ms_ || resubmit_ms_ == 0
        || anchor_hash == crypto::null_hash)
      return false;
    if (!context_set_ || epoch_ != epoch || round_ != round
        || anchor_hash_ != anchor_hash)
    {
      entries_.clear();
      epoch_ = epoch;
      round_ = round;
      anchor_hash_ = anchor_hash;
      context_set_ = true;
    }
    return true;
  }

  bool receipt_retry_tracker_v2::can_attempt(
      const crypto::hash &slot, const uint64_t now_ms) const
  {
    if (!context_set_ || slot == crypto::null_hash)
      return false;
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    return found == entries_.end()
        || (!found->in_flight && now_ms >= found->next_attempt_ms);
  }

  bool receipt_retry_tracker_v2::start(
      const crypto::hash &slot, const uint64_t now_ms)
  {
    if (!can_attempt(slot, now_ms))
      return false;
    auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    if (found == entries_.end())
    {
      if (entries_.size() >= max_entries_)
        return false;
      entries_.push_back({slot, now_ms, 0, true});
    }
    else
      found->in_flight = true;
    return true;
  }

  void receipt_retry_tracker_v2::failed(
      const crypto::hash &slot, const uint64_t now_ms)
  {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    if (found == entries_.end())
      return;
    found->in_flight = false;
    if (found->failures != std::numeric_limits<uint32_t>::max())
      ++found->failures;
    uint64_t delay = base_backoff_ms_;
    for (uint32_t attempt = 1;
         attempt < found->failures && delay < max_backoff_ms_; ++attempt)
      delay = std::min(max_backoff_ms_, delay > max_backoff_ms_ / 2
          ? max_backoff_ms_ : delay * 2);
    found->next_attempt_ms = now_ms > std::numeric_limits<uint64_t>::max() - delay
        ? std::numeric_limits<uint64_t>::max() : now_ms + delay;
  }

  void receipt_retry_tracker_v2::submitted(
      const crypto::hash &slot, const uint64_t now_ms)
  {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    if (found == entries_.end())
      return;
    found->in_flight = false;
    found->next_attempt_ms =
        now_ms > std::numeric_limits<uint64_t>::max() - resubmit_ms_
        ? std::numeric_limits<uint64_t>::max() : now_ms + resubmit_ms_;
  }

  void receipt_retry_tracker_v2::canonical(const crypto::hash &slot)
  {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; }),
        entries_.end());
  }

  receipt_retry_tracker_v2::retry_info receipt_retry_tracker_v2::status(
      const crypto::hash &slot) const
  {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    if (found == entries_.end())
      return {};
    return {true, found->next_attempt_ms, found->failures, found->in_flight};
  }

  size_t receipt_retry_tracker_v2::size() const
  {
    return entries_.size();
  }

  service_producer_status_v2 build_initial_service_enrollment_v2(
      const consensus_parameters_v2 &parameters,
      const service_enrollment_config_v2 &configuration,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel)
  {
    enrollment = {};
    if (!parameters.valid() || max_nonce_attempts == 0
        || configuration.target_epoch == 0
        || configuration.expiry_epoch <= configuration.target_epoch
        || configuration.keystore.operator_public_key
            == configuration.keystore.service_public_key
        || !valid_key_pair(
            configuration.keystore.operator_public_key,
            configuration.keystore.operator_secret_key)
        || !valid_key_pair(
            configuration.keystore.service_public_key,
            configuration.keystore.service_secret_key))
      return service_producer_status_v2::invalid_configuration;
    if (admission_context_hash == crypto::null_hash
        || configuration.target_epoch
            < parameters.admission.context_epoch_offset)
      return service_producer_status_v2::invalid_context;

    uint64_t context_height = 0;
    const uint64_t context_epoch =
        configuration.target_epoch - parameters.admission.context_epoch_offset;
    if (!parameters.timing.epoch_start(context_epoch, context_height))
      return service_producer_status_v2::invalid_context;

    const service_producer_status_v2 endpoint_status = build_endpoint(
        parameters, configuration, 0,
        enrollment.endpoint_update.descriptor);
    if (endpoint_status != service_producer_status_v2::accepted)
      return endpoint_status;
    enrollment.endpoint_update.present = true;

    identity_descriptor_v2 &descriptor =
        enrollment.lifecycle.next_descriptor;
    descriptor.identity_id = derive_identity_id_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash,
        configuration.keystore.operator_public_key);
    descriptor.service_public_key = configuration.keystore.service_public_key;
    descriptor.operator_authorization_public_key =
        configuration.keystore.operator_public_key;
    descriptor.reward_address = configuration.reward_address;
    descriptor.endpoint_descriptor_hash = hash_endpoint_descriptor_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, enrollment.endpoint_update.descriptor);
    descriptor.sequence = 0;
    descriptor.effective_epoch = configuration.target_epoch;
    descriptor.expiry_epoch = configuration.expiry_epoch;
    if (!sign_lifecycle_record_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, enrollment.lifecycle,
            configuration.keystore.operator_secret_key,
            configuration.keystore.service_secret_key)
        || validate_lifecycle_record_authorization_v2(
               parameters.nettype, parameters.genesis_hash,
               parameters.parameter_set_hash, enrollment.lifecycle)
            != lifecycle_status_v2::accepted)
      return service_producer_status_v2::signing_failed;

    envelope_record_v2 lifecycle_record{};
    envelope_record_v2 admission_record{};
    if (encode_lifecycle_record_v2(
            enrollment.lifecycle, parameters.nettype,
            parameters.genesis_hash, parameters.parameter_set_hash,
            lifecycle_record) != record_codec_status_v2::accepted)
    {
      enrollment = {};
      return service_producer_status_v2::encoding_failed;
    }
    const service_producer_status_v2 admission_status = build_admission(
        parameters, descriptor, configuration.target_epoch,
        admission_context_hash, max_nonce_attempts,
        enrollment.admission, admission_record, cancel);
    if (admission_status != service_producer_status_v2::accepted)
    {
      enrollment = {};
      return admission_status;
    }
    enrollment.records = {std::move(lifecycle_record), std::move(admission_record)};
    return service_producer_status_v2::accepted;
  }

  service_producer_status_v2 build_service_renewal_enrollment_v2(
      const consensus_parameters_v2 &parameters,
      const service_enrollment_config_v2 &configuration,
      const identity_descriptor_v2 &current_descriptor,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel)
  {
    enrollment = {};
    const uint64_t target_epoch = configuration.target_epoch;
    if (!parameters.valid() || target_epoch == 0
        || current_descriptor.identity_id != derive_identity_id_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash,
            configuration.keystore.operator_public_key)
        || current_descriptor.operator_authorization_public_key
            != configuration.keystore.operator_public_key
        || current_descriptor.service_public_key
            != configuration.keystore.service_public_key
        || current_descriptor.reward_address.m_view_public_key
            != configuration.reward_address.m_view_public_key
        || current_descriptor.reward_address.m_spend_public_key
            != configuration.reward_address.m_spend_public_key
        || configuration.expiry_epoch != target_epoch + 2
        || !valid_key_pair(
            configuration.keystore.operator_public_key,
            configuration.keystore.operator_secret_key)
        || !valid_key_pair(
            configuration.keystore.service_public_key,
            configuration.keystore.service_secret_key)
        || current_descriptor.sequence == std::numeric_limits<uint64_t>::max()
        || target_epoch <= current_descriptor.effective_epoch
        || target_epoch > std::numeric_limits<uint64_t>::max() - 2)
      return service_producer_status_v2::invalid_configuration;

    const service_producer_status_v2 endpoint_status = build_endpoint(
        parameters, configuration, current_descriptor.sequence + 1,
        enrollment.endpoint_update.descriptor);
    if (endpoint_status != service_producer_status_v2::accepted)
      return endpoint_status;
    enrollment.endpoint_update.present = true;

    enrollment.lifecycle.action = lifecycle_action_v2::update_descriptor;
    enrollment.lifecycle.previous_descriptor_hash = hash_identity_descriptor_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, current_descriptor);
    enrollment.lifecycle.next_descriptor = current_descriptor;
    enrollment.lifecycle.next_descriptor.sequence = current_descriptor.sequence + 1;
    enrollment.lifecycle.next_descriptor.effective_epoch = target_epoch;
    enrollment.lifecycle.next_descriptor.expiry_epoch = configuration.expiry_epoch;
    enrollment.lifecycle.next_descriptor.endpoint_descriptor_hash =
        hash_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash,
            enrollment.endpoint_update.descriptor);
    if (enrollment.lifecycle.next_descriptor.expiry_epoch
            <= current_descriptor.expiry_epoch
        || !sign_lifecycle_record_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, enrollment.lifecycle,
            configuration.keystore.operator_secret_key,
            configuration.keystore.service_secret_key)
        || validate_lifecycle_record_authorization_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, enrollment.lifecycle)
            != lifecycle_status_v2::accepted)
    {
      enrollment = {};
      return service_producer_status_v2::signing_failed;
    }

    envelope_record_v2 lifecycle_record{};
    envelope_record_v2 admission_record{};
    if (encode_lifecycle_record_v2(
            enrollment.lifecycle, parameters.nettype,
            parameters.genesis_hash, parameters.parameter_set_hash,
            lifecycle_record) != record_codec_status_v2::accepted)
    {
      enrollment = {};
      return service_producer_status_v2::encoding_failed;
    }
    const service_producer_status_v2 admission_status = build_admission(
        parameters, enrollment.lifecycle.next_descriptor, target_epoch,
        admission_context_hash, max_nonce_attempts,
        enrollment.admission, admission_record, cancel);
    if (admission_status != service_producer_status_v2::accepted)
    {
      enrollment = {};
      return admission_status;
    }
    enrollment.records = {std::move(lifecycle_record), std::move(admission_record)};
    return service_producer_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
