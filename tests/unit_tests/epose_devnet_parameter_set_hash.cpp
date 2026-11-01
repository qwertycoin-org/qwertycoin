// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include <cstring>

#include "epose/compiled_profile_v2.h"

TEST(epose_devnet_parameter_set_hash, differs_from_the_mainnet_and_the_retired_epoch_30_profile)
{
  using namespace qwertycoin::epose;
  EXPECT_STRNE(MAINNET_FINAL_PARAMETER_SET_HASH_V2, DEVNET_PARAMETER_SET_HASH_V2);
  EXPECT_STRNE(
      "750a4367734ca00c8e732e504f2fcd572391ed904700e66ea563fc64925bedd9",
      DEVNET_PARAMETER_SET_HASH_V2);
  EXPECT_EQ(64u, std::strlen(DEVNET_PARAMETER_SET_HASH_V2));
}
