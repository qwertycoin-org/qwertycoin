// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/height_window.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_basic/verification_context.h"
#include "cryptonote_config.h"
#include "cryptonote_core/expired_window_tx_cache.h"
#include "epose/envelope_v2.h"

namespace
{
  using namespace cryptonote;
  using namespace qwertycoin::epose;

  std::vector<uint8_t> extra_with(uint64_t from, uint64_t until)
  {
    std::vector<uint8_t> extra;
    EXPECT_TRUE(add_height_window_to_tx_extra(extra, {from, until}));
    return extra;
  }

  envelope_limits_v2 limits()
  {
    envelope_limits_v2 out{};
    out.max_envelope_bytes = 1024;
    out.max_records = 4;
    out.max_record_payload_bytes = 512;
    out.max_signature_verifications = 8;
    out.max_admission_verifications = 2;
    return out;
  }

  transaction_prefix prefix_with(const std::vector<uint8_t> &extra, uint64_t unlock_time = 0)
  {
    transaction_prefix tx;
    tx.extra = extra;
    tx.unlock_time = unlock_time;
    return tx;
  }

  static const contract_extra_scope window_allowed = contract_extra_scope::transaction;
  static const contract_extra_scope window_forbidden = contract_extra_scope::coinbase;
  envelope_status_v2 strict_parse(const std::vector<uint8_t> &extra, uint8_t major, contract_extra_scope scope = window_allowed)
  {
    std::vector<envelope_record_v2> records;
    envelope_budget_v2 budget{};
    return parse_transaction_extra_v2(extra, major, 1, limits(), records, budget, scope);
  }
}

TEST(height_window, serialization_roundtrip_and_tag)
{
  const std::vector<uint8_t> extra = extra_with(300, 1000);
  ASSERT_FALSE(extra.empty());
  EXPECT_EQ(TX_EXTRA_TAG_HEIGHT_WINDOW, extra[0]);
  EXPECT_EQ(0x06, extra[0]);

  tx_extra_height_window out;
  ASSERT_EQ(1u, get_tx_height_window(extra, out));
  EXPECT_EQ(300u, out.from);
  EXPECT_EQ(1000u, out.until);

  const uint64_t big = std::numeric_limits<uint64_t>::max();
  const std::vector<uint8_t> big_extra = extra_with(big - 1, big);
  ASSERT_EQ(1u, get_tx_height_window(big_extra, out));
  EXPECT_EQ(big - 1, out.from);
  EXPECT_EQ(big, out.until);

  std::vector<uint8_t> none;
  EXPECT_EQ(0u, get_tx_height_window(none, out));
}

TEST(height_window, sorts_after_epose_and_before_nonce)
{
  std::vector<uint8_t> extra;
  ASSERT_TRUE(add_height_window_to_tx_extra(extra, {1, 2}));
  ASSERT_TRUE(add_extra_nonce_to_tx_extra(extra, "nonce"));
  crypto::public_key pk{};
  ASSERT_TRUE(add_tx_pub_key_to_extra(extra, pk));

  std::vector<uint8_t> sorted;
  ASSERT_TRUE(sort_tx_extra(extra, sorted));
  std::vector<tx_extra_field> fields;
  ASSERT_TRUE(parse_tx_extra(sorted, fields));
  ASSERT_EQ(3u, fields.size());
  EXPECT_EQ(typeid(tx_extra_pub_key), fields[0].type());
  EXPECT_EQ(typeid(tx_extra_height_window), fields[1].type());
  EXPECT_EQ(typeid(tx_extra_nonce), fields[2].type());
}

TEST(height_window, rejected_like_unknown_tag_before_fork_and_accepted_from_fork)
{
  const std::vector<uint8_t> extra = extra_with(10, 20);
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(extra, HF_VERSION_QWC_EPOSE));
  EXPECT_EQ(envelope_status_v2::accepted, strict_parse(extra, HF_VERSION_QWC_CONTRACTS));
  EXPECT_EQ(envelope_status_v2::accepted, strict_parse(extra, HF_VERSION_QWC_CONTRACTS + 1));

  const std::vector<uint8_t> unknown{0x7f, 0x01};
  EXPECT_EQ(strict_parse(unknown, HF_VERSION_QWC_EPOSE), strict_parse(extra, HF_VERSION_QWC_EPOSE));
}

