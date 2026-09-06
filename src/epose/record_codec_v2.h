// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "epose/envelope_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/membership_v2.h"
#include "epose/reward_v2.h"
#include "epose/service_receipt_v2.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint8_t EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2 = 402;
  constexpr uint8_t EPOSE_ADMISSION_LEASE_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2 = 322;
  constexpr uint8_t EPOSE_LIFECYCLE_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_LIFECYCLE_PAYLOAD_BYTES_V2 = 378;
  constexpr uint8_t EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2 = 96;

  enum class record_codec_status_v2
  {
    accepted,
    wrong_type,
    wrong_version,
    wrong_size,
    invalid_record
  };

  record_codec_status_v2 encode_service_receipt_record_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context,
      envelope_record_v2 &record);

  record_codec_status_v2 decode_service_receipt_record_v2(
      const envelope_record_v2 &record,
      const receipt_context_v2 &context,
      authenticated_service_receipt_v2 &receipt);

  record_codec_status_v2 encode_admission_lease_record_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      envelope_record_v2 &record);

  record_codec_status_v2 decode_admission_lease_record_v2(
      const envelope_record_v2 &record,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      admission_lease_v2 &lease);

  record_codec_status_v2 encode_lifecycle_record_v2(
      const lifecycle_record_v2 &lifecycle,
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      envelope_record_v2 &record);

  record_codec_status_v2 decode_lifecycle_record_v2(
      const envelope_record_v2 &record,
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      lifecycle_record_v2 &lifecycle);

  record_codec_status_v2 encode_payment_proof_record_v2(
      const scoped_payment_proof_v2 &proof,
      const service_payment_context_v2 &context,
      envelope_record_v2 &record);

  record_codec_status_v2 decode_payment_proof_record_v2(
      const envelope_record_v2 &record,
      const service_payment_context_v2 &context,
      scoped_payment_proof_v2 &proof);
} // namespace epose
} // namespace qwertycoin
