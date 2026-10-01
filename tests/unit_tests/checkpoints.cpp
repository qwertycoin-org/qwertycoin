// Copyright (c) 2014-2022, The Monero Project
// Copyright (c) 2026, The Qwertycoin Project
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
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"

#include "checkpoints/checkpoints.cpp"

#include <array>

using namespace cryptonote;

namespace
{
  struct expected_checkpoint
  {
    uint64_t height;
    const char* hash;
    const char* cumulative_difficulty;
  };

  constexpr std::array<expected_checkpoint, 18> expected_mainnet_checkpoints{{
    {0,    "4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39", "0x1"},
    {719,  "69cf9a283099298d1ea9c71b14b0f1d73c7ca81c87e4a2303c1ea91ff8c5b68d", "0xde51cae4"},
    {1439, "019b6a911b0fd41f6e83740f1c4c3fc83f5299339b62cb532b48c97b1e2c35f1", "0x3705926df"},
    {2159, "011bdb8505cc457a3b4be6deccd8fb7fcd55849075b67cbac1f1601b034860a0", "0x7a9125791"},
    {2879, "cc954d4cb4352affc224a9191fb356932e4a9ec405e89b4932654fb1737a50d0", "0x13e28e554a"},
    {3599, "75c7cff8db59f803962fa86ab79a7429ebd3cd2025968bbe80d5744554ba35db", "0x246cae087a"},
    {4319, "b1be6607441fdd441264f5827401207e58052937f7a600287b442c1808d4160e", "0x39566a3802"},
    {5039, "55ac2256275255e99628c19e9fbfb3eb8708016a68af87f9d327f253ea150ecc", "0x4a90d5fca8"},
    {5759, "a39a08b8d95157e6ea39e33e91a2a7c1f25141db36f931911629b9e96d61737b", "0x5b4dc1a177"},
    {6479, "4c4d825d6c1d56e4a173658c7e5dd001d76050ff6b620407d74f5069f3d12996", "0x6c49228c02"},
    {7199, "5f69ac8432e5572d55a4cfca05c4cdef3964a95a09fe4b61ecb384908075072e", "0x7d83b8cccc"},
    {7919, "ae157dc07f24fb8075cfda96bcff3e0c2c303c07f09b37674465ba47a3719b9e", "0x908fdebe23"},
    {8639, "d60ea59c24852ef0c1bc12e25954550bae4592e94437182c0fc14c3755f3015c", "0xa30db806e8"},
    {9359, "1a19876876537d20a2eea9ff6bfb1e55beb9687a666737ad462c28d870a31c4c", "0xb153f7b7cd"},
    {10079, "24a4a4dca58f94bcdeac71795ce0e434023d9dfc086b72b40238dadd5f690998", "0xbb0d648773"},
    {10799, "4c9b3093213620d56a5a26bf58a2a7423e212cd2bc0483eb3c8ca84f8346f5ed", "0xc4611179c0"},
    {11519, "65abb7baad939246dc1dd9c0b5ac04f0e3cd5bc0ee1dca34a6abd873eabda935", "0xcde8b8a389"},
    {12239, "e515a18cdcb584963c19ce289ed064c8bb78d8b51920363e97847da9d6ca01dd", "0xe00312c3d8"},
  }};
}


TEST(checkpoints_default_mainnet, includes_completed_epose_epoch_boundaries)
{
  checkpoints cp;
  ASSERT_TRUE(cp.init_default_checkpoints(MAINNET));

  ASSERT_EQ(expected_mainnet_checkpoints.size(), cp.get_points().size());
  ASSERT_EQ(expected_mainnet_checkpoints.size(), cp.get_difficulty_points().size());
  const uint64_t latest_checkpoint_height = expected_mainnet_checkpoints.back().height;
  EXPECT_EQ(latest_checkpoint_height, cp.get_max_height());
  EXPECT_TRUE(cp.is_in_checkpoint_zone(latest_checkpoint_height));
  EXPECT_FALSE(cp.is_in_checkpoint_zone(latest_checkpoint_height + 1));

  for (const expected_checkpoint& expected : expected_mainnet_checkpoints)
  {
    crypto::hash expected_hash = crypto::null_hash;
    ASSERT_TRUE(epee::string_tools::hex_to_pod(expected.hash, expected_hash));

    bool is_checkpoint = false;
    EXPECT_TRUE(cp.check_block(expected.height, expected_hash, is_checkpoint));
    EXPECT_TRUE(is_checkpoint);
    EXPECT_EQ(expected_hash, cp.get_points().at(expected.height));
    EXPECT_EQ(difficulty_type(expected.cumulative_difficulty), cp.get_difficulty_points().at(expected.height));

    if (expected.height != 0)
    {
      EXPECT_EQ(719u, expected.height % 720u);
    }
  }
}

TEST(checkpoints_default_mainnet, rejects_conflicts_at_or_below_latest_checkpoint)
{
  checkpoints cp;
  ASSERT_TRUE(cp.init_default_checkpoints(MAINNET));

  for (const expected_checkpoint& expected : expected_mainnet_checkpoints)
  {
    bool is_checkpoint = false;
    EXPECT_FALSE(cp.check_block(expected.height, crypto::null_hash, is_checkpoint));
    EXPECT_TRUE(is_checkpoint);
  }

  const uint64_t latest_checkpoint_height = expected_mainnet_checkpoints.back().height;
  EXPECT_FALSE(cp.is_alternative_block_allowed(latest_checkpoint_height, latest_checkpoint_height));
  EXPECT_FALSE(cp.is_alternative_block_allowed(latest_checkpoint_height + 1000, latest_checkpoint_height));
  EXPECT_TRUE(cp.is_alternative_block_allowed(latest_checkpoint_height, latest_checkpoint_height + 1));
  EXPECT_TRUE(cp.is_alternative_block_allowed(latest_checkpoint_height + 1000, latest_checkpoint_height + 1));
}


TEST(checkpoints_is_alternative_block_allowed, handles_empty_checkpoints)
{
  checkpoints cp;

  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE(cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE(cp.is_alternative_block_allowed(1, 9));
  ASSERT_TRUE(cp.is_alternative_block_allowed(9, 1));
}

TEST(checkpoints_is_alternative_block_allowed, handles_one_checkpoint)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(5, "0000000000000000000000000000000000000000000000000000000000000000"));

  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 9));

  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 9));

  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 9));

  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 9));

  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 9));
}

TEST(checkpoints_is_alternative_block_allowed, handles_two_and_more_checkpoints)
{
  checkpoints cp;
  ASSERT_TRUE(cp.add_checkpoint(5, "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_TRUE(cp.add_checkpoint(9, "0000000000000000000000000000000000000000000000000000000000000000"));

  ASSERT_FALSE(cp.is_alternative_block_allowed(0, 0));

  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(1, 11));

  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 1));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 4));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(4, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(5, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(5, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(6, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(6, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(8, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(8, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(8, 5));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 6));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 8));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(8, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 6));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 8));
  ASSERT_FALSE(cp.is_alternative_block_allowed(9, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(9, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 6));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 8));
  ASSERT_FALSE(cp.is_alternative_block_allowed(10, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(10, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(10, 11));

  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 1));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 4));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 5));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 6));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 8));
  ASSERT_FALSE(cp.is_alternative_block_allowed(11, 9));
  ASSERT_TRUE (cp.is_alternative_block_allowed(11, 10));
  ASSERT_TRUE (cp.is_alternative_block_allowed(11, 11));
}