TEST(height_window, coinbase_never_carries_the_tag)
{
  const std::vector<uint8_t> extra = extra_with(10, 20);
  EXPECT_EQ(envelope_status_v2::accepted, strict_parse(extra, HF_VERSION_QWC_CONTRACTS, window_allowed));
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(extra, HF_VERSION_QWC_CONTRACTS, window_forbidden));

  std::vector<tx_extra_field> fields;
  EXPECT_FALSE(parse_tx_extra_strict(extra, fields, true, false));
  EXPECT_TRUE(parse_tx_extra_strict(extra, fields, true, true));
}

TEST(height_window, duplicate_and_noncanonical_are_rejected)
{
  std::vector<uint8_t> twice = extra_with(1, 2);
  const std::vector<uint8_t> second = extra_with(3, 4);
  twice.insert(twice.end(), second.begin(), second.end());
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(twice, HF_VERSION_QWC_CONTRACTS));

  const std::vector<uint8_t> noncanonical{0x06, 0x80, 0x00, 0x05};
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(noncanonical, HF_VERSION_QWC_CONTRACTS));

  const std::vector<uint8_t> truncated{0x06, 0x05};
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(truncated, HF_VERSION_QWC_CONTRACTS));
}

TEST(height_window, boundaries_from_until_and_one_past)
{
  const tx_extra_height_window w{100, 200};
  EXPECT_EQ(height_window_verdict::early, check_height_window(w, 99));
  EXPECT_EQ(height_window_verdict::inside, check_height_window(w, 100));
  EXPECT_EQ(height_window_verdict::inside, check_height_window(w, 150));
  EXPECT_EQ(height_window_verdict::inside, check_height_window(w, 200));
  EXPECT_EQ(height_window_verdict::expired, check_height_window(w, 201));

  EXPECT_EQ(height_window_verdict::inside, check_height_window({5, 5}, 5));
  EXPECT_EQ(height_window_verdict::malformed, check_height_window({6, 5}, 5));
  EXPECT_EQ(height_window_verdict::malformed, check_height_window({6, 5}, 6));
}

TEST(height_window, tx_extra_verdict_counts_windows)
{
  tx_extra_height_window w{};
  EXPECT_EQ(height_window_verdict::absent, check_tx_height_window(prefix_with({}), 150, w));
  EXPECT_EQ(height_window_verdict::inside, check_tx_height_window(prefix_with(extra_with(100, 200)), 150, w));
  std::vector<uint8_t> twice = extra_with(100, 200);
  ASSERT_TRUE(add_height_window_to_tx_extra(twice, {100, 200}));
  EXPECT_EQ(height_window_verdict::malformed, check_tx_height_window(prefix_with(twice), 150, w));
}

TEST(height_window, timestamp_unlock_time_makes_window_malformed)
{
  tx_extra_height_window w{};
  const std::vector<uint8_t> windowed = extra_with(100, 200);
  EXPECT_EQ(height_window_verdict::inside, check_tx_height_window(prefix_with(windowed, 0), 150, w));
  EXPECT_EQ(height_window_verdict::inside, check_tx_height_window(prefix_with(windowed, CRYPTONOTE_MAX_BLOCK_NUMBER - 1), 150, w));
  EXPECT_EQ(height_window_verdict::malformed, check_tx_height_window(prefix_with(windowed, CRYPTONOTE_MAX_BLOCK_NUMBER), 150, w));
  EXPECT_EQ(height_window_verdict::malformed, check_tx_height_window(prefix_with(windowed, 1787788800), 150, w));
  EXPECT_EQ(height_window_verdict::malformed, check_tx_height_window(prefix_with(windowed, CRYPTONOTE_MAX_BLOCK_NUMBER), 250, w));
  EXPECT_EQ(height_window_verdict::absent, check_tx_height_window(prefix_with({}, CRYPTONOTE_MAX_BLOCK_NUMBER), 150, w));
}

