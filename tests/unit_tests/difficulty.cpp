// Copyright (c) 2019-2022, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gtest/gtest.h"
#include "int-util.h"
#include "cryptonote_basic/difficulty.h"
#include "cryptonote_config.h"
#include "cryptonote_core/block_rate_probability.h"

static cryptonote::difficulty_type MKDIFF(uint64_t high, uint64_t low)
{
  cryptonote::difficulty_type d = high;
  d = (d << 64) | low;
  return d;
}

static crypto::hash MKHASH(uint64_t high, uint64_t low)
{
  cryptonote::difficulty_type hash_target = high;
  hash_target = (hash_target << 64) | low;
  boost::multiprecision::uint256_t hash_value = std::numeric_limits<boost::multiprecision::uint256_t>::max() / hash_target;
  crypto::hash h;
  uint64_t val;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[0] = SWAP64LE(val);
  hash_value >>= 64;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[1] = SWAP64LE(val);
  hash_value >>= 64;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[2] = SWAP64LE(val);
  hash_value >>= 64;
  val = (hash_value & 0xffffffffffffffff).convert_to<uint64_t>();
  ((uint64_t*)&h)[3] = SWAP64LE(val);
  return h;
}

TEST(difficulty, check_hash)
{
  ASSERT_TRUE(cryptonote::check_hash(MKHASH(0, 1), MKDIFF(0, 1)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(0, 1), MKDIFF(0, 2)));

  ASSERT_TRUE(cryptonote::check_hash(MKHASH(0, 0xffffffffffffffff), MKDIFF(0, 0xffffffffffffffff)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(0, 0xffffffffffffffff), MKDIFF(1, 0)));

  ASSERT_TRUE(cryptonote::check_hash(MKHASH(1, 1), MKDIFF(1, 1)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(1, 1), MKDIFF(1, 2)));

  ASSERT_TRUE(cryptonote::check_hash(MKHASH(0xffffffffffffffff, 1), MKDIFF(0xffffffffffffffff, 1)));
  ASSERT_FALSE(cryptonote::check_hash(MKHASH(0xffffffffffffffff, 1), MKDIFF(0xffffffffffffffff, 2)));
}

TEST(difficulty, network_block_targets)
{
  EXPECT_EQ(15u, cryptonote::block_target(cryptonote::DEVNET));
  EXPECT_EQ(120u, cryptonote::block_target(cryptonote::MAINNET));
  EXPECT_EQ(120u, cryptonote::block_target(cryptonote::TESTNET));
  EXPECT_EQ(120u, cryptonote::block_target(cryptonote::STAGENET));
  EXPECT_EQ(60u, cryptonote::block_target(cryptonote::DEVNET, 1));
  EXPECT_EQ(15u, cryptonote::locked_tx_allowed_delta_seconds(cryptonote::DEVNET, 2));
  EXPECT_EQ(120u, cryptonote::locked_tx_allowed_delta_seconds(cryptonote::MAINNET, 2));
}

TEST(difficulty, round_up_division_does_not_overflow)
{
  EXPECT_EQ(1317624576693539402u, cryptonote::divide_round_up(UINT64_MAX, 14));
  EXPECT_EQ(1u, cryptonote::divide_round_up(1, 15));
}

TEST(difficulty, network_block_rate_sample_windows)
{
  EXPECT_EQ(360u, cryptonote::blocks_for_duration(cryptonote::DEVNET, 5400));
  EXPECT_EQ(45u, cryptonote::blocks_for_duration(cryptonote::MAINNET, 5400));
  EXPECT_EQ(1u, cryptonote::blocks_for_duration(cryptonote::DEVNET, 1));
}

TEST(difficulty, devnet_block_rate_probability_is_finite)
{
  const double normal_rate = cryptonote::block_rate_probability(360, 360);
  EXPECT_TRUE(std::isfinite(normal_rate));
  EXPECT_GT(normal_rate, 0.4);
  EXPECT_LT(normal_rate, 0.6);
  EXPECT_LT(cryptonote::block_rate_probability(150, 360), 1.0 / 57600);
}

TEST(difficulty, devnet_retarget_tracks_fifteen_second_spacing)
{
  std::vector<uint64_t> timestamps;
  std::vector<cryptonote::difficulty_type> cumulative_difficulties;
  for (uint64_t i = 0; i < DIFFICULTY_WINDOW; ++i)
  {
    timestamps.push_back(i * cryptonote::block_target(cryptonote::DEVNET));
    cumulative_difficulties.push_back(i * 100);
  }
  EXPECT_EQ(100, cryptonote::next_difficulty(timestamps, cumulative_difficulties,
      cryptonote::block_target(cryptonote::DEVNET)));
  EXPECT_EQ(800, cryptonote::next_difficulty(timestamps, cumulative_difficulties,
      cryptonote::block_target(cryptonote::MAINNET)));
  for (auto &timestamp : timestamps)
    timestamp = timestamp * 2 / 3;
  EXPECT_EQ(150, cryptonote::next_difficulty(timestamps, cumulative_difficulties,
      cryptonote::block_target(cryptonote::DEVNET)));
  for (auto &timestamp : timestamps)
    timestamp *= 3;
  EXPECT_EQ(50, cryptonote::next_difficulty(timestamps, cumulative_difficulties,
      cryptonote::block_target(cryptonote::DEVNET)));
}
