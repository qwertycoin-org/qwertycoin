// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "epose/membership_v2.h"

namespace qwertycoin
{
namespace epose
{
  enum class service_kind_v2 : uint8_t
  {
    canonical_object = 1
  };

  // The transport obtains the requested object and validates its canonical
  // bytes locally. Consensus validates this bounded transcript and its two
  // signatures; it never performs network I/O.
  struct service_challenge_v2
  {
    uint8_t version = EPOSE_PROTOCOL_VERSION_V2;
    uint8_t service_kind = static_cast<uint8_t>(service_kind_v2::canonical_object);
    uint64_t epoch = 0;
    uint64_t round = 0;
    crypto::hash snapshot_hash{};
    crypto::hash anchor_hash{};
    crypto::public_key subject_public_key{};
    crypto::public_key verifier_public_key{};
    crypto::hash endpoint_descriptor_hash{};
    crypto::hash nonce{};
    crypto::hash requested_object_hash{};
  };

  struct authenticated_service_receipt_v2
  {
    service_challenge_v2 challenge{};
    crypto::hash response_object_hash{};
    crypto::signature subject_signature{};
    crypto::signature verifier_signature{};
  };

  struct receipt_context_v2
  {
    cryptonote::network_type nettype = cryptonote::UNDEFINED;
    crypto::hash genesis_hash{};
    crypto::hash parameter_set_hash{};
  };

  crypto::hash hash_service_challenge_v2(
      const service_challenge_v2 &challenge,
      const receipt_context_v2 &context);

  crypto::hash hash_subject_response_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context);

  crypto::hash hash_authenticated_service_receipt_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context);

  void sign_subject_response_v2(
      authenticated_service_receipt_v2 &receipt,
      const crypto::secret_key &subject_secret_key,
      const receipt_context_v2 &context);

  void sign_verifier_receipt_v2(
      authenticated_service_receipt_v2 &receipt,
      const crypto::secret_key &verifier_secret_key,
      const receipt_context_v2 &context);

  bool validate_authenticated_service_receipt_v2(
      const authenticated_service_receipt_v2 &receipt,
      const receipt_context_v2 &context);

} // namespace epose
} // namespace qwertycoin