TEST(height_window, pool_early_is_lead_ten_beyond_next_height)
{
  const uint64_t next = 1000;
  const uint64_t lead = QWC_WINDOW_POOL_LEAD_BLOCKS;
  const uint64_t max = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(height_window_verdict::inside, check_height_window({next, next + 100}, next, lead));
  EXPECT_EQ(height_window_verdict::inside, check_height_window({next + 10, next + 100}, next, lead));
  EXPECT_EQ(height_window_verdict::early, check_height_window({next + 11, next + 100}, next, lead));
  EXPECT_EQ(height_window_verdict::inside, check_height_window({max, max}, max, lead));
}

TEST(height_window, pool_meta_stores_window_in_padding_without_changing_size)
{
  static_assert(sizeof(txpool_tx_meta_t) == 192, "meta size");
  txpool_tx_meta_t meta{};
  uint64_t from = 0, until = 0;
  EXPECT_FALSE(meta.get_height_window(from, until));
  meta.set_height_window(123456789012345ull, 987654321098765ull);
  ASSERT_TRUE(meta.get_height_window(from, until));
  EXPECT_EQ(123456789012345ull, from);
  EXPECT_EQ(987654321098765ull, until);
  meta.clear_height_window();
  EXPECT_FALSE(meta.get_height_window(from, until));

  txpool_tx_meta_t legacy;
  memset(&legacy, 0, sizeof(legacy));
  EXPECT_FALSE(legacy.get_height_window(from, until));
}

TEST(height_window, verification_context_has_pool_flags)
{
  tx_verification_context tvc{};
  EXPECT_FALSE(tvc.m_window_early);
  EXPECT_FALSE(tvc.m_window_expired);
}

TEST(height_window, effective_unlock_boundaries)
{
  const uint64_t until = 1000;
  const tx_extra_height_window w{900, until};
  const uint64_t delay = QWC_WINDOW_UNLOCK_DELAY_BLOCKS;
  ASSERT_EQ(10u, delay);

  EXPECT_EQ(0u, height_window_effective_unlock(0, 900, w));
  EXPECT_EQ(0u, height_window_effective_unlock(0, until - 11, w));
  EXPECT_EQ(until + 10, height_window_effective_unlock(0, until - 10, w));
  EXPECT_EQ(until + 10, height_window_effective_unlock(0, until - 1, w));
  EXPECT_EQ(until + 10, height_window_effective_unlock(0, until, w));

  EXPECT_EQ(5000u, height_window_effective_unlock(5000, until, w));
  EXPECT_EQ(CRYPTONOTE_MAX_BLOCK_NUMBER + 5, height_window_effective_unlock(CRYPTONOTE_MAX_BLOCK_NUMBER + 5, until, w));
  EXPECT_EQ(CRYPTONOTE_MAX_BLOCK_NUMBER - 1, height_window_effective_unlock(0, 600000000ull - 5, {0, 600000000ull}));
}

TEST(height_window, effective_unlock_agrees_with_wallet_spendable_age_ten)
{
  const uint64_t age = CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE;
  ASSERT_EQ(QWC_WINDOW_UNLOCK_DELAY_BLOCKS, age);
  for (uint64_t until = 100; until < 110; ++until)
  {
    const tx_extra_height_window w{1, until};
    for (uint64_t m = until - 20; m <= until; ++m)
    {
      const uint64_t eff = height_window_effective_unlock(0, m, w);
      const uint64_t wallet_first_spendable = m + age;
      EXPECT_GE(std::max(eff, wallet_first_spendable), wallet_first_spendable);
      if (m == until)
        EXPECT_EQ(wallet_first_spendable, std::max(eff, wallet_first_spendable));
      if (m + age < until)
        EXPECT_EQ(0u, eff);
      else
        EXPECT_EQ(until + age, eff);
    }
  }
}

TEST(height_window, wallet_default_window_rounding)
{
  EXPECT_EQ(800u, height_window_default_until(0));
  EXPECT_EQ(800u, height_window_default_until(80));
  EXPECT_EQ(800u, height_window_default_until(79));
  EXPECT_EQ(900u, height_window_default_until(81));
  EXPECT_EQ(1800u, height_window_default_until(1000));
}

