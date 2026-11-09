// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_producer_v2.h"

#include <algorithm>
#include <limits>
#include <boost/filesystem.hpp>
#include "file_io_utils.h"

namespace
{
#include "service_producer_transport_v2.h"
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
    lease.member.descriptor_version = descriptor.version;
    lease.member.endpoint_transport = descriptor.endpoint_transport;
    lease.member.dial_capabilities = descriptor.dial_capabilities;
    lease.member.dns_address_families = descriptor.dns_address_families;
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
    endpoint.version = configuration.endpoint_version;
    endpoint.service_public_key = configuration.keystore.service_public_key;
    endpoint.transport = configuration.endpoint_transport;
    endpoint.host = configuration.endpoint_host;
    endpoint.port = configuration.endpoint_port;
    endpoint.dial_capabilities = configuration.dial_capabilities;
    endpoint.dns_address_families = configuration.dns_address_families;
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

  bool valid_keystore_key_pairs(const qwertycoin::epose::service_keystore_v2 &keystore)
  {
    return valid_key_pair(keystore.operator_public_key, keystore.operator_secret_key)
        && valid_key_pair(keystore.service_public_key, keystore.service_secret_key);
  }

  qwertycoin::epose::service_producer_status_v2 encode_enrollment_records(
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const uint64_t target_epoch,
      const crypto::hash &admission_context_hash,
      const uint64_t max_nonce_attempts,
      qwertycoin::epose::service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel)
  {
    using namespace qwertycoin::epose;
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
}

namespace qwertycoin
{
namespace epose
{
  bool receipt_launch_allowed_v2(
      uint8_t block_version,
      bool has_previous_attempt,
      std::chrono::steady_clock::duration elapsed,
      std::chrono::steady_clock::duration interval)
  {
    return is_qwc_contracts_hardfork(block_version)
        || !has_previous_attempt || elapsed >= interval;
  }

  service_producer_status_v2 build_service_admission_v2(
      const consensus_parameters_v2 &parameters,
      const identity_descriptor_v2 &descriptor,
      uint64_t target_epoch,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel)
  {
    enrollment = {};
    envelope_record_v2 admission_record{};
    const service_producer_status_v2 status = build_admission(
        parameters, descriptor, target_epoch, admission_context_hash,
        max_nonce_attempts, enrollment.admission, admission_record, cancel);
    if (status != service_producer_status_v2::accepted)
    {
      enrollment = {};
      return status;
    }
    enrollment.records.push_back(std::move(admission_record));
    return service_producer_status_v2::accepted;
  }

  receipt_retry_tracker_v2::receipt_retry_tracker_v2(
      const size_t max_entries,
      const uint64_t base_backoff_ms,
      const uint64_t max_backoff_ms,
      const uint64_t resubmit_ms,
      const uint32_t max_transport_attempts)
    : max_entries_(max_entries),
      base_backoff_ms_(base_backoff_ms),
      max_backoff_ms_(max_backoff_ms),
      resubmit_ms_(resubmit_ms),
      max_transport_attempts_(max_transport_attempts)
  {
  }

