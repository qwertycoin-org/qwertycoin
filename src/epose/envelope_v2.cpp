// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/envelope_v2.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"

#include <algorithm>
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

  size_t varint_size(uint64_t value)
  {
    size_t result = 1;
    while (value >= 0x80)
    {
      value >>= 7;
      ++result;
    }
    return result;
  }

  bool add_cost(uint8_t type, envelope_budget_v2 &budget)
  {
    switch (static_cast<record_type_v2>(type))
    {
      case record_type_v2::identity_descriptor:
        budget.signature_verifications += 2;
        return true;
      case record_type_v2::admission_lease:
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

    envelope_budget_v2 next_budget{};
    size_t records_size = 0;
    for (const envelope_record_v2 &record : records)
    {
      const envelope_status_v2 status = validate_record(record, limits, next_budget);
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

    std::string next_encoded;
    next_encoded.append("QEP2", 4);
    next_encoded.push_back(static_cast<char>(EPOSE_ENVELOPE_VERSION_V2));
    next_encoded.push_back(0);
    append_u16_le(next_encoded, static_cast<uint16_t>(records.size()));
    append_u32_le(next_encoded, static_cast<uint32_t>(records_size));
    for (const envelope_record_v2 &record : records)
    {
      next_encoded.push_back(static_cast<char>(record.type));
      next_encoded.push_back(static_cast<char>(record.version));
      append_u16_le(next_encoded, 0);
      append_u32_le(next_encoded, static_cast<uint32_t>(record.payload.size()));
      next_encoded.append(record.payload);
    }
    next_budget.bytes = next_encoded.size();
    next_budget.records = records.size();
    encoded.swap(next_encoded);
    budget = next_budget;
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

    std::vector<envelope_record_v2> next_records;
    envelope_budget_v2 next_budget{};
    next_records.reserve(record_count);
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
      const envelope_status_v2 status = validate_record(record, limits, next_budget);
      if (status != envelope_status_v2::accepted)
        return status;
      next_records.push_back(std::move(record));
    }
    if (next_records.size() != record_count)
      return envelope_status_v2::record_count_mismatch;
    if (offset != encoded.size())
      return envelope_status_v2::trailing_bytes;
    next_budget.bytes = encoded.size();
    next_budget.records = next_records.size();
    records.swap(next_records);
    budget = next_budget;
    return envelope_status_v2::accepted;
  }

  envelope_status_v2 encode_tx_extra_envelope_field_v2(
      const std::vector<envelope_record_v2> &records,
      const envelope_limits_v2 &limits,
      std::string &field,
      envelope_budget_v2 &budget)
  {
    field.clear();
    budget = {};
    std::string envelope;
    envelope_budget_v2 next_budget{};
    const envelope_status_v2 status = encode_envelope_v2(records, limits, envelope, next_budget);
    if (status != envelope_status_v2::accepted)
      return status;
    std::string next_field;
    append_varint(next_field, EPOSE_TX_EXTRA_TAG_V2);
    append_varint(next_field, envelope.size());
    next_field.append(envelope);
    next_budget.bytes = next_field.size();
    field.swap(next_field);
    budget = next_budget;
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
    std::vector<envelope_record_v2> next_records;
    envelope_budget_v2 next_budget{};
    const envelope_status_v2 status = parse_envelope_v2(field.substr(offset), limits, next_records, next_budget);
    if (status != envelope_status_v2::accepted)
      return status;
    next_budget.bytes = field.size();
    records.swap(next_records);
    budget = next_budget;
    return envelope_status_v2::accepted;
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
    std::vector<envelope_record_v2> next_records;
    envelope_budget_v2 next_budget{};
    for (const std::string &field : fields)
    {
      std::vector<envelope_record_v2> parsed;
      envelope_budget_v2 field_budget{};
      const envelope_status_v2 status = parse_tx_extra_envelope_field_v2(field, limits, parsed, field_budget);
      if (status != envelope_status_v2::accepted)
        return status;
      next_records.insert(next_records.end(), parsed.begin(), parsed.end());
      if (!checked_add(next_budget.bytes, field_budget.bytes, next_budget.bytes))
        return envelope_status_v2::oversized_envelope;
      if (!checked_add(next_budget.records, field_budget.records, next_budget.records))
        return envelope_status_v2::record_count_exceeded;
      if (!checked_add(next_budget.signature_verifications, field_budget.signature_verifications, next_budget.signature_verifications))
        return envelope_status_v2::signature_budget_exceeded;
      if (!checked_add(next_budget.admission_verifications, field_budget.admission_verifications, next_budget.admission_verifications))
        return envelope_status_v2::admission_budget_exceeded;
    }
    records.swap(next_records);
    budget = next_budget;
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

  envelope_status_v2 parse_transaction_extra_v2(
      const std::vector<uint8_t> &tx_extra,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      std::vector<envelope_record_v2> &records,
      envelope_budget_v2 &budget)
  {
    records.clear();
    budget = {};
    if (major_version != HF_VERSION_QWC_EPOSE_V2)
      return envelope_status_v2::inactive_protocol;
    if (!limits.valid() || max_envelopes_per_transaction == 0)
      return envelope_status_v2::invalid_limits;

    std::vector<cryptonote::tx_extra_field> fields;
    if (!cryptonote::parse_tx_extra_strict(tx_extra, fields, true))
      return envelope_status_v2::malformed_transaction_extra;

    std::vector<std::string> envelope_fields;
    for (const cryptonote::tx_extra_field &field : fields)
    {
      if (field.type() != typeid(cryptonote::tx_extra_epose_v2))
        continue;
      const auto &epose = boost::get<cryptonote::tx_extra_epose_v2>(field);
      std::string framed;
      append_varint(framed, EPOSE_TX_EXTRA_TAG_V2);
      append_varint(framed, epose.data.size());
      framed.append(epose.data);
      envelope_fields.push_back(std::move(framed));
    }
    if (envelope_fields.size() > max_envelopes_per_transaction)
      return envelope_status_v2::envelope_count_exceeded;
    if (envelope_fields.empty())
      return envelope_status_v2::accepted;
    return parse_transaction_envelope_fields_v2(
        envelope_fields, max_envelopes_per_transaction, limits, records, budget);
  }

  envelope_status_v2 append_transaction_envelope_v2(
      const std::vector<envelope_record_v2> &records,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      std::vector<uint8_t> &tx_extra,
      envelope_budget_v2 &budget)
  {
    budget = {};
    if (major_version != HF_VERSION_QWC_EPOSE_V2)
      return envelope_status_v2::inactive_protocol;
    if (!limits.valid() || max_envelopes_per_transaction == 0)
      return envelope_status_v2::invalid_limits;

    std::vector<envelope_record_v2> existing_records;
    envelope_budget_v2 existing_budget{};
    const envelope_status_v2 existing_status = parse_transaction_extra_v2(
        tx_extra, major_version, max_envelopes_per_transaction, limits,
        existing_records, existing_budget);
    if (existing_status != envelope_status_v2::accepted)
      return existing_status;

    std::vector<cryptonote::tx_extra_field> fields;
    if (!cryptonote::parse_tx_extra_strict(tx_extra, fields, true))
      return envelope_status_v2::malformed_transaction_extra;
    const size_t existing_envelopes = std::count_if(fields.begin(), fields.end(), [](const cryptonote::tx_extra_field &field) {
      return field.type() == typeid(cryptonote::tx_extra_epose_v2);
    });
    if (existing_envelopes >= max_envelopes_per_transaction)
      return envelope_status_v2::envelope_count_exceeded;

    std::string envelope;
    envelope_budget_v2 next_budget{};
    const envelope_status_v2 encode_status = encode_envelope_v2(records, limits, envelope, next_budget);
    if (encode_status != envelope_status_v2::accepted)
      return encode_status;

    std::vector<uint8_t> next_extra = tx_extra;
    if (!cryptonote::add_epose_v2_to_tx_extra(next_extra, envelope))
      return envelope_status_v2::malformed_transaction_extra;
    next_budget.bytes += 1 + varint_size(envelope.size());
    tx_extra.swap(next_extra);
    budget = next_budget;
    return envelope_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
