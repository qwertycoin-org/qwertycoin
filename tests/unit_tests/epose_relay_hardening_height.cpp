// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose/relay_hardening_v2.h"

using qwertycoin::epose::relay_hardening_height_v2;

TEST(epose_relay_hardening_height, devnet_hardens_the_relay_from_genesis_so_redelivery_is_always_bounded)
{
  EXPECT_EQ(0u, relay_hardening_height_v2(cryptonote::DEVNET));
}

TEST(epose_relay_hardening_height, public_networks_keep_the_launch_height)
{
  EXPECT_EQ(QWC_EPOSE_RELAY_HARDENING_HEIGHT,
      relay_hardening_height_v2(cryptonote::MAINNET));
  EXPECT_EQ(QWC_EPOSE_RELAY_HARDENING_HEIGHT,
      relay_hardening_height_v2(cryptonote::TESTNET));
  EXPECT_EQ(QWC_EPOSE_RELAY_HARDENING_HEIGHT,
      relay_hardening_height_v2(cryptonote::STAGENET));
}
