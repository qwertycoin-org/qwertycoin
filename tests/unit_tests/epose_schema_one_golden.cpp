#include <gtest/gtest.h>

#include <array>
#include <map>
#include <string>
#include <vector>

#include "epose/block_transition_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"
#include "string_tools.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &value)
  {
    return crypto::cn_fast_hash(value.data(), value.size());
  }

  const crypto::hash genesis = hash_text("schema-one-genesis");
  const crypto::hash parameters = hash_text("schema-one-parameters");
  const epoch_timing_v2 timing{0, 12, 2};
  const admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};

  struct keys
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  keys fixed_keys(uint8_t scalar)
  {
    keys result{};
    result.secret_key.data[0] = scalar;
    EXPECT_TRUE(crypto::secret_key_to_public_key(result.secret_key, result.public_key));
    return result;
  }

  struct member
  {
    keys service;
    keys authority;
    keys view;
    keys spend;
    identity_descriptor_v2 descriptor{};
  };

  member fixed_member(uint8_t first_scalar, uint64_t effective_epoch)
  {
    member result{fixed_keys(first_scalar), fixed_keys(uint8_t(first_scalar + 1)),
        fixed_keys(uint8_t(first_scalar + 2)), fixed_keys(uint8_t(first_scalar + 3)), {}};
    result.descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, result.authority.public_key);
    result.descriptor.service_public_key = result.service.public_key;
    result.descriptor.operator_authorization_public_key = result.authority.public_key;
    result.descriptor.reward_address.m_view_public_key = result.view.public_key;
    result.descriptor.reward_address.m_spend_public_key = result.spend.public_key;
    result.descriptor.endpoint_descriptor_hash = hash_text(
        "endpoint-" + std::to_string(first_scalar));
    result.descriptor.effective_epoch = effective_epoch;
    result.descriptor.expiry_epoch = effective_epoch + 4;
    return result;
  }

  envelope_record_v2 lifecycle_envelope(
      const member &value, lifecycle_action_v2 action,
      const identity_descriptor_v2 &descriptor,
      const crypto::hash &previous = crypto::null_hash)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.action = action;
    lifecycle.previous_descriptor_hash = previous;
    lifecycle.next_descriptor = descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(cryptonote::TESTNET, genesis,
        parameters, lifecycle, value.authority.secret_key, value.service.secret_key));
    envelope_record_v2 envelope{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            lifecycle, cryptonote::TESTNET, genesis, parameters, envelope));
    return envelope;
  }

  envelope_record_v2 admission_envelope(
      const member &value, uint64_t target_epoch,
      uint64_t context_height, const crypto::hash &context_hash)
  {
    admission_lease_v2 lease{};
    lease.member.service_public_key = value.service.public_key;
    lease.member.identity_id = value.descriptor.identity_id;
    lease.member.operator_authorization_public_key = value.authority.public_key;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor.reward_address);
    lease.member.endpoint_descriptor_hash = value.descriptor.endpoint_descriptor_hash;
    lease.target_epoch = target_epoch;
    lease.work_algorithm = static_cast<uint8_t>(admission.algorithm);
    lease.leading_zero_bits = admission.leading_zero_bits;
    lease.admission_context_height = context_height;
    lease.admission_context_hash = context_hash;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters, context_height, context_hash};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(lease.work_hash, admission.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    envelope_record_v2 envelope{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_admission_lease_record_v2(lease, context, admission, envelope));
    return envelope;
  }

  envelope_record_v2 receipt_envelope(
      const block_transition_v2 &chain, const member &subject,
      const member &verifier, uint64_t epoch, const crypto::hash &anchor,
      const std::string &nonce)
  {
    const auto *snapshot = chain.state().membership().snapshot(epoch);
    EXPECT_NE(nullptr, snapshot);
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.service_kind = committee.service_kind;
    receipt.challenge.epoch = epoch;
    receipt.challenge.snapshot_hash = snapshot == nullptr
        ? crypto::null_hash : snapshot->snapshot_hash;
    receipt.challenge.anchor_hash = anchor;
    receipt.challenge.subject_public_key = subject.service.public_key;
    receipt.challenge.verifier_public_key = verifier.service.public_key;
    receipt.challenge.endpoint_descriptor_hash = subject.descriptor.endpoint_descriptor_hash;
    receipt.challenge.nonce = hash_text(nonce);
    receipt.challenge.requested_object_hash = anchor;
    receipt.response_object_hash = anchor;
    const receipt_context_v2 context{cryptonote::TESTNET, genesis, parameters};
    const std::map<std::string, std::string> signatures{
        {"alice-1", "36f6f93a2c6b620dafe22e4fbb592a61d0ad04d2adf396fe1243b812eae90908d5adc9615e303f68bf2727652f2b82d58304276a90ba94fba1e8abdd6530d501"},
        {"bob-1", "5cf96655021f985ae368269e9661224ae320907ecc5c566e0fd0368b2c3f4807e456dce5ed4120b171512e0d977813b85cfad05a2a96a0010918f4c6f9afc408"},
        {"alice-2", "fe67598f8ff317d30480d9982236062f182e3e861ff6fd1dacbc19c513c4020ec6305a099b5af4289e9da97e0f08d120e59bc92d1a1e3055bf555cd9b1e7a207"},
        {"bob-2", "e6ca03628e49769ce0efd8fc2e3fff6ab63a25c5e2719a2546ce999af8a6350da73aafc97c4f2c18bae7cac021e9a84a48f1bd012e05f9fff9039823f1ebc600"},
        {"carol-3", "7bc064cf6767bbe3f8453ae6125b16a58ca53fe9b84007f5cbe1b5c8a538dc016b6c5beee0390c3110c43640a3ea5b7f8fac187fcfed406a137b5e5faf374d0e"},
        {"bob-3", "c6834c3f4f04989d084c7613a92e9f714f1f83f120ac804a97f0c7991c46d60ab4fbb64181e61d3495135d2cd94bef57879a439ac3c4cd35a2781c51f0a5a309"}};
    const auto signature = signatures.find(nonce);
    EXPECT_NE(signatures.end(), signature);
    if (signature != signatures.end())
      EXPECT_TRUE(epee::string_tools::hex_to_pod(
          signature->second, receipt.subject_signature));
    sign_verifier_receipt_v2(receipt, verifier.service.secret_key, context);
    envelope_record_v2 envelope{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_service_receipt_record_v2(receipt, context, envelope));
    return envelope;
  }

  class contexts final : public canonical_context_source_v2
  {
  public:
    std::map<uint64_t, crypto::hash> blocks;

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      const auto found = blocks.find(height);
      if (found == blocks.end())
        return false;
      hash = found->second;
      return true;
    }

    bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &hash) const override
    {
      if (round != 0 || epoch == 0)
        return false;
      return block_hash(epoch * timing.epoch_length - timing.anchor_depth, hash);
    }
  };

  block_transition_limits_v2 chain_limits()
  {
    block_transition_limits_v2 limits{};
    limits.max_envelopes_per_transaction = 8;
    limits.envelope.max_envelope_bytes = 16384;
    limits.envelope.max_records = 16;
    limits.envelope.max_record_payload_bytes = 2048;
    limits.envelope.max_signature_verifications = 32;
    limits.envelope.max_admission_verifications = 8;
    limits.envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
    limits.block = {32768, 32, 64, 16};
    limits.max_recent_undo_blocks = 64;
    return limits;
  }

  block_transition_v2 make_chain()
  {
    return {cryptonote::TESTNET, genesis, parameters,
        timing, admission, committee, chain_limits()};
  }

  block_transition_status_v2 apply(
      block_transition_v2 &chain, contexts &source, uint64_t height,
      const std::vector<envelope_record_v2> &records)
  {
    cryptonote::transaction miner{};
    cryptonote::transaction transaction{};
    std::vector<block_transaction_context_v2> transactions{{&miner, true, nullptr}};
    if (!records.empty())
    {
      envelope_budget_v2 budget{};
      EXPECT_EQ(envelope_status_v2::accepted,
          append_transaction_envelope_v2(records, HF_VERSION_QWC_EPOSE,
              chain_limits().max_envelopes_per_transaction,
              chain_limits().envelope, transaction.extra, budget));
      transactions.push_back({&transaction, false, nullptr});
    }
    const crypto::hash block_hash = height == 0
        ? genesis : hash_text("schema-one-block-" + std::to_string(height));
    source.blocks[height] = block_hash;
    block_apply_summary_v2 summary{};
    return chain.apply_block(HF_VERSION_QWC_EPOSE, height,
        block_hash, transactions, source, summary);
  }
}

