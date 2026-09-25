// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "include_base_utils.h"

int main(int argc, char **argv)
{
  TRY_ENTRY();

  epee::debug::get_set_enable_assert(true, false);
  ::testing::InitGoogleTest(&argc, argv);

  CATCH_ENTRY_L0("main", 1);

  return RUN_ALL_TESTS();
}
