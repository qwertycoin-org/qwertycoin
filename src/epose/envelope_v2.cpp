// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/envelope_v2.h"

#include <limits>
#include <utility>

namespace
{
  using qwertycoin::epose::envelope_budget_v2;
  using qwertycoin::epose::envelope_limits_v2;
  using qwertycoin::epose::envelope_status_v2;
  using qwertycoin::epose::record_type_v2;

  constexpr size_t ENVELOPE_HEADER_SIZE = 12;
  constexpr size_t RECORD_HEADER_SIZE = 8;

  void append_u16_le(std::string &out, uint16_t value)
  {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
  }

  void append_u32_le(std::string &out, uint32_t value)
  {
    for (unsigned shift = 0; shift < 32; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  bool read_u16_le(const std::string &in, size_t &offset, uint16_t &value)
  {
    if (offset > in.size() || in.size() - offset < 2)
      return false;
    value = static_cast<uint8_t>(in[offset])
        | static_cast<uint16_t>(static_cast<uint8_t>(in[offset + 1])) << 8;
    offset += 2;
    return true;
  }

  bool read_u32_le(const std::string &in, size_t &offset, uint32_t &value)
  {
    if (offset > in.size() || in.size() - offset < 4)
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
      value |= static_cast<uint32_t>(static_cast<uint8_t>(in[offset++])) << shift;
    return true;
  }

  void append_varint(std::string &out, uint64_t value)
  {
    do
    {
      uint8_t byte = value & 0x7f;
      value >>= 7;
      if (value != 0)
        byte |= 0x80;
      out.push_back(static_cast<char>(byte));
    } while (value != 0);
  }

  bool read_canonical_varint(const std::string &in, size_t &offset, uint64_t &value, bool &canonical)
  {
    const size_t start = offset;
    value = 0;
    canonical = false;
    for (unsigned index = 0; index < 10; ++index)
    {
      if (offset >= in.size())
        return false;
      const uint8_t byte = static_cast<uint8_t>(in[offset++]);
      if (index == 9 && (byte & 0xfe) != 0)
        return false;
      value |= static_cast<uint64_t>(byte & 0x7f) << (index * 7);
      if ((byte & 0x80) == 0)
      {
        std::string encoded;
        append_varint(encoded, value);
        canonical = encoded.size() == offset - start;
        return true;
      }
    }
    return false;
  }

  bool checked_add(size_t left, size_t right, size_t &out)
  {
    if (left > std::numeric_limits<size_t>::max() - right)
      return false;
    out = left + right;
    return true;
  }

  bool add_cost(uint8_t type, envelope_budget_v2 &budget)
  {
    switch (static_cast<record_type_v2>(type))
    {
      case record_type_v2::identity_descriptor:
        ++budget.signature_verifications;
        return true;
      case record_type_v2::admission_lease:
        ++budget.signature_verifications;
        ++budget.admission_verifications;
        return true;
      case record_type_v2::service_receipt:
        budget.signature_verifications += 2;
        return true;
      case record_type_v2::descriptor_lifecycle:
        budget.signature_verifications += 2;
        return true;
      case record_type_v2::service_payment_proof:
        ++budget.signature_verifications;
        return true;
    }
    return false;
  }

  envelope_status_v2 validate_record(
      const qwertycoin::epose::envelope_record_v2 &record,
      const envelope_limits_v2 &limits,
      envelope_budget_v2 &budget)
  {
    if (record.type == 0 || record.type >= limits.supported_record_versions.size())
      return envelope_status_v2::unknown_record_type;
    const uint8_t supported = limits.supported_record_versions[record.type];
    if (supported == 0 || record.version != supported)
      return envelope_status_v2::unsupported_record_version;
    if (record.payload.size() > limits.max_record_payload_bytes)
      return envelope_status_v2::oversized_record;
    if (!add_cost(record.type, budget))
      return envelope_status_v2::unknown_record_type;
    if (budget.signature_verifications > limits.max_signature_verifications)
      return envelope_status_v2::signature_budget_exceeded;
    if (budget.admission_verifications > limits.max_admission_verifications)
      return envelope_status_v2::admission_budget_exceeded;
    return envelope_status_v2::accepted;
  }
}

namespace qwertycoin
{
namespace epose
{
  bool envelope_limits_v2::valid() const
  {
    return max_envelope_bytes >= ENVELOPE_HEADER_SIZE
        && max_records > 0
        && max_record_payload_bytes > 0
        && max_signature_verifications > 0
        && max_admission_verifications > 0;
  }

  bool block_budget_limits_v2::valid() const
  {
    return max_epose_bytes > 0
        && max_records > 0
        && max_signature_verifications > 0
        && max_admission_verifications > 0;
  }

  envelope_status_v2 encode_envelope_v2(
      const std::vector<envelope_record_v2> &records,
      const envelope_limits_v2 &limits,
      std::string &encoded,
      envelope_budget_v2 &budget)
  {
    encoded.clear();
    budget = {};
    if (!limits.valid())
      return envelope_status_v2::invalid_limits;
    if (records.empty())
      return envelope_status_v2::empty_envelope;
    if (records.size() > limits.max_records || records.size() > std::numeric_limits<uint16_t>::max())
      return envelope_status_v2::record_count_exceeded;

    size_t records_size = 0;
    for (const envelope_record_v2 &record : records)
    {
      const envelope_status_v2 status = validate_record(record, limits, budget);
      if (status != envelope_status_v2::accepted)
        return status;
      size_t record_size = 0;
      if (!checked_add(RECORD_HEADER_SIZE, record.payload.size(), record_size)
          || !checked_add(records_size, record_size, records_size)
          || records_size > std::numeric_limits<uint32_t>::max())
        return envelope_status_v2::oversized_envelope;
    }
    size_t total_size = 0;
    if (!checked_add(ENVELOPE_HEADER_SIZE, records_size, total_size)
        || total_size > limits.max_envelope_bytes)
      return envelope_status_v2::oversized_envelope;

    encoded.append("QEP2", 4);
    encoded.push_back(static_cast<char>(EPOSE_ENVELOPE_VERSION_V2));
    encoded.push_back(0);
    append_u16_le(encoded, static_cast<uint16_t>(records.size()));
    append_u32_le(encoded, static_cast<uint32_t>(records_size));
    for (const envelope_record_v2 &record : records)
    {
      encoded.push_back(static_cast<char>(record.type));
      encoded.push_back(static_cast<char>(record.version));
      append_u16_le(encoded, 0);
      append_u32_le(encoded, static_cast<uint32_t>(record.payload.size()));
      encoded.append(record.payload);
    }
    budget.bytes = encoded.size();
    budget.records = records.size();
    return envelope_status_v2::accepted;
  }

  envelope_status_v2 parse_envelope_v2(
      const std::string &encoded,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget)
  {
    records.clear();
    budget = {};
    if (!limits.valid())
      return envelope_status_v2::invalid_limits;
    if (encoded.size() > limits.max_envelope_bytes)
      return envelope_status_v2::oversized_envelope;
    if (encoded.size() < ENVELOPE_HEADER_SIZE)
      return envelope_status_v2::malformed_outer_field;
    if (encoded.compare(0, 4, "QEP2") != 0)
      return envelope_status_v2::bad_magic;
    if (static_cast<uint8_t>(encoded[4]) != EPOSE_ENVELOPE_VERSION_V2)
      return envelope_status_v2::unsupported_envelope_version;
    if (static_cast<uint8_t>(encoded[5]) != 0)
      return envelope_status_v2::nonzero_flags;

    size_t offset = 6;
    uint16_t record_count = 0;
    uint32_t records_size = 0;
    if (!read_u16_le(encoded, offset, record_count) || !read_u32_le(encoded, offset, records_size))
      return envelope_status_v2::malformed_outer_field;
    if (record_count > limits.max_records)
      return envelope_status_v2::record_count_exceeded;
    if (record_count == 0)
      return envelope_status_v2::empty_envelope;
    if (records_size != encoded.size() - offset)
      return envelope_status_v2::record_size_mismatch;
    if (record_count > records_size / RECORD_HEADER_SIZE)
      return envelope_status_v2::record_count_mismatch;

    records.reserve(record_count);
    for (uint16_t index = 0; index < record_count; ++index)
    {
      if (offset > encoded.size() || encoded.size() - offset < RECORD_HEADER_SIZE)
        return envelope_status_v2::truncated_record;
      envelope_record_v2 record{};
      record.type = static_cast<uint8_t>(encoded[offset++]);
      record.version = static_cast<uint8_t>(encoded[offset++]);
      uint16_t flags = 0;
      uint32_t payload_size = 0;
      if (!read_u16_le(encoded, offset, flags) || !read_u32_le(encoded, offset, payload_size))
        return envelope_status_v2::truncated_record;
      if (flags != 0)
        return envelope_status_v2::nonzero_flags;
      if (payload_size > limits.max_record_payload_bytes)
        return envelope_status_v2::oversized_record;
      if (offset > encoded.size() || encoded.size() - offset < payload_size)
        return envelope_status_v2::truncated_record;
      record.payload.assign(encoded.data() + offset, payload_size);
      offset += payload_size;
      const envelope_status_v2 status = validate_record(record, limits, budget);
      if (status != envelope_status_v2::accepted)
        return status;
      records.push_back(std::move(record));
    }
    if (records.size() != record_count)
      return envelope_status_v2::record_count_mismatch;
    if (offset != encoded.size())
      return envelope_status_v2::trailing_bytes;
    budget.bytes = encoded.size();
    budget.records = records.size();
    return envelope_status_v2::accepted;
  }

  envelope_status_v2 encode_tx_extra_envelope_field_v2(
      const std::vector<envelope_record_v2> &records,
      const envelope_limits_v2 &limits,
      std::string &field,
      envelope_budget_v2 &budget)
  {
    std::string envelope;
    const envelope_status_v2 status = encode_envelope_v2(records, limits, envelope, budget);
    if (status != envelope_status_v2::accepted)
      return status;
    field.clear();
    append_varint(field, EPOSE_TX_EXTRA_TAG_V2);
    append_varint(field, envelope.size());
    field.append(envelope);
    budget.bytes = field.size();
    return envelope_status_v2::accepted;
  }

  envelope_status_v2 parse_tx_extra_envelope_field_v2(
      const std::string &field,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget)
  {
    records.clear();
    budget = {};
    if (!limits.valid())
      return envelope_status_v2::invalid_limits;
    size_t offset = 0;
    uint64_t tag = 0;
    uint64_t envelope_size = 0;
    bool canonical = false;
    if (!read_canonical_varint(field, offset, tag, canonical))
      return envelope_status_v2::malformed_outer_field;
    if (!canonical)
      return envelope_status_v2::noncanonical_varint;
    if (tag != EPOSE_TX_EXTRA_TAG_V2)
      return envelope_status_v2::wrong_tag;
    if (!read_canonical_varint(field, offset, envelope_size, canonical))
      return envelope_status_v2::malformed_outer_field;
    if (!canonical)
      return envelope_status_v2::noncanonical_varint;
    if (envelope_size > limits.max_envelope_bytes)
      return envelope_status_v2::oversized_envelope;
    if (envelope_size != field.size() - offset)
      return envelope_status_v2::malformed_outer_field;
    const envelope_status_v2 status = parse_envelope_v2(field.substr(offset), limits, records, budget);
    if (status == envelope_status_v2::accepted)
      budget.bytes = field.size();
    return status;
  }

  envelope_status_v2 parse_transaction_envelope_fields_v2(
      const std::vector<std::string> &fields,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget)
  {
    records.clear();
    budget = {};
    if (!limits.valid() || max_envelopes_per_transaction == 0)
      return envelope_status_v2::invalid_limits;
    if (fields.size() > max_envelopes_per_transaction)
      return envelope_status_v2::envelope_count_exceeded;
    for (const std::string &field : fields)
    {
      std::vector<envelope_record_v2> parsed;
      envelope_budget_v2 field_budget{};
      const envelope_status_v2 status = parse_tx_extra_envelope_field_v2(field, limits, parsed, field_budget);
      if (status != envelope_status_v2::accepted)
        return status;
      records.insert(records.end(), parsed.begin(), parsed.end());
      if (!checked_add(budget.bytes, field_budget.bytes, budget.bytes))
        return envelope_status_v2::oversized_envelope;
      if (!checked_add(budget.records, field_budget.records, budget.records))
        return envelope_status_v2::record_count_exceeded;
      if (!checked_add(budget.signature_verifications, field_budget.signature_verifications, budget.signature_verifications))
        return envelope_status_v2::signature_budget_exceeded;
      if (!checked_add(budget.admission_verifications, field_budget.admission_verifications, budget.admission_verifications))
        return envelope_status_v2::admission_budget_exceeded;
    }
    return envelope_status_v2::accepted;
  }

  envelope_status_v2 charge_block_budget_v2(
      const envelope_budget_v2 &transaction_budget,
      const block_budget_limits_v2 &limits,
      envelope_budget_v2 &block_budget)
  {
    if (!limits.valid())
      return envelope_status_v2::invalid_limits;
    envelope_budget_v2 next = block_budget;
    if (!checked_add(next.bytes, transaction_budget.bytes, next.bytes)
        || next.bytes > limits.max_epose_bytes)
      return envelope_status_v2::block_byte_budget_exceeded;
    if (!checked_add(next.records, transaction_budget.records, next.records)
        || next.records > limits.max_records)
      return envelope_status_v2::block_record_budget_exceeded;
    if (!checked_add(next.signature_verifications, transaction_budget.signature_verifications, next.signature_verifications)
        || next.signature_verifications > limits.max_signature_verifications)
      return envelope_status_v2::block_signature_budget_exceeded;
    if (!checked_add(next.admission_verifications, transaction_budget.admission_verifications, next.admission_verifications)
        || next.admission_verifications > limits.max_admission_verifications)
      return envelope_status_v2::block_admission_budget_exceeded;
    block_budget = next;
    return envelope_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