  bool receipt_retry_tracker_v2::begin_context(
      const uint64_t epoch,
      const uint64_t round,
      const crypto::hash &anchor_hash)
  {
    if (max_entries_ == 0 || base_backoff_ms_ == 0
        || max_backoff_ms_ < base_backoff_ms_ || resubmit_ms_ == 0
        || max_transport_attempts_ == 0
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
        || (!found->in_flight && found->envelope.empty()
            && now_ms >= found->next_attempt_ms);
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
      entries_.push_back({slot, now_ms, 0, true, {}, 0, 0});
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
      const crypto::hash &slot, const uint64_t now_ms,
      const std::string &envelope)
  {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    if (found == entries_.end())
      return;
    found->in_flight = false;
    if (!envelope.empty())
    {
      found->envelope = envelope;
      found->transport_attempts = 1;
      found->transport_window_attempts = 1;
    }
    const uint64_t delay = found->transport_window_attempts
            >= max_transport_attempts_
        ? std::max(resubmit_ms_, max_backoff_ms_) : resubmit_ms_;
    if (found->transport_window_attempts >= max_transport_attempts_)
      found->transport_window_attempts = 0;
    found->next_attempt_ms =
        now_ms > std::numeric_limits<uint64_t>::max() - delay
        ? std::numeric_limits<uint64_t>::max() : now_ms + delay;
  }

  bool receipt_retry_tracker_v2::transport_retry(
      const crypto::hash &slot, const uint64_t now_ms,
      std::string &envelope)
  {
    envelope.clear();
    const auto found = std::find_if(entries_.begin(), entries_.end(),
        [&slot](const entry &value) { return value.slot == slot; });
    if (found == entries_.end() || found->in_flight
        || found->envelope.empty()
        || now_ms < found->next_attempt_ms)
      return false;
    envelope = found->envelope;
    if (found->transport_attempts != std::numeric_limits<uint32_t>::max())
      ++found->transport_attempts;
    if (found->transport_window_attempts != std::numeric_limits<uint32_t>::max())
      ++found->transport_window_attempts;
    const uint64_t delay = found->transport_window_attempts
            >= max_transport_attempts_
        ? std::max(resubmit_ms_, max_backoff_ms_) : resubmit_ms_;
    if (found->transport_window_attempts >= max_transport_attempts_)
      found->transport_window_attempts = 0;
    found->next_attempt_ms =
        now_ms > std::numeric_limits<uint64_t>::max() - delay
        ? std::numeric_limits<uint64_t>::max() : now_ms + delay;
    return true;
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
    return {true, found->next_attempt_ms, found->failures, found->in_flight,
        !found->envelope.empty(), found->transport_attempts};
  }

  size_t receipt_retry_tracker_v2::size() const
  {
    return entries_.size();
  }

  relay_delivery_limiter_v2::relay_delivery_limiter_v2(
      const size_t max_entries,
      const uint64_t interval_ms,
      const uint32_t max_attempts)
    : max_entries_(max_entries),
      interval_ms_(interval_ms),
      max_attempts_(max_attempts)
  {
  }

  bool relay_delivery_limiter_v2::allow(
      const std::string &delivery_key, const uint64_t now_ms)
  {
    if (delivery_key.empty() || max_entries_ == 0 || interval_ms_ == 0
        || max_attempts_ == 0)
      return false;
    auto found = std::find_if(entries_.begin(), entries_.end(),
        [&delivery_key](const entry &value) {
          return value.key == delivery_key;
        });
    if (found == entries_.end())
    {
      if (entries_.size() >= max_entries_)
      {
        const auto evict = std::min_element(
            entries_.begin(), entries_.end(),
            [](const entry &left, const entry &right) {
              return left.last_attempt_ms < right.last_attempt_ms;
            });
        if (evict == entries_.end())
          return false;
        entries_.erase(evict);
      }
      const uint64_t delay = interval_ms_;
      entries_.push_back({delivery_key,
          now_ms > std::numeric_limits<uint64_t>::max() - delay
              ? std::numeric_limits<uint64_t>::max()
              : now_ms + delay,
          now_ms, 1});
      return true;
    }
    if (now_ms < found->next_attempt_ms)
      return false;
    if (found->attempts >= max_attempts_)
      found->attempts = 0;
    ++found->attempts;
    found->last_attempt_ms = now_ms;
    uint64_t delay = interval_ms_;
    if (found->attempts >= max_attempts_)
    {
      delay = max_attempts_ > std::numeric_limits<uint64_t>::max() / interval_ms_
          ? std::numeric_limits<uint64_t>::max()
          : interval_ms_ * max_attempts_;
    }
    found->next_attempt_ms =
        delay == std::numeric_limits<uint64_t>::max()
            || now_ms > std::numeric_limits<uint64_t>::max() - delay
        ? std::numeric_limits<uint64_t>::max() : now_ms + delay;
    return true;
  }

  size_t relay_delivery_limiter_v2::size() const
  {
    return entries_.size();
  }

  std::vector<std::string> filter_envelope_delivery_v2(
      relay_delivery_limiter_v2 &limiter,
      const std::vector<std::string> &envelopes, uint64_t peer_id,
      uint64_t now_ms, bool origin)
  {
    std::vector<bool> enrollment(envelopes.size());
    std::vector<std::string> keys;
    keys.reserve(envelopes.size());
    for (size_t i = 0; i < envelopes.size(); ++i)
    {
      const auto &envelope = envelopes[i];
      const envelope_limits_v2 limits{envelope.size(), 1, envelope.size(),
          2, 1, {0, 1, 1, 1, 1, 1}};
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 budget{};
      enrollment[i] = parse_envelope_v2(envelope, limits, records, budget)
              == envelope_status_v2::accepted && records.size() == 1
          && (records.front().type == static_cast<uint8_t>(record_type_v2::identity_descriptor)
              || records.front().type == static_cast<uint8_t>(record_type_v2::descriptor_lifecycle)
              || records.front().type == static_cast<uint8_t>(record_type_v2::admission_lease));
      const auto hash = crypto::cn_fast_hash(envelope.data(), envelope.size());
      std::string key(reinterpret_cast<const char *>(&hash), sizeof(hash));
      key.append(reinterpret_cast<const char *>(&peer_id), sizeof(peer_id));
      keys.push_back(std::move(key));
    }
    bool enrollment_allowed = true;
    if (!origin && std::any_of(enrollment.begin(), enrollment.end(), [](bool value) { return value; }))
    {
      auto staged = limiter;
      for (size_t i = 0; i < envelopes.size(); ++i)
        if (enrollment[i] && !staged.allow(keys[i], now_ms))
        {
          enrollment_allowed = false;
          break;
        }
      if (enrollment_allowed)
        limiter = std::move(staged);
    }
    std::vector<std::string> delivery;
    for (size_t i = 0; i < envelopes.size(); ++i)
      if (enrollment[i] ? enrollment_allowed : limiter.allow(keys[i], now_ms))
        delivery.push_back(envelopes[i]);
    return delivery;
  }

  relay_ingress_status_v2 redeliver_pending_enrollment_v2(
      const std::string &path, const consensus_parameters_v2 &parameters,
      const std::vector<std::string> &envelopes, uint64_t height, bool confirmed,
      const std::function<relay_ingress_status_v2(const std::vector<std::string> &,
          std::vector<std::string> &)> &submit,
      const std::function<void(const std::vector<std::string> &)> &relay,
      bool &save_failed, bool &newly_accepted)
  {
    save_failed = newly_accepted = false;
    if (confirmed)
      return relay_ingress_status_v2::expired;
    std::vector<envelope_record_v2> records;
    for (const auto &envelope : envelopes)
    {
      std::vector<envelope_record_v2> decoded;
      envelope_budget_v2 budget{};
      if (parse_envelope_v2(envelope, parameters.limits.envelope, decoded, budget)
              != envelope_status_v2::accepted || decoded.size() != 1)
        return relay_ingress_status_v2::invalid_batch;
      records.push_back(decoded.front());
    }
    admission_lease_v2 admission{};
    uint64_t cutoff = 0;
    if (records.empty()
        || decode_admission_lease_record_structure_v2(records.back(), admission)
            != record_codec_status_v2::accepted
        || !parameters.timing.enrollment_cutoff(admission.target_epoch, cutoff))
      return relay_ingress_status_v2::invalid_batch;
    if (height >= cutoff)
      return relay_ingress_status_v2::expired;
    save_failed = !save_pending_enrollment_v2(path, parameters, records);
    std::vector<std::string> accepted;
    const auto status = submit(envelopes, accepted);
    if (status != relay_ingress_status_v2::accepted)
      return status;
    newly_accepted = !accepted.empty();
    // Always send the original complete set, including canonical/pending
    // prerequisites omitted from the pool's newly accepted output.
    relay(envelopes);
    return status;
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
        || !valid_keystore_key_pairs(configuration.keystore))
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
    bind_endpoint_transport_v2(descriptor, enrollment.endpoint_update.descriptor);
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

    return encode_enrollment_records(
        parameters, configuration.target_epoch, admission_context_hash,
        max_nonce_attempts, enrollment, cancel);
  }

