// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/canonical_service_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  struct key_pair
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  key_pair make_key_pair()
  {
    key_pair out{};
    crypto::generate_keys(out.public_key, out.secret_key);
    return out;
  }

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  cryptonote::block make_block(uint32_t nonce)
  {
    cryptonote::block block{};
    block.major_version = 17;
    block.minor_version = 17;
    block.timestamp = 123456;
    block.prev_id = hash_text("parent");
    block.nonce = nonce;
    block.miner_tx.version = 2;
    block.miner_tx.vin.push_back(cryptonote::txin_gen{42});
    return block;
  }

  receipt_context_v2 make_context()
  {
    return {cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters")};
  }

  service_challenge_v2 make_challenge(const key_pair &subject, const key_pair &verifier,
      const crypto::hash &object_hash)
  {
    service_challenge_v2 challenge{};
    challenge.epoch = 8;
    challenge.round = 1;
    challenge.snapshot_hash = hash_text("snapshot");
    challenge.anchor_hash = hash_text("anchor");
    challenge.subject_public_key = subject.public_key;
    challenge.verifier_public_key = verifier.public_key;
    challenge.endpoint_descriptor_hash = hash_text("endpoint");
    challenge.nonce = hash_text("nonce");
    challenge.requested_object_hash = object_hash;
    return challenge;
  }

  challenge_authorizer_v2 allow = [](const service_challenge_v2 &, const receipt_context_v2 &) {
    return true;
  };
}

TEST(epose_canonical_service_v2, subject_serves_and_signs_exact_canonical_block_bytes)
{
  const key_pair subject = make_key_pair();
  const key_pair verifier = make_key_pair();
  const cryptonote::block block = make_block(7);
  const cryptonote::blobdata blob = cryptonote::block_to_blob(block);
  const crypto::hash block_hash = cryptonote::get_block_hash(block);
  const auto challenge = make_challenge(subject, verifier, block_hash);
  const canonical_service_limits_v2 limits{blob.size()};
  const canonical_block_source_v2 source = [&](const crypto::hash &requested, cryptonote::blobdata &out) {
    if (requested != block_hash)
      return false;
    out = blob;
    return true;
  };

  canonical_service_response_v2 response{};
  ASSERT_EQ(canonical_service_status_v2::accepted,
      answer_canonical_block_challenge_v2(challenge, make_context(), limits,
          subject.secret_key, allow, source, response));
  EXPECT_EQ(blob, response.block_blob);

  authenticated_service_receipt_v2 receipt{};
  ASSERT_EQ(canonical_service_status_v2::accepted,
      verify_canonical_block_response_v2(challenge, response, make_context(), limits,
          verifier.secret_key, allow, source, receipt));
  EXPECT_TRUE(validate_authenticated_service_receipt_v2(receipt, make_context()));
}

TEST(epose_canonical_service_v2, authorization_precedes_lookup_and_signing)
{
  const key_pair subject = make_key_pair();
  const key_pair verifier = make_key_pair();
  const auto block = make_block(8);
  const auto challenge = make_challenge(subject, verifier, cryptonote::get_block_hash(block));
  bool source_called = false;
  const canonical_block_source_v2 source = [&](const crypto::hash &, cryptonote::blobdata &) {
    source_called = true;
    return true;
  };
  canonical_service_response_v2 response{};
  response.block_blob = "stale";
  ASSERT_EQ(canonical_service_status_v2::unauthorized_challenge,
      answer_canonical_block_challenge_v2(challenge, make_context(), {1024}, subject.secret_key,
          [](const service_challenge_v2 &, const receipt_context_v2 &) { return false; },
          source, response));
  EXPECT_FALSE(source_called);
  EXPECT_TRUE(response.block_blob.empty());

  auto invalid = challenge;
  invalid.nonce = crypto::null_hash;
  ASSERT_EQ(canonical_service_status_v2::invalid_challenge,
      answer_canonical_block_challenge_v2(invalid, make_context(), {1024}, subject.secret_key,
          allow, source, response));
  EXPECT_FALSE(source_called);
}

TEST(epose_canonical_service_v2, subject_rejects_wrong_key_unknown_oversized_and_wrong_objects_atomically)
{
  const key_pair subject = make_key_pair();
  const key_pair verifier = make_key_pair();
  const key_pair other = make_key_pair();
  const auto block = make_block(9);
  const auto other_block = make_block(10);
  const auto blob = cryptonote::block_to_blob(block);
  const auto other_blob = cryptonote::block_to_blob(other_block);
  const auto challenge = make_challenge(subject, verifier, cryptonote::get_block_hash(block));
  canonical_service_response_v2 response{};

  const canonical_block_source_v2 good = [&](const crypto::hash &, cryptonote::blobdata &out) {
    out = blob;
    return true;
  };
  EXPECT_EQ(canonical_service_status_v2::wrong_signing_key,
      answer_canonical_block_challenge_v2(challenge, make_context(), {blob.size()},
          other.secret_key, allow, good, response));
  EXPECT_TRUE(response.block_blob.empty());

  EXPECT_EQ(canonical_service_status_v2::object_not_found,
      answer_canonical_block_challenge_v2(challenge, make_context(), {blob.size()},
          subject.secret_key, allow,
          [](const crypto::hash &, cryptonote::blobdata &) { return false; }, response));
  EXPECT_TRUE(response.block_blob.empty());

  EXPECT_EQ(canonical_service_status_v2::object_too_large,
      answer_canonical_block_challenge_v2(challenge, make_context(), {blob.size() - 1},
          subject.secret_key, allow, good, response));
  EXPECT_TRUE(response.block_blob.empty());

  const canonical_block_source_v2 wrong = [&](const crypto::hash &, cryptonote::blobdata &out) {
    out = other_blob;
    return true;
  };
  EXPECT_EQ(canonical_service_status_v2::wrong_object,
      answer_canonical_block_challenge_v2(challenge, make_context(), {other_blob.size()},
          subject.secret_key, allow, wrong, response));
  EXPECT_TRUE(response.block_blob.empty());
}

