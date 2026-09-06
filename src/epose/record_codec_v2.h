// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "epose/envelope_v2.h"
#include "epose/service_receipt_v2.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint8_t EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2 = 1;
  constexpr size_t EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2 = 402;

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
} // namespace epose
} // namespace qwertycoin
