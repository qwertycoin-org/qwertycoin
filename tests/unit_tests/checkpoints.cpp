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

  constexpr std::array<expected_checkpoint, 6> expected_mainnet_checkpoints{{
    {0,    "4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39", "0x1"},
    {719,  "69cf9a283099298d1ea9c71b14b0f1d73c7ca81c87e4a2303c1ea91ff8c5b68d", "0xde51cae4"},
    {1439, "019b6a911b0fd41f6e83740f1c4c3fc83f5299339b62cb532b48c97b1e2c35f1", "0x3705926df"},
    {2159, "011bdb8505cc457a3b4be6deccd8fb7fcd55849075b67cbac1f1601b034860a0", "0x7a9125791"},
    {2879, "cc954d4cb4352affc224a9191fb356932e4a9ec405e89b4932654fb1737a50d0", "0x13e28e554a"},
    {3599, "75c7cff8db59f803962fa86ab79a7429ebd3cd2025968bbe80d5744554ba35db", "0x246cae087a"},
  }};
}


TEST(checkpoints_default_mainnet, includes_completed_epose_epoch_boundaries)
{
  checkpoints cp;
  ASSERT_TRUE(cp.init_default_checkpoints(MAINNET));

  ASSERT_EQ(expected_mainnet_checkpoints.size(), cp.get_points().size());
  ASSERT_EQ(expected_mainnet_checkpoints.size(), cp.get_difficulty_points().size());
  EXPECT_EQ(3599u, cp.get_max_height());
  EXPECT_TRUE(cp.is_in_checkpoint_zone(3599));
  EXPECT_FALSE(cp.is_in_checkpoint_zone(3600));

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

  EXPECT_FALSE(cp.is_alternative_block_allowed(3599, 3599));
  EXPECT_FALSE(cp.is_alternative_block_allowed(4000, 3599));
  EXPECT_TRUE(cp.is_alternative_block_allowed(3599, 3600));
  EXPECT_TRUE(cp.is_alternative_block_allowed(4000, 3600));
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
