// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_config.h"
#include "epose/chainlock_v1.h"
#include "file_io_utils.h"

namespace
{
  using namespace cryptonote;
  using namespace qwertycoin::epose;

  TEST(chainlock_v1, age_expires_only_after_inclusive_limit_without_overflow)
  {
    EXPECT_FALSE(qwc_chainlock_age_expired(2, 2 + QWC_LOCK_MAX_AGE_BLOCKS));
    EXPECT_TRUE(qwc_chainlock_age_expired(2, 3 + QWC_LOCK_MAX_AGE_BLOCKS));
    EXPECT_FALSE(qwc_chainlock_age_expired(UINT64_MAX - 719, 5));
    EXPECT_FALSE(qwc_chainlock_age_expired(UINT64_MAX - 1, UINT64_MAX));
  }

  struct signer_key
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  chainlock_context_v1 make_context()
  {
    return {cryptonote::TESTNET, hash_text("chainlock-genesis"), hash_text("chainlock-parameters")};
  }

  std::vector<signer_key> make_signers(size_t count)
  {
    std::vector<signer_key> signers(count);
    for (signer_key &signer : signers)
      crypto::generate_keys(signer.public_key, signer.secret_key);
    return signers;
  }

  std::vector<crypto::public_key> public_keys(const std::vector<signer_key> &signers)
  {
    std::vector<crypto::public_key> keys;
    for (const signer_key &signer : signers)
      keys.push_back(signer.public_key);
    return keys;
  }

  const signer_key &key_for(const std::vector<signer_key> &signers, const crypto::public_key &member)
  {
    for (const signer_key &signer : signers)
      if (signer.public_key == member)
        return signer;
    throw std::runtime_error("unknown member");
  }

  chainlock_v1 share_from(const std::vector<signer_key> &signers, const chainlock_quorum_v1 &quorum, size_t index, uint64_t height, const crypto::hash &block_hash)
  {
    const signer_key &signer = key_for(signers, quorum.members[index]);
    chainlock_v1 share;
    EXPECT_TRUE(sign_chainlock_share_v1(height, block_hash, quorum, signer.public_key, signer.secret_key, make_context(), share));
    return share;
  }

  chainlock_v1 complete_lock(const std::vector<signer_key> &signers, const chainlock_quorum_v1 &quorum, uint64_t height, const crypto::hash &block_hash)
  {
    chainlock_v1 aggregate;
    aggregate.height = height;
    aggregate.block_hash = block_hash;
    aggregate.epoch = quorum.epoch;
    for (size_t index = quorum.members.size(); index-- > 0 && !chainlock_complete_v1(aggregate, quorum);)
      EXPECT_TRUE(merge_chainlock_share_v1(aggregate, share_from(signers, quorum, index, height, block_hash), quorum.threshold()));
    return aggregate;
  }

  struct chainlock_fixture : ::testing::Test
  {
    std::vector<signer_key> signers = make_signers(60);
    chainlock_quorum_v1 quorum = select_chainlock_quorum_v1(7, hash_text("anchor"), public_keys(signers), make_context());
    crypto::hash block_hash = hash_text("block");
  };
}

TEST(chainlock_v1, threshold_is_68_percent_rounded_up)
{
  EXPECT_EQ(34u, qwc_chainlock_threshold(50));
  EXPECT_EQ(7u, qwc_chainlock_threshold(10));
  EXPECT_EQ(9u, qwc_chainlock_threshold(13));
  EXPECT_EQ(1u, qwc_chainlock_threshold(1));
}

TEST(chainlock_v1, quorum_requires_minimum_nodes)
{
  const auto context = make_context();
  const auto nine = public_keys(make_signers(QWC_CHAINLOCK_MIN_NODES - 1));
  const auto ten = public_keys(make_signers(QWC_CHAINLOCK_MIN_NODES));
  EXPECT_FALSE(select_chainlock_quorum_v1(1, hash_text("a"), nine, context).can_lock());
  EXPECT_TRUE(select_chainlock_quorum_v1(1, hash_text("a"), ten, context).can_lock());
}

