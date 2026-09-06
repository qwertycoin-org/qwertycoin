// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "epose/envelope_v2.h"
#include "epose/membership_v2.h"
#include "epose/service_receipt_v2.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint8_t EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2 = 402;
  constexpr uint8_t EPOSE_ADMISSION_LEASE_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2 = 322;

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
} // namespace epose
} // namespace qwertycoin
