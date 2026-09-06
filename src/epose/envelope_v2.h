// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qwertycoin
{
namespace epose
{
  constexpr uint64_t EPOSE_TX_EXTRA_TAG_V2 = 0x05;
  constexpr uint8_t EPOSE_ENVELOPE_VERSION_V2 = 1;

  enum class record_type_v2 : uint8_t
  {
    identity_descriptor = 1,
    admission_lease = 2,
    service_receipt = 3,
    descriptor_lifecycle = 4,
    service_payment_proof = 5
  };

  struct envelope_record_v2
  {
    uint8_t type = 0;
    uint8_t version = 0;
    std::string payload;
  };

  struct envelope_limits_v2
  {
    size_t max_envelope_bytes = 0;
    size_t max_records = 0;
    size_t max_record_payload_bytes = 0;
    size_t max_signature_verifications = 0;
    size_t max_admission_verifications = 0;
    // Index 0 is unused. Zero means the record type is reserved but disabled.
    std::array<uint8_t, 6> supported_record_versions{};

    bool valid() const;
  };

  struct envelope_budget_v2
  {
    size_t bytes = 0;
    size_t records = 0;
    size_t signature_verifications = 0;
    size_t admission_verifications = 0;
  };

  struct block_budget_limits_v2
  {
    size_t max_epose_bytes = 0;
    size_t max_records = 0;
    size_t max_signature_verifications = 0;
    size_t max_admission_verifications = 0;

    bool valid() const;
  };

  enum class envelope_status_v2
  {
    accepted,
    invalid_limits,
    oversized_envelope,
    malformed_outer_field,
    noncanonical_varint,
    wrong_tag,
    bad_magic,
    unsupported_envelope_version,
    nonzero_flags,
    empty_envelope,
    envelope_count_exceeded,
    record_count_exceeded,
    record_count_mismatch,
    record_size_mismatch,
    truncated_record,
    unknown_record_type,
    unsupported_record_version,
    oversized_record,
    signature_budget_exceeded,
    admission_budget_exceeded,
    block_byte_budget_exceeded,
    block_record_budget_exceeded,
    block_signature_budget_exceeded,
    block_admission_budget_exceeded,
    trailing_bytes
  };

  envelope_status_v2 encode_envelope_v2(
      const std::vector<envelope_record_v2> &records,
      const envelope_limits_v2 &limits,
      std::string &encoded,
      envelope_budget_v2 &budget);

  envelope_status_v2 parse_envelope_v2(
      const std::string &encoded,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget);

  envelope_status_v2 encode_tx_extra_envelope_field_v2(
      const std::vector<envelope_record_v2> &records,
      const envelope_limits_v2 &limits,
      std::string &field,
      envelope_budget_v2 &budget);

  envelope_status_v2 parse_tx_extra_envelope_field_v2(
      const std::string &field,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget);

  envelope_status_v2 parse_transaction_envelope_fields_v2(
      const std::vector<std::string> &fields,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget);

  envelope_status_v2 charge_block_budget_v2(
      const envelope_budget_v2 &transaction_budget,
      const block_budget_limits_v2 &limits,
      envelope_budget_v2 &block_budget);
} // namespace epose
} // namespace qwertycoin
