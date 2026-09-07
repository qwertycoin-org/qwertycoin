// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_producer_v2.h"

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
}

namespace qwertycoin
{
namespace epose
{
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

    enrollment.endpoint.service_public_key =
        configuration.keystore.service_public_key;
    enrollment.endpoint.transport = configuration.endpoint_transport;
    enrollment.endpoint.host = configuration.endpoint_host;
    enrollment.endpoint.port = configuration.endpoint_port;
    enrollment.endpoint.service_kind = parameters.committee.service_kind;
    enrollment.endpoint.service_version = EPOSE_PROTOCOL_VERSION_V2;
    enrollment.endpoint.sequence = 0;
    enrollment.endpoint.expiry_epoch = configuration.expiry_epoch;
    if (!sign_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, enrollment.endpoint,
            configuration.keystore.service_secret_key)
        || validate_endpoint_descriptor_v2(
               parameters.nettype, parameters.genesis_hash,
               parameters.parameter_set_hash, enrollment.endpoint)
            != resource_status_v2::accepted)
      return service_producer_status_v2::invalid_endpoint;

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
        parameters.parameter_set_hash, enrollment.endpoint);
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
      const service_keystore_v2 &keystore,
      const identity_descriptor_v2 &current_descriptor,
      uint64_t target_epoch,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel)
  {
    enrollment = {};
    if (!parameters.valid() || target_epoch == 0
        || current_descriptor.identity_id != derive_identity_id_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, keystore.operator_public_key)
        || current_descriptor.operator_authorization_public_key
            != keystore.operator_public_key
        || current_descriptor.service_public_key != keystore.service_public_key
        || !valid_key_pair(keystore.operator_public_key, keystore.operator_secret_key)
        || !valid_key_pair(keystore.service_public_key, keystore.service_secret_key)
        || current_descriptor.sequence == std::numeric_limits<uint64_t>::max()
        || target_epoch <= current_descriptor.effective_epoch
        || target_epoch > std::numeric_limits<uint64_t>::max() - 2)
      return service_producer_status_v2::invalid_configuration;

    enrollment.lifecycle.action = lifecycle_action_v2::renew_lease;
    enrollment.lifecycle.previous_descriptor_hash = hash_identity_descriptor_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, current_descriptor);
    enrollment.lifecycle.next_descriptor = current_descriptor;
    enrollment.lifecycle.next_descriptor.sequence = current_descriptor.sequence + 1;
    enrollment.lifecycle.next_descriptor.effective_epoch = target_epoch;
    enrollment.lifecycle.next_descriptor.expiry_epoch = target_epoch + 2;
    if (enrollment.lifecycle.next_descriptor.expiry_epoch
            <= current_descriptor.expiry_epoch
        || !sign_lifecycle_record_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, enrollment.lifecycle,
            keystore.operator_secret_key, keystore.service_secret_key)
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
