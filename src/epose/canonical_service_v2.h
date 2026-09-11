// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "cryptonote_basic/blobdatatype.h"
#include "epose/service_receipt_v2.h"

namespace qwertycoin
{
namespace epose
{
  enum class canonical_service_status_v2
  {
    accepted,
    invalid_configuration,
    invalid_challenge,
    unauthorized_challenge,
    wrong_signing_key,
    object_not_found,
    object_too_large,
    malformed_object,
    wrong_object,
    invalid_subject_signature
  };

  struct canonical_service_limits_v2
  {
    size_t max_block_blob_bytes = 0;

    bool valid() const { return max_block_blob_bytes > 0; }
  };

  struct canonical_service_response_v2
  {
    cryptonote::blobdata block_blob;
    crypto::signature subject_signature{};
  };

  // Implementations must return bytes for a block on their canonical chain,
  // never an alternative-chain or caller-supplied blob.
  using canonical_block_source_v2 = std::function<bool(
      const crypto::hash &block_hash,
      cryptonote::blobdata &block_blob)>;

  // Authorization is derived from frozen membership, selected committee,
  // round timing and the canonical anchor before any object lookup/signing.
  using challenge_authorizer_v2 = std::function<bool(
      const service_challenge_v2 &challenge,
      const receipt_context_v2 &context)>;

  canonical_service_status_v2 answer_canonical_block_challenge_v2(
      const service_challenge_v2 &challenge,
      const receipt_context_v2 &context,
      const canonical_service_limits_v2 &limits,
      const crypto::secret_key &subject_secret_key,
      const challenge_authorizer_v2 &authorize,
      const canonical_block_source_v2 &canonical_source,
      canonical_service_response_v2 &response);

  canonical_service_status_v2 verify_canonical_block_response_v2(
      const service_challenge_v2 &challenge,
      const canonical_service_response_v2 &response,
      const receipt_context_v2 &context,
      const canonical_service_limits_v2 &limits,
      const crypto::secret_key &verifier_secret_key,
      const challenge_authorizer_v2 &authorize,
      const canonical_block_source_v2 &canonical_source,
      authenticated_service_receipt_v2 &receipt);

} // namespace epose
} // namespace qwertycoin
