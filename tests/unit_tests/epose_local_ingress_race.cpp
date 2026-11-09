// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose/relay_pool_v2.h"

using namespace qwertycoin::epose;

TEST(epose_local_ingress_race, expiry_and_retained_window_are_expected_transport_races)
{
  EXPECT_TRUE(local_ingress_is_expected_race_v2(relay_ingress_status_v2::expired));
  EXPECT_TRUE(local_ingress_is_expected_race_v2(relay_ingress_status_v2::outside_retained_window));
  EXPECT_FALSE(local_ingress_is_expected_race_v2(relay_ingress_status_v2::accepted));
  EXPECT_FALSE(local_ingress_is_expected_race_v2(relay_ingress_status_v2::invalid_configuration));
  EXPECT_FALSE(local_ingress_is_expected_race_v2(relay_ingress_status_v2::invalid_batch));
  EXPECT_FALSE(local_ingress_is_expected_race_v2(relay_ingress_status_v2::capacity_exhausted));
}