TEST_F(chainlock_fixture, quorum_is_capped_deterministic_and_anchor_bound)
{
  EXPECT_EQ(QWC_CHAINLOCK_QUORUM_MAX, quorum.members.size());
  EXPECT_EQ(34u, quorum.threshold());
  EXPECT_EQ(7u, quorum.epoch);

  std::vector<crypto::public_key> reversed = public_keys(signers);
  std::reverse(reversed.begin(), reversed.end());
  reversed.push_back(reversed.front());
  EXPECT_EQ(quorum.members, select_chainlock_quorum_v1(7, hash_text("anchor"), reversed, make_context()).members);
  EXPECT_NE(quorum.members, select_chainlock_quorum_v1(7, hash_text("other-anchor"), public_keys(signers), make_context()).members);
  EXPECT_NE(quorum.members, select_chainlock_quorum_v1(8, hash_text("anchor"), public_keys(signers), make_context()).members);
}

TEST(chainlock_v1, candidates_are_snapshot_members_qualified_in_previous_epoch)
{
  const auto keys = public_keys(make_signers(4));
  membership_snapshot_v2 snapshot{};
  for (size_t index = 0; index < 3; ++index)
  {
    frozen_member_v2 member{};
    member.service_public_key = keys[index];
    snapshot.members.push_back(member);
  }
  qualification_set_v2 qualified{};
  qualified.qualified_nodes = {keys[1], keys[2], keys[3]};
  const auto candidates = chainlock_candidates_v1(snapshot, qualified);
  ASSERT_EQ(2u, candidates.size());
  EXPECT_NE(candidates.end(), std::find(candidates.begin(), candidates.end(), keys[1]));
  EXPECT_NE(candidates.end(), std::find(candidates.begin(), candidates.end(), keys[2]));
}

TEST_F(chainlock_fixture, complete_lock_roundtrips_and_verifies)
{
  const chainlock_v1 lock = complete_lock(signers, quorum, 1000, block_hash);
  EXPECT_TRUE(chainlock_complete_v1(lock, quorum));
  EXPECT_EQ(quorum.threshold(), lock.signatures.size());

  const std::string blob = serialize_chainlock_v1(lock);
  EXPECT_EQ(QWC_CHAINLOCK_HEADER_SIZE + quorum.threshold() * QWC_CHAINLOCK_SIGNATURE_SIZE, blob.size());
  chainlock_v1 parsed;
  ASSERT_TRUE(parse_chainlock_v1(blob, parsed));
  EXPECT_EQ(lock.height, parsed.height);
  EXPECT_EQ(lock.block_hash, parsed.block_hash);
  EXPECT_EQ(lock.epoch, parsed.epoch);
  EXPECT_EQ(lock.signers, parsed.signers);
  EXPECT_TRUE(verify_chainlock_v1(parsed, quorum, make_context(), true));
  const auto indices = parsed.signer_indices();
  EXPECT_TRUE(std::is_sorted(indices.begin(), indices.end()));
}

TEST_F(chainlock_fixture, verify_rejects_tampering_and_wrong_context)
{
  chainlock_v1 lock = complete_lock(signers, quorum, 1000, block_hash);

  chainlock_v1 wrong_hash = lock;
  wrong_hash.block_hash = hash_text("other-block");
  EXPECT_FALSE(verify_chainlock_v1(wrong_hash, quorum, make_context(), true));
  EXPECT_TRUE(verify_chainlock_v1(wrong_hash, quorum, make_context(), false));

  chainlock_v1 wrong_epoch = lock;
  wrong_epoch.epoch = quorum.epoch + 1;
  EXPECT_FALSE(verify_chainlock_v1(wrong_epoch, quorum, make_context(), false));

  chainlock_context_v1 other_context = make_context();
  other_context.nettype = cryptonote::MAINNET;
  EXPECT_FALSE(verify_chainlock_v1(lock, quorum, other_context, true));

  chainlock_v1 swapped = lock;
  std::swap(swapped.signatures[0], swapped.signatures[1]);
  EXPECT_FALSE(verify_chainlock_v1(swapped, quorum, make_context(), true));

  chainlock_v1 out_of_quorum = lock;
  out_of_quorum.signers.back() |= 0x80;
  out_of_quorum.signatures.push_back(lock.signatures.front());
  EXPECT_FALSE(verify_chainlock_v1(out_of_quorum, quorum, make_context(), false));
}

TEST_F(chainlock_fixture, parse_rejects_malformed_blobs)
{
  const std::string blob = serialize_chainlock_v1(complete_lock(signers, quorum, 1000, block_hash));
  chainlock_v1 parsed;
  EXPECT_FALSE(parse_chainlock_v1(blob.substr(0, QWC_CHAINLOCK_HEADER_SIZE - 1), parsed));
  EXPECT_FALSE(parse_chainlock_v1(blob.substr(0, blob.size() - 1), parsed));
  EXPECT_FALSE(parse_chainlock_v1(blob + std::string(QWC_CHAINLOCK_SIGNATURE_SIZE, '\0'), parsed));
  EXPECT_FALSE(parse_chainlock_v1(std::string(QWC_CHAINLOCK_MAX_SIZE + 1, '\0'), parsed));

  chainlock_v1 empty;
  empty.height = 1;
  EXPECT_FALSE(parse_chainlock_v1(serialize_chainlock_v1(empty), parsed));
}