  bool save_pending_enrollment_v2(const std::string &path,
      const consensus_parameters_v2 &parameters,
      const std::vector<envelope_record_v2> &records)
  {
    if (records.empty() || records.size() > 2)
      return false;
    std::string encoded;
    envelope_budget_v2 budget{};
    if (encode_envelope_v2(records, parameters.limits.envelope, encoded, budget)
        != envelope_status_v2::accepted)
      return false;
    if (encoded.size() > 4096)
      return false;
    std::string existing;
    if (epee::file_io_utils::load_file_to_string(path, existing, 4096) && existing == encoded)
      return true;
    const std::string temporary = path + ".tmp";
    if (!epee::file_io_utils::save_string_to_file(temporary, encoded))
      return false;
    boost::system::error_code error;
    boost::filesystem::rename(temporary, path, error);
    if (!error)
      return true;
    boost::filesystem::remove(temporary, error);
    return false;
  }

  bool load_pending_enrollment_v2(const std::string &path,
      const consensus_parameters_v2 &parameters,
      const crypto::hash &identity, uint64_t target_epoch,
      uint64_t inclusion_height, const crypto::hash &context_hash,
      std::vector<envelope_record_v2> &records)
  {
    records.clear();
    uint64_t cutoff = 0, context_height = 0;
    if (!parameters.valid() || target_epoch < parameters.admission.context_epoch_offset
        || !parameters.timing.enrollment_cutoff(target_epoch, cutoff)
        || inclusion_height > cutoff
        || !parameters.timing.epoch_start(target_epoch - parameters.admission.context_epoch_offset, context_height)
        || inclusion_height <= context_height || context_hash == crypto::null_hash)
      return false;
    std::string encoded;
    // A registration/update and admission are well below this fixed local cap.
    if (!epee::file_io_utils::load_file_to_string(path, encoded, 4096))
      return false;
    std::vector<envelope_record_v2> loaded;
    envelope_budget_v2 budget{};
    if (parse_envelope_v2(encoded, parameters.limits.envelope, loaded, budget)
            != envelope_status_v2::accepted
        || loaded.empty() || loaded.size() > 2)
      return false;
    admission_lease_v2 admission{};
    const admission_context_v2 context{parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, context_height, context_hash};
    if (decode_admission_lease_record_v2(loaded.back(), context, parameters.admission, admission)
            != record_codec_status_v2::accepted
        || admission.target_epoch != target_epoch || admission.member.identity_id != identity)
      return false;
    if (loaded.size() == 2)
    {
      lifecycle_record_v2 lifecycle{};
      if (decode_lifecycle_record_v2(loaded.front(), parameters.nettype,
              parameters.genesis_hash, parameters.parameter_set_hash, lifecycle)
              != record_codec_status_v2::accepted
          || lifecycle.next_descriptor.identity_id != identity
          || hash_identity_descriptor_v2(parameters.nettype, parameters.genesis_hash,
              parameters.parameter_set_hash, lifecycle.next_descriptor)
              != admission.member.descriptor_hash)
        return false;
    }
    records.swap(loaded);
    return true;
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
        || !valid_keystore_key_pairs(configuration.keystore)
        || current_descriptor.sequence == std::numeric_limits<uint64_t>::max()
        || target_epoch < current_descriptor.effective_epoch
        || target_epoch > std::numeric_limits<uint64_t>::max() - 2)
      return service_producer_status_v2::invalid_configuration;

