#include <gtest/gtest.h>

#include "epose/service_producer_v2.h"

TEST(epose_receipt_launch, distinct_slots_can_launch_without_global_delay_after_activation)
{
  using qwertycoin::epose::receipt_launch_allowed_v2;
  const auto interval = std::chrono::seconds(1);
  EXPECT_FALSE(receipt_launch_allowed_v2(HF_VERSION_QWC_CONTRACTS - 1,
      true, std::chrono::milliseconds(1), interval));
  EXPECT_TRUE(receipt_launch_allowed_v2(HF_VERSION_QWC_CONTRACTS,
      true, std::chrono::milliseconds(1), interval));
  EXPECT_TRUE(receipt_launch_allowed_v2(HF_VERSION_QWC_CONTRACTS - 1,
      true, interval, interval));
}
