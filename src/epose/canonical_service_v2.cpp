// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/canonical_service_v2.h"

#include "cryptonote_basic/cryptonote_format_utils.h"

namespace
{
  bool key_matches(const crypto::secret_key &secret_key, const crypto::public_key &expected)
  {
    crypto::public_key actual{};
    return crypto::secret_key_to_public_key(secret_key, actual) && actual == expected;
  }

  bool parse_exact_block(
      const cryptonote::blobdata &blob,
      const crypto::hash &expected_hash,
      cryptonote::block &block)
  {
    crypto::hash actual_hash{};
    return cryptonote::parse_and_validate_block_from_blob(blob, block, actual_hash)
        && actual_hash == expected_hash
        && cryptonote::block_to_blob(block) == blob;
  }

  void clear_response(qwertycoin::epose::canonical_service_response_v2 &response)
  {
    response = {};
  }
}

namespace qwertycoin
{
namespace epose
{
  canonical_service_status_v2 answer_canonical_block_challenge_v2(
      const service_challenge_v2 &challenge,
      const receipt_context_v2 &context,
      const canonical_service_limits_v2 &limits,
      const crypto::secret_key &subject_secret_key,
      const challenge_authorizer_v2 &authorize,
      const canonical_block_source_v2 &canonical_source,
      canonical_service_response_v2 &response)
  {
    clear_response(response);
    if (!limits.valid() || !authorize || !canonical_source)
      return canonical_service_status_v2::invalid_configuration;
    if (!validate_service_challenge_v2(challenge, context))
      return canonical_service_status_v2::invalid_challenge;
    if (!authorize(challenge, context))
      return canonical_service_status_v2::unauthorized_challenge;
    if (!key_matches(subject_secret_key, challenge.subject_public_key))
      return canonical_service_status_v2::wrong_signing_key;

    cryptonote::blobdata block_blob;
    if (!canonical_source(challenge.requested_object_hash, block_blob))
      return canonical_service_status_v2::object_not_found;
    if (block_blob.empty() || block_blob.size() > limits.max_block_blob_bytes)
      return canonical_service_status_v2::object_too_large;

    cryptonote::block block{};
    crypto::hash parsed_hash{};
    if (!cryptonote::parse_and_validate_block_from_blob(block_blob, block, parsed_hash)
        || cryptonote::block_to_blob(block) != block_blob)
      return canonical_service_status_v2::malformed_object;
    if (parsed_hash != challenge.requested_object_hash)
      return canonical_service_status_v2::wrong_object;

    authenticated_service_receipt_v2 partial{};
    partial.challenge = challenge;
    partial.response_object_hash = parsed_hash;
    sign_subject_response_v2(partial, subject_secret_key, context);
    response.block_blob = std::move(block_blob);
    response.subject_signature = partial.subject_signature;
    return canonical_service_status_v2::accepted;
  }

  canonical_service_status_v2 verify_canonical_block_response_v2(
      const service_challenge_v2 &challenge,
      const canonical_service_response_v2 &response,
      const receipt_context_v2 &context,
      const canonical_service_limits_v2 &limits,
      const crypto::secret_key &verifier_secret_key,
      const challenge_authorizer_v2 &authorize,
      const canonical_block_source_v2 &canonical_source,
      authenticated_service_receipt_v2 &receipt)
  {
    receipt = {};
    if (!limits.valid() || !authorize || !canonical_source)
      return canonical_service_status_v2::invalid_configuration;
    if (!validate_service_challenge_v2(challenge, context))
      return canonical_service_status_v2::invalid_challenge;
    if (!authorize(challenge, context))
      return canonical_service_status_v2::unauthorized_challenge;
    if (!key_matches(verifier_secret_key, challenge.verifier_public_key))
      return canonical_service_status_v2::wrong_signing_key;
    if (response.block_blob.empty() || response.block_blob.size() > limits.max_block_blob_bytes)
      return canonical_service_status_v2::object_too_large;

    cryptonote::block remote_block{};
    crypto::hash remote_hash{};
    if (!cryptonote::parse_and_validate_block_from_blob(response.block_blob, remote_block, remote_hash)
        || cryptonote::block_to_blob(remote_block) != response.block_blob)
      return canonical_service_status_v2::malformed_object;
    if (remote_hash != challenge.requested_object_hash)
      return canonical_service_status_v2::wrong_object;

    cryptonote::blobdata local_blob;
    if (!canonical_source(challenge.requested_object_hash, local_blob))
      return canonical_service_status_v2::object_not_found;
    if (local_blob.empty() || local_blob.size() > limits.max_block_blob_bytes)
      return canonical_service_status_v2::object_too_large;
    cryptonote::block local_block{};
    if (!parse_exact_block(local_blob, challenge.requested_object_hash, local_block)
        || local_blob != response.block_blob)
      return canonical_service_status_v2::wrong_object;

    authenticated_service_receipt_v2 candidate{};
    candidate.challenge = challenge;
    candidate.response_object_hash = challenge.requested_object_hash;
    candidate.subject_signature = response.subject_signature;
    if (!crypto::check_signature(
          hash_subject_response_v2(candidate, context),
          challenge.subject_public_key,
          candidate.subject_signature))
      return canonical_service_status_v2::invalid_subject_signature;

    sign_verifier_receipt_v2(candidate, verifier_secret_key, context);
    if (!validate_authenticated_service_receipt_v2(candidate, context))
      return canonical_service_status_v2::invalid_subject_signature;
    receipt = candidate;
    return canonical_service_status_v2::accepted;
  }

} // namespace epose
} // namespace qwertycoin