TEST_F(chainlock_fixture, merge_dedupes_and_stops_at_threshold)
{
  chainlock_v1 aggregate;
  aggregate.height = 1000;
  aggregate.block_hash = block_hash;
  aggregate.epoch = quorum.epoch;
  const chainlock_v1 first = share_from(signers, quorum, 3, 1000, block_hash);
  EXPECT_TRUE(merge_chainlock_share_v1(aggregate, first, quorum.threshold()));
  EXPECT_FALSE(merge_chainlock_share_v1(aggregate, first, quorum.threshold()));
  EXPECT_FALSE(merge_chainlock_share_v1(aggregate, share_from(signers, quorum, 4, 1001, block_hash), quorum.threshold()));
  EXPECT_FALSE(merge_chainlock_share_v1(aggregate, share_from(signers, quorum, 4, 1000, hash_text("fork")), quorum.threshold()));

  const chainlock_v1 lock = complete_lock(signers, quorum, 1000, block_hash);
  chainlock_v1 full = lock;
  EXPECT_FALSE(merge_chainlock_share_v1(full, share_from(signers, quorum, 0, 1000, block_hash), quorum.threshold()));
  EXPECT_EQ(quorum.threshold(), full.signer_count());
}

TEST_F(chainlock_fixture, non_member_cannot_sign)
{
  const signer_key outsider = make_signers(1).front();
  chainlock_v1 share;
  EXPECT_FALSE(sign_chainlock_share_v1(1000, block_hash, quorum, outsider.public_key, outsider.secret_key, make_context(), share));
}

TEST_F(chainlock_fixture, coinbase_field_roundtrips_at_full_size)
{
  const std::string blob = serialize_chainlock_v1(complete_lock(signers, quorum, 1000, block_hash));
  std::vector<uint8_t> extra;
  ASSERT_TRUE(add_chainlock_to_tx_extra(extra, blob));
  EXPECT_EQ(TX_EXTRA_TAG_CHAINLOCK, extra.front());
  EXPECT_LE(extra.size(), QWC_COINBASE_CHAINLOCK_FIELD_SIZE);
  blobdata parsed;
  EXPECT_EQ(1u, get_tx_chainlock(extra, parsed));
  EXPECT_EQ(blob, parsed);

  transaction_prefix prefix{};
  prefix.extra = extra;
  EXPECT_TRUE(check_tx_contract_extra(prefix, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_tx_contract_extra(prefix, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
  EXPECT_FALSE(check_tx_contract_extra(prefix, HF_VERSION_QWC_EPOSE, contract_extra_scope::coinbase));
}

TEST(chainlock_v1, coinbase_rejects_two_chainlock_fields)
{
  std::vector<tx_extra_field> one{tx_extra_chainlock{"lock"}};
  std::vector<tx_extra_field> two{one[0], one[0]};
  EXPECT_TRUE(check_contract_extra_fields(one, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_contract_extra_fields(two, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_contract_extra_fields(one, contract_extra_scope::transaction));
}

TEST(chainlock_v1, signer_state_persists_and_rejects_corruption)
{
  const boost::filesystem::path path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("chainlock-%%%%-%%%%");
  std::string error;
  chainlock_signer_state_v1 state;
  ASSERT_TRUE(load_chainlock_signer_state_v1(path.string(), state, error));
  EXPECT_EQ(0u, state.height);

  const chainlock_signer_state_v1 saved{4242, hash_text("signed")};
  ASSERT_TRUE(save_chainlock_signer_state_v1(path.string(), saved, error));
  ASSERT_TRUE(load_chainlock_signer_state_v1(path.string(), state, error));
  EXPECT_EQ(saved.height, state.height);
  EXPECT_EQ(saved.block_hash, state.block_hash);

  ASSERT_TRUE(epee::file_io_utils::save_string_to_file(path.string(), "4242 nothex\n"));
  EXPECT_FALSE(load_chainlock_signer_state_v1(path.string(), state, error));
  EXPECT_FALSE(error.empty());
  boost::filesystem::remove(path);
}