TEST(epose_schema_one_golden, multi_epoch_chain_and_reorg)
{
  auto chain = make_chain();
  ASSERT_TRUE(chain.valid());
  contexts source{};
  const member alice = fixed_member(1, 1);
  const member bob = fixed_member(5, 1);
  const member carol = fixed_member(9, 3);
  const std::map<uint64_t, std::string> expected_by_change{
      {0, "deec6318b4ef00ead570a3aa603d4fe147970b47f71a8431793f64f392a1933b"},
      {1, "66c011b7a93ec54c75074bca2e90c4bf4ab26b5daee5b13964e7095f67123309"},
      {10, "8d4b2fa69acf56ce0822df6436d86d14ff1e9394f8cedc7ca1a0f6a12ed8b48c"},
      {12, "7fa2d9fcbc799a317cd694e8c6f1a00c7f398a7105c08547f1c0622acf12767a"},
      {13, "4dbfee472d6825fbb7b959145089617b33f8af8b81c18a2fa76901f816d5cb71"},
      {21, "e25fedadbc313d0eb08b0fd9f38db6cc7b8f0c3291f49478f2e0c48ddc27858d"},
      {22, "03814caf127f86a978442b73abd5b7862bf31c5bcda95efe6927619ed5ccda26"},
      {24, "4adf3fdcce0929a866bb181b69eb0294051c8b617b5b217c57ee3175df13bce1"},
      {25, "e73ac41e44a6a1db436979fdd4220cbccc50320a8661c5fda2b29c90b372d9bc"},
      {33, "1aaa2ab0f29f5f2a14514df40cca8eacb245cea5abe56d44e6289fbf3e0608e7"},
      {34, "c0cec57d4b5bacb7ac9ecac27cec4297303ba8ef33d2a964c0e79a56ca8e59cb"},
      {36, "2dabbe0ac3f63a0311104a06c4354c55f98005ec544901bb6df046d44e070af3"},
      {45, "bdc2f45283227b7b74194ad27b06ce065d50bce317d375ba12ab134a1ee5ffe3"},
      {46, "887c0dddf7f4fc9d919a619790eeaf7e7106f67809648f6e24cb2318d86b5c9c"}};
  std::vector<std::string> hashes;
  std::string expected;
  for (uint64_t height = 0; height < 48; ++height)
  {
    std::vector<envelope_record_v2> records;
    if (height == 26)
      EXPECT_EQ(block_transition_status_v2::invalid_semantics,
          apply(chain, source, height, {lifecycle_envelope(
              alice, lifecycle_action_v2::register_identity,
              alice.descriptor)}));
    if (height == 1)
    {
      records.push_back(lifecycle_envelope(
          alice, lifecycle_action_v2::register_identity, alice.descriptor));
      records.push_back(lifecycle_envelope(
          bob, lifecycle_action_v2::register_identity, bob.descriptor));
      records.push_back(admission_envelope(alice, 1, 0, genesis));
      records.push_back(admission_envelope(bob, 1, 0, genesis));
    }
    if (height == 12 || height == 24)
    {
      const uint64_t epoch = height / timing.epoch_length;
      const crypto::hash anchor = source.blocks.at(height - timing.anchor_depth);
      records.push_back(receipt_envelope(chain, alice, bob, epoch,
          anchor, "alice-" + std::to_string(epoch)));
      records.push_back(receipt_envelope(chain, bob, alice, epoch,
          anchor, "bob-" + std::to_string(epoch)));
    }
    if (height == 13)
    {
      records.push_back(admission_envelope(alice, 2, 12, source.blocks.at(12)));
      records.push_back(admission_envelope(bob, 2, 12, source.blocks.at(12)));
    }
    if (height == 25)
    {
      identity_descriptor_v2 retired = alice.descriptor;
      retired.sequence = 1;
      retired.effective_epoch = 3;
      retired.expiry_epoch = 3;
      records.push_back(lifecycle_envelope(alice,
          lifecycle_action_v2::deregister_identity, retired,
          hash_identity_descriptor_v2(cryptonote::TESTNET,
              genesis, parameters, alice.descriptor)));
      records.push_back(lifecycle_envelope(carol,
          lifecycle_action_v2::register_identity, carol.descriptor));
      records.push_back(admission_envelope(carol, 3, 24, source.blocks.at(24)));
      records.push_back(admission_envelope(bob, 3, 24, source.blocks.at(24)));
    }
    if (height == 36)
    {
      const crypto::hash anchor = source.blocks.at(34);
      records.push_back(receipt_envelope(chain, carol, bob, 3, anchor, "carol-3"));
      records.push_back(receipt_envelope(chain, bob, carol, 3, anchor, "bob-3"));
    }
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply(chain, source, height, records)) << height;
    hashes.push_back(epee::string_tools::pod_to_hex(chain.state().state_hash()));
    const auto change = expected_by_change.find(height);
    if (change != expected_by_change.end())
      expected = change->second;
    EXPECT_EQ(expected, hashes.back()) << height;
  }
  for (uint64_t height : {47u, 46u, 45u})
    ASSERT_EQ(block_transition_status_v2::accepted, chain.disconnect_tip(height));
  EXPECT_EQ(hashes[44], epee::string_tools::pod_to_hex(chain.state().state_hash()));
  for (uint64_t height : {45u, 46u, 47u})
    ASSERT_EQ(block_transition_status_v2::accepted, apply(chain, source, height, {}));
  EXPECT_EQ(hashes[47], epee::string_tools::pod_to_hex(chain.state().state_hash()));
}
