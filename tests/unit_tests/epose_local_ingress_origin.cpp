#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <limits>

#include "cryptonote_core/epose_warning_throttle.h"

TEST(epose_local_ingress_origin, rpc_uses_its_own_throttle_without_step_failure)
{
  constexpr uint64_t unset = std::numeric_limits<uint64_t>::max();
  std::atomic<uint64_t> enrollment{unset};
  std::atomic<uint64_t> receipt{unset};
  std::atomic<uint64_t> rpc{unset};
  std::atomic<uint64_t> failures{0};
  const auto rejected = [&](cryptonote::epose_local_ingress_origin origin) {
    return cryptonote::epose_local_ingress_rejection_warning(
        origin, 7, enrollment, receipt, rpc, failures);
  };

  EXPECT_TRUE(rejected(cryptonote::epose_local_ingress_origin::rpc_submit));
  EXPECT_FALSE(rejected(cryptonote::epose_local_ingress_origin::rpc_submit));
  EXPECT_EQ(0u, failures.load());
  EXPECT_EQ(unset, enrollment.load());
  EXPECT_EQ(unset, receipt.load());
  EXPECT_EQ(7u, rpc.load());

  EXPECT_TRUE(rejected(cryptonote::epose_local_ingress_origin::enrollment));
  EXPECT_TRUE(rejected(cryptonote::epose_local_ingress_origin::receipt));
  EXPECT_EQ(2u, failures.load());
  EXPECT_EQ(7u, enrollment.load());
  EXPECT_EQ(7u, receipt.load());
  EXPECT_FALSE(rejected(cryptonote::epose_local_ingress_origin::enrollment));
  EXPECT_EQ(3u, failures.load());
}
