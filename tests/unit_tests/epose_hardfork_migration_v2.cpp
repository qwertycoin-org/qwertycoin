// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <deque>
#include <limits>
#include <vector>

#include "gtest/gtest.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "blockchain_db/testdb.h"
#include "cryptonote_basic/hardfork.h"
#include "cryptonote_config.h"
#include "hardforks/hardforks.h"

namespace
{
  class hardfork_test_db final : public cryptonote::BaseTestDB
  {
  public:
    uint64_t height() const override { return blocks_.size(); }

    void add_block(
        const cryptonote::block &block,
        size_t,
        uint64_t,
        const cryptonote::difficulty_type &,
        const uint64_t &,
        uint64_t,
        const crypto::hash &) override
    {
      blocks_.push_back(block);
    }

    void remove_block() override { blocks_.pop_back(); }

    cryptonote::block get_block_from_height(const uint64_t &height) const override
    {
      return blocks_.at(height);
    }

    void set_hard_fork_version(uint64_t height, uint8_t version) override
    {
      if (versions_.size() <= height)
        versions_.resize(height + 1);
      versions_[height] = version;
    }

    uint8_t get_hard_fork_version(uint64_t height) const override
    {
      return versions_.at(height);
    }

  private:
    std::vector<cryptonote::block> blocks_;
    std::deque<uint8_t> versions_;
  };

  cryptonote::block block_with_version(uint8_t version)
  {
    cryptonote::block block{};
    block.major_version = version;
    block.minor_version = version;
    return block;
  }
}

TEST(epose_hardfork_migration_v2, scheduled_future_version_keeps_epose_active)
{
  EXPECT_FALSE(is_qwc_epose_v2_hardfork(HF_VERSION_MONERO_CURRENT_CONSENSUS));
  EXPECT_TRUE(is_qwc_epose_v2_hardfork(HF_VERSION_QWC_EPOSE));
  EXPECT_TRUE(is_qwc_epose_v2_hardfork(
      HF_VERSION_QWC_EPOSE_RELAY_HARDENING));
  EXPECT_TRUE(is_qwc_epose_v2_hardfork(std::numeric_limits<uint8_t>::max()));
  EXPECT_FALSE(is_qwc_epose_relay_hardening_hardfork(HF_VERSION_QWC_EPOSE));
  EXPECT_TRUE(is_qwc_epose_relay_hardening_hardfork(
      HF_VERSION_QWC_EPOSE_RELAY_HARDENING));

  hardfork_test_db db;
  cryptonote::HardFork hardfork(db, HF_VERSION_QWC_EPOSE, 0, 0, 0, 1, 0);
  ASSERT_TRUE(hardfork.add_fork(HF_VERSION_QWC_EPOSE, 0, 0));
  ASSERT_TRUE(hardfork.add_fork(
      HF_VERSION_QWC_EPOSE_RELAY_HARDENING, 3, 1));
  hardfork.init();

  for (uint64_t height = 0; height < 6; ++height)
  {
    const uint8_t expected = height < 3
        ? HF_VERSION_QWC_EPOSE
        : HF_VERSION_QWC_EPOSE_RELAY_HARDENING;
    ASSERT_EQ(expected, hardfork.get_ideal_version(height));
    const cryptonote::block candidate = block_with_version(expected);
    ASSERT_TRUE(hardfork.check_for_height(candidate, height));
    db.add_block(candidate, 0, 0, 0, 0, 0, crypto::hash{});
    ASSERT_TRUE(hardfork.add(db.get_block_from_height(height), height));
  }

  EXPECT_EQ(HF_VERSION_QWC_EPOSE_RELAY_HARDENING,
      hardfork.get_current_version());
}

TEST(epose_hardfork_migration_v2, mainnet_relay_hardening_is_hf18_at_height_20000)
{
  ASSERT_EQ(2u, num_mainnet_hard_forks);
  EXPECT_EQ(HF_VERSION_QWC_EPOSE, mainnet_hard_forks[0].version);
  EXPECT_EQ(0u, mainnet_hard_forks[0].height);
  EXPECT_EQ(HF_VERSION_QWC_EPOSE_RELAY_HARDENING,
      mainnet_hard_forks[1].version);
  EXPECT_EQ(QWC_EPOSE_RELAY_HARDENING_HEIGHT,
      mainnet_hard_forks[1].height);
  EXPECT_EQ(0u, mainnet_hard_forks[1].threshold);
  EXPECT_GT(mainnet_hard_forks[1].time, mainnet_hard_forks[0].time);

  hardfork_test_db db;
  cryptonote::HardFork hardfork(db, HF_VERSION_QWC_EPOSE, 0, 0, 0, 1, 0);
  for (size_t index = 0; index < num_mainnet_hard_forks; ++index)
    ASSERT_TRUE(hardfork.add_fork(
        mainnet_hard_forks[index].version,
        mainnet_hard_forks[index].height,
        mainnet_hard_forks[index].threshold,
        mainnet_hard_forks[index].time));
  hardfork.init();

  EXPECT_EQ(HF_VERSION_QWC_EPOSE,
      hardfork.get_ideal_version(QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  EXPECT_EQ(HF_VERSION_QWC_EPOSE_RELAY_HARDENING,
      hardfork.get_ideal_version(QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  EXPECT_TRUE(hardfork.check_for_height(
      block_with_version(HF_VERSION_QWC_EPOSE),
      QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  EXPECT_FALSE(hardfork.check_for_height(
      block_with_version(HF_VERSION_QWC_EPOSE_RELAY_HARDENING),
      QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  EXPECT_FALSE(hardfork.check_for_height(
      block_with_version(HF_VERSION_QWC_EPOSE),
      QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  EXPECT_TRUE(hardfork.check_for_height(
      block_with_version(HF_VERSION_QWC_EPOSE_RELAY_HARDENING),
      QWC_EPOSE_RELAY_HARDENING_HEIGHT));
}
