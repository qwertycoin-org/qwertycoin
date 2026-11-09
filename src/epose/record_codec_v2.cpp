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

  template <typename T>
  void append_le(std::string &out, T value)
  {
    for (unsigned shift = 0; shift < 8 * sizeof(T); shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_u64_le(std::string &out, uint64_t value) { append_le(out, value); }

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

  template <typename T>
  bool read_le(const std::string &in, size_t &offset, T &value)
  {
    if (offset > in.size() || in.size() - offset < sizeof(T))
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 8 * sizeof(T); shift += 8)
      value |= static_cast<T>(static_cast<uint8_t>(in[offset++])) << shift;
    return true;
  }

  bool read_u64_le(const std::string &in, size_t &offset, uint64_t &value)
  {
    return read_le(in, offset, value);
  }

  size_t payment_proof_payload_bytes(bool indexed)
  {
    return indexed
        ? qwertycoin::epose::EPOSE_PAYMENT_PROOF_INDEXED_PAYLOAD_BYTES_V2
        : qwertycoin::epose::EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2;
  }
}

namespace qwertycoin
{
namespace epose
{
  record_codec_status_v2 encode_admission_lease_record_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      envelope_record_v2 &record)
  {
    record = {};
    if (!validate_admission_lease_v2(lease, context, policy))
      return record_codec_status_v2::invalid_record;
    std::string payload;
    payload.reserve(EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2);
    append_bytes(payload, lease.member.service_public_key);
    append_bytes(payload, lease.member.identity_id);
    append_bytes(payload, lease.member.operator_authorization_public_key);
    append_bytes(payload, lease.member.descriptor_hash);
    append_bytes(payload, lease.member.reward_binding_hash);
    append_bytes(payload, lease.member.endpoint_descriptor_hash);
    append_u64_le(payload, lease.member.sequence);
    if (lease.member.descriptor_version == 2)
    {
      append_u8(payload, lease.member.endpoint_transport);
      append_u8(payload, lease.member.dial_capabilities);
      append_u8(payload, lease.member.dns_address_families);
    }
    append_u64_le(payload, lease.target_epoch);
    append_u8(payload, lease.work_algorithm);
    append_u8(payload, lease.leading_zero_bits);
    append_u64_le(payload, lease.admission_context_height);
    append_bytes(payload, lease.admission_context_hash);
    append_u64_le(payload, lease.nonce);
    append_bytes(payload, lease.work_hash);
    append_bytes(payload, lease.lease_hash);
    if (payload.size() != (lease.member.descriptor_version == 2
            ? EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_HF18_V2
            : EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2))
      return record_codec_status_v2::wrong_size;
    record.type = static_cast<uint8_t>(record_type_v2::admission_lease);
    record.version = lease.member.descriptor_version;
    record.payload.swap(payload);
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_admission_lease_record_structure_v2(
      const envelope_record_v2 &record,
      admission_lease_v2 &lease)
  {
    lease = {};
    if (record.type != static_cast<uint8_t>(record_type_v2::admission_lease))
      return record_codec_status_v2::wrong_type;
    if (record.version != 1 && record.version != 2)
      return record_codec_status_v2::wrong_version;
    if (record.payload.size() != (record.version == 2
            ? EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_HF18_V2
            : EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2))
      return record_codec_status_v2::wrong_size;
    admission_lease_v2 next{};
    size_t offset = 0;
    if (!read_bytes(record.payload, offset, next.member.service_public_key)
        || !read_bytes(record.payload, offset, next.member.identity_id)
        || !read_bytes(record.payload, offset, next.member.operator_authorization_public_key)
        || !read_bytes(record.payload, offset, next.member.descriptor_hash)
        || !read_bytes(record.payload, offset, next.member.reward_binding_hash)
        || !read_bytes(record.payload, offset, next.member.endpoint_descriptor_hash)
        || !read_u64_le(record.payload, offset, next.member.sequence))
      return record_codec_status_v2::wrong_size;
    next.member.descriptor_version = record.version;
    if (record.version == 2
        && (!read_u8(record.payload, offset, next.member.endpoint_transport)
            || !read_u8(record.payload, offset, next.member.dial_capabilities)
            || !read_u8(record.payload, offset, next.member.dns_address_families)))
      return record_codec_status_v2::wrong_size;
    if (!read_u64_le(record.payload, offset, next.target_epoch)
        || !read_u8(record.payload, offset, next.work_algorithm)
        || !read_u8(record.payload, offset, next.leading_zero_bits)
        || !read_u64_le(record.payload, offset, next.admission_context_height)
        || !read_bytes(record.payload, offset, next.admission_context_hash)
        || !read_u64_le(record.payload, offset, next.nonce)
        || !read_bytes(record.payload, offset, next.work_hash)
        || !read_bytes(record.payload, offset, next.lease_hash)
        || offset != record.payload.size())
      return record_codec_status_v2::wrong_size;
    lease = next;
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_admission_lease_record_v2(
      const envelope_record_v2 &record,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      admission_lease_v2 &lease)
  {
    admission_lease_v2 next{};
    const record_codec_status_v2 status = decode_admission_lease_record_structure_v2(record, next);
    if (status != record_codec_status_v2::accepted)
    {
      lease = {};
      return status;
    }
    if (!validate_admission_lease_v2(next, context, policy))
    {
      lease = {};
      return record_codec_status_v2::invalid_record;
    }
    lease = next;
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 encode_lifecycle_record_v2(
      const lifecycle_record_v2 &lifecycle,
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      envelope_record_v2 &record)
  {
    record = {};
    if (validate_lifecycle_record_authorization_v2(
            nettype, genesis_hash, parameter_set_hash, lifecycle) != lifecycle_status_v2::accepted)
      return record_codec_status_v2::invalid_record;
    return encode_applied_lifecycle_record_v2(lifecycle, record);
  }

  record_codec_status_v2 encode_applied_lifecycle_record_v2(
      const lifecycle_record_v2 &lifecycle,
      envelope_record_v2 &record)
  {
    record = {};
    std::string payload;
    payload.reserve(EPOSE_LIFECYCLE_PAYLOAD_BYTES_V2);
    append_u8(payload, static_cast<uint8_t>(lifecycle.action));
    append_bytes(payload, lifecycle.previous_descriptor_hash);
    append_u8(payload, lifecycle.next_descriptor.version);
    append_bytes(payload, lifecycle.next_descriptor.identity_id);
    append_bytes(payload, lifecycle.next_descriptor.service_public_key);
    append_bytes(payload, lifecycle.next_descriptor.operator_authorization_public_key);
    append_bytes(payload, lifecycle.next_descriptor.reward_address.m_view_public_key);
    append_bytes(payload, lifecycle.next_descriptor.reward_address.m_spend_public_key);
    append_bytes(payload, lifecycle.next_descriptor.endpoint_descriptor_hash);
    append_u64_le(payload, lifecycle.next_descriptor.sequence);
    append_u64_le(payload, lifecycle.next_descriptor.effective_epoch);
    append_u64_le(payload, lifecycle.next_descriptor.expiry_epoch);
    if (lifecycle.next_descriptor.version == 2)
    {
      append_u8(payload, lifecycle.next_descriptor.endpoint_transport);
      append_u8(payload, lifecycle.next_descriptor.dial_capabilities);
      append_u8(payload, lifecycle.next_descriptor.dns_address_families);
    }
    append_bytes(payload, lifecycle.operator_signature);
    append_bytes(payload, lifecycle.service_signature);
    if (payload.size() != (lifecycle.next_descriptor.version == 2
            ? EPOSE_LIFECYCLE_PAYLOAD_BYTES_HF18_V2
            : EPOSE_LIFECYCLE_PAYLOAD_BYTES_V2))
      return record_codec_status_v2::wrong_size;
    record.type = static_cast<uint8_t>(lifecycle.action == lifecycle_action_v2::register_identity
        ? record_type_v2::identity_descriptor : record_type_v2::descriptor_lifecycle);
    record.version = lifecycle.next_descriptor.version;
    record.payload.swap(payload);
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_lifecycle_record_structure_v2(
      const envelope_record_v2 &record,
      lifecycle_record_v2 &lifecycle)
  {
    lifecycle = {};
    if (record.type != static_cast<uint8_t>(record_type_v2::identity_descriptor)
        && record.type != static_cast<uint8_t>(record_type_v2::descriptor_lifecycle))
      return record_codec_status_v2::wrong_type;
    if (record.version != 1 && record.version != 2)
      return record_codec_status_v2::wrong_version;
    if (record.payload.size() != (record.version == 2
            ? EPOSE_LIFECYCLE_PAYLOAD_BYTES_HF18_V2
            : EPOSE_LIFECYCLE_PAYLOAD_BYTES_V2))
      return record_codec_status_v2::wrong_size;
    lifecycle_record_v2 next{};
    uint8_t action = 0;
    size_t offset = 0;
    if (!read_u8(record.payload, offset, action)
        || !read_bytes(record.payload, offset, next.previous_descriptor_hash)
        || !read_u8(record.payload, offset, next.next_descriptor.version)
        || !read_bytes(record.payload, offset, next.next_descriptor.identity_id)
        || !read_bytes(record.payload, offset, next.next_descriptor.service_public_key)
        || !read_bytes(record.payload, offset, next.next_descriptor.operator_authorization_public_key)
        || !read_bytes(record.payload, offset, next.next_descriptor.reward_address.m_view_public_key)
        || !read_bytes(record.payload, offset, next.next_descriptor.reward_address.m_spend_public_key)
        || !read_bytes(record.payload, offset, next.next_descriptor.endpoint_descriptor_hash)
        || !read_u64_le(record.payload, offset, next.next_descriptor.sequence)
        || !read_u64_le(record.payload, offset, next.next_descriptor.effective_epoch)
        || !read_u64_le(record.payload, offset, next.next_descriptor.expiry_epoch))
      return record_codec_status_v2::wrong_size;
    if (record.version == 2
        && (!read_u8(record.payload, offset, next.next_descriptor.endpoint_transport)
            || !read_u8(record.payload, offset, next.next_descriptor.dial_capabilities)
            || !read_u8(record.payload, offset, next.next_descriptor.dns_address_families)))
      return record_codec_status_v2::wrong_size;
    if (record.version != next.next_descriptor.version
        || !read_bytes(record.payload, offset, next.operator_signature)
        || !read_bytes(record.payload, offset, next.service_signature)
        || offset != record.payload.size())
      return record_codec_status_v2::wrong_size;
    next.action = static_cast<lifecycle_action_v2>(action);
    const bool registration_type = record.type == static_cast<uint8_t>(record_type_v2::identity_descriptor);
    if (registration_type != (next.action == lifecycle_action_v2::register_identity))
      return record_codec_status_v2::wrong_type;
    lifecycle = next;
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_lifecycle_record_v2(
      const envelope_record_v2 &record,
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      lifecycle_record_v2 &lifecycle)
  {
    lifecycle_record_v2 next{};
    const record_codec_status_v2 status = decode_lifecycle_record_structure_v2(record, next);
    if (status != record_codec_status_v2::accepted)
    {
      lifecycle = {};
      return status;
    }
    if (validate_lifecycle_record_authorization_v2(
            nettype, genesis_hash, parameter_set_hash, next) != lifecycle_status_v2::accepted)
    {
      lifecycle = {};
      return record_codec_status_v2::invalid_record;
    }
    lifecycle = next;
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 encode_payment_proof_record_v2(
      const scoped_payment_proof_v2 &proof,
      const service_payment_context_v2 &context,
      envelope_record_v2 &record)
  {
    record = {};
    if (verify_scoped_payment_proof_v2(context, proof) != reward_status_v2::accepted)
      return record_codec_status_v2::invalid_record;
    const size_t payload_bytes = payment_proof_payload_bytes(
        static_cast<bool>(proof.service_output_start));
    std::string payload;
    payload.reserve(payload_bytes);
    append_bytes(payload, proof.derivation);
    append_bytes(payload, proof.proof);
    if (proof.service_output_start)
      append_le(payload, *proof.service_output_start);
    if (payload.size() != payload_bytes)
      return record_codec_status_v2::wrong_size;
    record.type = static_cast<uint8_t>(record_type_v2::service_payment_proof);
    record.version = EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2;
    record.payload.swap(payload);
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_payment_proof_record_v2(
      const envelope_record_v2 &record,
      const service_payment_context_v2 &context,
      scoped_payment_proof_v2 &proof,
      verification_counters_v2 *counters)
  {
    scoped_payment_proof_v2 next{};
    const record_codec_status_v2 structure =
        decode_payment_proof_record_structure_v2(record, next);
    if (structure != record_codec_status_v2::accepted)
    {
      proof = {};
      return structure;
    }
    if (verify_scoped_payment_proof_v2(context, next, counters)
        != reward_status_v2::accepted)
    {
      proof = {};
      return record_codec_status_v2::invalid_record;
    }
    proof = next;
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_payment_proof_record_structure_v2(
      const envelope_record_v2 &record,
      scoped_payment_proof_v2 &proof)
  {
    proof = {};
    if (record.type != static_cast<uint8_t>(record_type_v2::service_payment_proof))
      return record_codec_status_v2::wrong_type;
    if (record.version != EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2)
      return record_codec_status_v2::wrong_version;
    const bool indexed = record.payload.size() == payment_proof_payload_bytes(true);
    if (record.payload.size() != payment_proof_payload_bytes(indexed))
      return record_codec_status_v2::wrong_size;
    scoped_payment_proof_v2 next{};
    size_t offset = 0;
    if (!read_bytes(record.payload, offset, next.derivation)
        || !read_bytes(record.payload, offset, next.proof))
      return record_codec_status_v2::wrong_size;
    if (indexed)
    {
      uint32_t service_output_start = 0;
      if (!read_le(record.payload, offset, service_output_start))
        return record_codec_status_v2::wrong_size;
      next.service_output_start = service_output_start;
    }
    if (offset != record.payload.size())
      return record_codec_status_v2::wrong_size;
    proof = next;
    return record_codec_status_v2::accepted;
  }

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

  record_codec_status_v2 decode_service_receipt_record_structure_v2(
      const envelope_record_v2 &record,
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
    receipt = next;
    return record_codec_status_v2::accepted;
  }

  record_codec_status_v2 decode_service_receipt_record_v2(
      const envelope_record_v2 &record,
      const receipt_context_v2 &context,
      authenticated_service_receipt_v2 &receipt)
  {
    authenticated_service_receipt_v2 next{};
    const record_codec_status_v2 status = decode_service_receipt_record_structure_v2(record, next);
    if (status != record_codec_status_v2::accepted)
    {
      receipt = {};
      return status;
    }
    if (!validate_authenticated_service_receipt_v2(next, context))
    {
      receipt = {};
      return record_codec_status_v2::invalid_record;
    }
    receipt = next;
    return record_codec_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