    // A restart after lifecycle-only inclusion must reuse the committed
    // descriptor. A second lifecycle for the same effective epoch is invalid.
    if (target_epoch == current_descriptor.effective_epoch)
    {
      envelope_record_v2 admission_record{};
      const auto status = build_admission(parameters, current_descriptor,
          target_epoch, admission_context_hash, max_nonce_attempts,
          enrollment.admission, admission_record, cancel);
      if (status == service_producer_status_v2::accepted)
        enrollment.records.push_back(std::move(admission_record));
      else
        enrollment = {};
      return status;
    }

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
    bind_endpoint_transport_v2(enrollment.lifecycle.next_descriptor,
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

    return encode_enrollment_records(
        parameters, target_epoch, admission_context_hash,
        max_nonce_attempts, enrollment, cancel);
  }

  service_producer_status_v2 build_service_enrollment_v2(
      const consensus_parameters_v2 &parameters,
      const service_enrollment_config_v2 &configuration,
      const lifecycle_record_v2 *latest_record,
      const crypto::hash &admission_context_hash,
      uint64_t max_nonce_attempts,
      service_enrollment_v2 &enrollment,
      const std::atomic<bool> *cancel)
  {
    if (latest_record == nullptr)
      return build_initial_service_enrollment_v2(
          parameters, configuration, admission_context_hash,
          max_nonce_attempts, enrollment, cancel);
    if (latest_record->action == lifecycle_action_v2::deregister_identity)
    {
      enrollment = {};
      return service_producer_status_v2::deregistered;
    }
    const identity_descriptor_v2 &descriptor = latest_record->next_descriptor;
    if (descriptor.effective_epoch <= configuration.target_epoch
        && configuration.target_epoch < descriptor.expiry_epoch)
      return build_service_admission_v2(
          parameters, descriptor, configuration.target_epoch,
          admission_context_hash, max_nonce_attempts, enrollment, cancel);
    return build_service_renewal_enrollment_v2(
        parameters, configuration, descriptor, admission_context_hash,
        max_nonce_attempts, enrollment, cancel);
  }

  bool service_enrollment_inputs_match_v2(
      uint64_t epoch, const crypto::hash &descriptor_hash,
      const crypto::hash &context_hash, uint64_t pending_epoch,
      const crypto::hash &pending_descriptor_hash,
      const crypto::hash &pending_context_hash)
  {
    return epoch == pending_epoch
        && descriptor_hash == pending_descriptor_hash
        && context_hash == pending_context_hash;
  }
} // namespace epose
} // namespace qwertycoin