TEST(epose_canonical_service_v2, verifier_rejects_tampering_bad_signature_and_noncanonical_local_view)
{
  const key_pair subject = make_key_pair();
  const key_pair verifier = make_key_pair();
  const auto block = make_block(11);
  const auto other_block = make_block(12);
  const auto blob = cryptonote::block_to_blob(block);
  const auto other_blob = cryptonote::block_to_blob(other_block);
  const auto hash = cryptonote::get_block_hash(block);
  const auto challenge = make_challenge(subject, verifier, hash);
  const canonical_service_limits_v2 limits{blob.size() + 64};
  const canonical_block_source_v2 good = [&](const crypto::hash &, cryptonote::blobdata &out) {
    out = blob;
    return true;
  };
  canonical_service_response_v2 response{};
  ASSERT_EQ(canonical_service_status_v2::accepted,
      answer_canonical_block_challenge_v2(challenge, make_context(), limits,
          subject.secret_key, allow, good, response));

  authenticated_service_receipt_v2 receipt{};
  receipt.challenge.snapshot_hash = hash_text("stale");
  auto tampered = response;
  tampered.block_blob.back() ^= 1;
  EXPECT_EQ(canonical_service_status_v2::malformed_object,
      verify_canonical_block_response_v2(challenge, tampered, make_context(), limits,
          verifier.secret_key, allow, good, receipt));
  EXPECT_EQ(crypto::null_hash, receipt.challenge.snapshot_hash);

  tampered = response;
  std::memset(&tampered.subject_signature, 0, sizeof(tampered.subject_signature));
  EXPECT_EQ(canonical_service_status_v2::invalid_subject_signature,
      verify_canonical_block_response_v2(challenge, tampered, make_context(), limits,
          verifier.secret_key, allow, good, receipt));
  EXPECT_EQ(crypto::null_hash, receipt.challenge.snapshot_hash);

  const canonical_block_source_v2 divergent = [&](const crypto::hash &, cryptonote::blobdata &out) {
    out = other_blob;
    return true;
  };
  EXPECT_EQ(canonical_service_status_v2::wrong_object,
      verify_canonical_block_response_v2(challenge, response, make_context(), limits,
          verifier.secret_key, allow, divergent, receipt));
  EXPECT_EQ(crypto::null_hash, receipt.challenge.snapshot_hash);
}

TEST(epose_canonical_service_v2, verifier_authorizes_before_local_lookup_and_requires_its_key)
{
  const key_pair subject = make_key_pair();
  const key_pair verifier = make_key_pair();
  const key_pair other = make_key_pair();
  const auto block = make_block(13);
  const auto blob = cryptonote::block_to_blob(block);
  const auto challenge = make_challenge(subject, verifier, cryptonote::get_block_hash(block));
  const canonical_service_limits_v2 limits{blob.size()};
  bool source_called = false;
  const canonical_block_source_v2 source = [&](const crypto::hash &, cryptonote::blobdata &out) {
    source_called = true;
    out = blob;
    return true;
  };
  canonical_service_response_v2 response{};
  ASSERT_EQ(canonical_service_status_v2::accepted,
      answer_canonical_block_challenge_v2(challenge, make_context(), limits,
          subject.secret_key, allow, source, response));

  source_called = false;
  authenticated_service_receipt_v2 receipt{};
  EXPECT_EQ(canonical_service_status_v2::unauthorized_challenge,
      verify_canonical_block_response_v2(challenge, response, make_context(), limits,
          verifier.secret_key,
          [](const service_challenge_v2 &, const receipt_context_v2 &) { return false; },
          source, receipt));
  EXPECT_FALSE(source_called);
  EXPECT_EQ(crypto::null_hash, receipt.challenge.snapshot_hash);

  EXPECT_EQ(canonical_service_status_v2::wrong_signing_key,
      verify_canonical_block_response_v2(challenge, response, make_context(), limits,
          other.secret_key, allow, source, receipt));
  EXPECT_FALSE(source_called);
  EXPECT_EQ(crypto::null_hash, receipt.challenge.snapshot_hash);

  EXPECT_EQ(canonical_service_status_v2::object_too_large,
      verify_canonical_block_response_v2(challenge, response, make_context(), {blob.size() - 1},
          verifier.secret_key, allow, source, receipt));
  EXPECT_FALSE(source_called);
  EXPECT_EQ(crypto::null_hash, receipt.challenge.snapshot_hash);
}
