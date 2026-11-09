// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <set>
#include <string>

#include "epose/reward_v2.h"

using namespace qwertycoin::epose;

TEST(epose_reward_status_name_v2, every_status_has_a_distinct_name)
{
  std::set<std::string> names;
  for (reward_status_v2 status : {
           reward_status_v2::accepted,
           reward_status_v2::invalid_basis_points,
           reward_status_v2::unresolved_empty_policy,
           reward_status_v2::arithmetic_overflow,
           reward_status_v2::invalid_height,
           reward_status_v2::empty_qualification,
           reward_status_v2::invalid_context,
           reward_status_v2::invalid_transaction_key,
           reward_status_v2::invalid_derivation,
           reward_status_v2::invalid_output_allocation,
           reward_status_v2::invalid_payment_proof})
    names.insert(reward_status_name_v2(status));
  EXPECT_EQ(11u, names.size());
  EXPECT_EQ(0u, names.count("unknown"));
}
