// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/record_codec_v2.h"

#include <cstring>

namespace
{
  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
  }

  void append_u8(std::string &out, uint8_t value) { out.push_back(static_cast<char>(value)); }
  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  template <typename T>
  bool read_bytes(const std::string &in, size_t &offset, T &value)
  {
    if (offset > in.size() || in.size() - offset < sizeof(T))
      return false;
    std::memcpy(&value, in.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
  }

  bool read_u8(const std::string &in, size_t &offset, uint8_t &value)
  {
    if (offset >= in.size())
      return false;
    value = static_cast<uint8_t>(in[offset++]);
    return true;
  }

  bool read_u64_le(const std::string &in, size_t &offset, uint64_t &value)
  {
    if (offset > in.size() || in.size() - offset < 8)
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8)
      value |= static_cast<uint64_t>(static_cast<uint8_t>(in[offset++])) << shift;
    return true;
  }
}

namespace qwertycoin
{
namespace epose
{
  record_codec_status_v2 encode_service_receipt_record_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context,
      envelope_record_v2 &record)
  {
    record = {};
    if (!validate_authenticated_service_receipt_v2(receipt, context))
      return record_codec_status_v2::invalid_record;

    std::string payload;
    payload.reserve(EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2);
    append_u8(payload, receipt.challenge.version);
    append_u8(payload, receipt.challenge.service_kind);
    append_u64_le(payload, receipt.challenge.epoch);
    append_u64_le(payload, receipt.challenge.round);
    append_bytes(payload, receipt.challenge.snapshot_hash);
    append_bytes(payload, receipt.challenge.anchor_hash);
    append_bytes(payload, receipt.challenge.subject_public_key);
    append_bytes(payload, receipt.challenge.verifier_public_key);
    append_bytes(payload, receipt.challenge.endpoint_descriptor_hash);
    append_bytes(payload, receipt.challenge.nonce);
    append_bytes(payload, receipt.challenge.requested_object_hash);
    append_bytes(payload, receipt.response_object_hash);
    append_bytes(payload, receipt.subject_signature);
    append_bytes(payload, receipt.verifier_signature);
    if (payload.size() != EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2)
      return record_codec_status_v2::wrong_size;

    record.type = static_cast<uint8_t>(record_type_v2::service_receipt);
    record.version = EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2;
    record.payload.swap(payload);
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_service_receipt_record_v2(
      const envelope_record_v2 &record,
      const receipt_context_v2 &context,
      authenticated_service_receipt_v2 &receipt)
  {
    receipt = {};
    if (record.type != static_cast<uint8_t>(record_type_v2::service_receipt))
      return record_codec_status_v2::wrong_type;
    if (record.version != EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2)
      return record_codec_status_v2::wrong_version;
    if (record.payload.size() != EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2)
      return record_codec_status_v2::wrong_size;

    authenticated_service_receipt_v2 next{};
    size_t offset = 0;
    if (!read_u8(record.payload, offset, next.challenge.version)
        || !read_u8(record.payload, offset, next.challenge.service_kind)
        || !read_u64_le(record.payload, offset, next.challenge.epoch)
        || !read_u64_le(record.payload, offset, next.challenge.round)
        || !read_bytes(record.payload, offset, next.challenge.snapshot_hash)
        || !read_bytes(record.payload, offset, next.challenge.anchor_hash)
        || !read_bytes(record.payload, offset, next.challenge.subject_public_key)
        || !read_bytes(record.payload, offset, next.challenge.verifier_public_key)
        || !read_bytes(record.payload, offset, next.challenge.endpoint_descriptor_hash)
        || !read_bytes(record.payload, offset, next.challenge.nonce)
        || !read_bytes(record.payload, offset, next.challenge.requested_object_hash)
        || !read_bytes(record.payload, offset, next.response_object_hash)
        || !read_bytes(record.payload, offset, next.subject_signature)
        || !read_bytes(record.payload, offset, next.verifier_signature)
        || offset != record.payload.size())
      return record_codec_status_v2::wrong_size;
    if (!validate_authenticated_service_receipt_v2(next, context))
      return record_codec_status_v2::invalid_record;
    receipt = next;
    return record_codec_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