TEST(height_window, block_below_contracts_fork_rejects_any_window_tag)
{
  tx_extra_height_window window{};
  const transaction_prefix tagged = prefix_with(extra_with(10, 20));
  const transaction_prefix plain = prefix_with({});
  for (const uint8_t major : {HF_VERSION_QWC_EPOSE - 1, HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_CONTRACTS - 1})
  {
    EXPECT_EQ(height_window_verdict::malformed, check_block_tx_height_window(tagged, major, 15, window));
    EXPECT_EQ(height_window_verdict::absent, check_block_tx_height_window(plain, major, 15, window));
  }
  EXPECT_EQ(height_window_verdict::inside, check_block_tx_height_window(tagged, HF_VERSION_QWC_CONTRACTS, 15, window));
  EXPECT_EQ(height_window_verdict::expired, check_block_tx_height_window(tagged, HF_VERSION_QWC_CONTRACTS, 21, window));
  EXPECT_EQ(height_window_verdict::absent, check_block_tx_height_window(plain, HF_VERSION_QWC_CONTRACTS, 15, window));
}

namespace
{
  crypto::hash hash_of(const uint8_t seed)
  {
    crypto::hash out{};
    out.data[0] = seed;
    return out;
  }
}

TEST(expired_window_tx_cache, take_returns_the_blob_once)
{
  expired_window_tx_cache cache(720, 1024);
  cache.insert(hash_of(1), "abc", 100);
  EXPECT_TRUE(cache.contains(hash_of(1)));
  EXPECT_EQ(3u, cache.bytes());
  blobdata blob;
  EXPECT_TRUE(cache.take(hash_of(1), blob));
  EXPECT_EQ("abc", blob);
  EXPECT_FALSE(cache.contains(hash_of(1)));
  EXPECT_EQ(0u, cache.bytes());
  EXPECT_FALSE(cache.take(hash_of(1), blob));
}

TEST(expired_window_tx_cache, read_does_not_consume_transaction)
{
  expired_window_tx_cache cache(720, 1024);
  const crypto::hash id = hash_of(5);
  cache.insert(id, "transaction", 8);
  blobdata first;
  blobdata second;
  EXPECT_TRUE(cache.get(id, first));
  EXPECT_TRUE(cache.take(id, second));
  EXPECT_EQ(first, second);
  EXPECT_FALSE(cache.get(id, first));
}

TEST(expired_window_tx_cache, evicts_after_retention_blocks)
{
  expired_window_tx_cache cache(720, 1024);
  cache.insert(hash_of(1), "a", 100);
  cache.insert(hash_of(2), "b", 200);
  cache.evict_expired(820);
  EXPECT_TRUE(cache.contains(hash_of(1)));
  cache.evict_expired(821);
  EXPECT_FALSE(cache.contains(hash_of(1)));
  EXPECT_TRUE(cache.contains(hash_of(2)));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(1u, cache.bytes());
}

TEST(expired_window_tx_cache, byte_cap_drops_oldest_and_rejects_oversized)
{
  expired_window_tx_cache cache(720, 10);
  cache.insert(hash_of(1), "aaaa", 100);
  cache.insert(hash_of(2), "bbbb", 101);
  cache.insert(hash_of(3), "cccc", 102);
  EXPECT_FALSE(cache.contains(hash_of(1)));
  EXPECT_TRUE(cache.contains(hash_of(2)));
  EXPECT_TRUE(cache.contains(hash_of(3)));
  EXPECT_EQ(8u, cache.bytes());
  cache.insert(hash_of(4), "0123456789a", 103);
  EXPECT_FALSE(cache.contains(hash_of(4)));
  EXPECT_EQ(8u, cache.bytes());
}

TEST(expired_window_tx_cache, reinsert_replaces_and_clear_empties)
{
  expired_window_tx_cache cache(720, 1024);
  cache.insert(hash_of(1), "aa", 100);
  cache.insert(hash_of(1), "bbb", 300);
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(3u, cache.bytes());
  cache.evict_expired(821);
  EXPECT_TRUE(cache.contains(hash_of(1)));
  cache.clear();
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(0u, cache.bytes());
}
