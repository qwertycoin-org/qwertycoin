// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_receipt_v2.h"

#include <cstring>
#include <string>

namespace
{
  template <typename T>
  bool bytes_equal(const T &left, const T &right)
  {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
  }

  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
  }

  void append_u8(std::string &out, uint8_t value)
  {
    out.push_back(static_cast<char>(value));
  }

  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_network(std::string &out, cryptonote::network_type nettype)
  {
    const auto &network_id = cryptonote::get_config(nettype).NETWORK_ID;
    for (const auto byte : network_id)
      append_u8(out, byte);
  }

  crypto::hash hash_blob(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  bool context_valid(const qwertycoin::epose::receipt_context_v2 &context)
  {
    return context.nettype != cryptonote::UNDEFINED
        && context.genesis_hash != crypto::null_hash
        && context.parameter_set_hash != crypto::null_hash;
  }

  bool challenge_valid(const qwertycoin::epose::service_challenge_v2 &challenge)
  {
    using qwertycoin::epose::EPOSE_PROTOCOL_VERSION_V2;
    using qwertycoin::epose::service_kind_v2;
    return challenge.version == EPOSE_PROTOCOL_VERSION_V2
        && challenge.service_kind == static_cast<uint8_t>(service_kind_v2::canonical_object)
        && challenge.snapshot_hash != crypto::null_hash
        && challenge.anchor_hash != crypto::null_hash
        && challenge.endpoint_descriptor_hash != crypto::null_hash
        && challenge.nonce != crypto::null_hash
        && challenge.requested_object_hash != crypto::null_hash
        && crypto::check_key(challenge.subject_public_key)
        && crypto::check_key(challenge.verifier_public_key)
        && !bytes_equal(challenge.subject_public_key, challenge.verifier_public_key);
  }
}

namespace qwertycoin
{
namespace epose
{
  crypto::hash hash_service_challenge_v2(
      const service_challenge_v2 &challenge,
      const receipt_context_v2 &context)
  {
    std::string blob("QWC_EPOSE_CHALLENGE_V2");
    append_network(blob, context.nettype);
    append_bytes(blob, context.genesis_hash);
    append_bytes(blob, context.parameter_set_hash);
    append_u8(blob, challenge.version);
    append_u8(blob, challenge.service_kind);
    append_u64_le(blob, challenge.epoch);
    append_u64_le(blob, challenge.round);
    append_bytes(blob, challenge.snapshot_hash);
    append_bytes(blob, challenge.anchor_hash);
    append_bytes(blob, challenge.subject_public_key);
    append_bytes(blob, challenge.verifier_public_key);
    append_bytes(blob, challenge.endpoint_descriptor_hash);
    append_bytes(blob, challenge.nonce);
    append_bytes(blob, challenge.requested_object_hash);
    return hash_blob(blob);
  }

  bool validate_service_challenge_v2(
      const service_challenge_v2 &challenge,
      const receipt_context_v2 &context)
  {
    return context_valid(context) && challenge_valid(challenge);
  }

  crypto::hash hash_subject_response_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context)
  {
    std::string blob("QWC_EPOSE_SUBJECT_RESPONSE_V2");
    append_bytes(blob, hash_service_challenge_v2(receipt.challenge, context));
    append_bytes(blob, receipt.response_object_hash);
    return hash_blob(blob);
  }

  crypto::hash hash_authenticated_service_receipt_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context)
  {
    std::string blob("QWC_EPOSE_RECEIPT_V2");
    append_bytes(blob, hash_subject_response_v2(receipt, context));
    append_bytes(blob, receipt.subject_signature);
    return hash_blob(blob);
  }

  void sign_subject_response_v2(
      authenticated_service_receipt_v2 &receipt,
      const crypto::secret_key &subject_secret_key,
      const receipt_context_v2 &context)
  {
    crypto::generate_signature(
        hash_subject_response_v2(receipt, context),
        receipt.challenge.subject_public_key,
        subject_secret_key,
        receipt.subject_signature);
  }

  void sign_verifier_receipt_v2(
      authenticated_service_receipt_v2 &receipt,
      const crypto::secret_key &verifier_secret_key,
      const receipt_context_v2 &context)
  {
    crypto::generate_signature(
        hash_authenticated_service_receipt_v2(receipt, context),
        receipt.challenge.verifier_public_key,
        verifier_secret_key,
        receipt.verifier_signature);
  }

  bool validate_authenticated_service_receipt_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context,
      verification_counters_v2 *counters)
  {
    if (!validate_service_challenge_v2(receipt.challenge, context))
      return false;
    if (receipt.response_object_hash != receipt.challenge.requested_object_hash)
      return false;
    if (counters != nullptr)
      ++counters->signatures;
    if (!crypto::check_signature(
          hash_subject_response_v2(receipt, context),
          receipt.challenge.subject_public_key,
          receipt.subject_signature))
      return false;
    if (counters != nullptr)
      ++counters->signatures;
    return crypto::check_signature(
        hash_authenticated_service_receipt_v2(receipt, context),
        receipt.challenge.verifier_public_key,
        receipt.verifier_signature);
  }

} // namespace epose
} // namespace qwertycoin
