#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <limits>

#include "cryptonote_core/epose_warning_throttle.h"

TEST(epose_warning_throttle, once_per_epoch_for_each_warning)
{
  std::atomic<uint64_t> current{std::numeric_limits<uint64_t>::max()};
  std::atomic<uint64_t> target{std::numeric_limits<uint64_t>::max()};
  std::atomic<uint64_t> ingress{std::numeric_limits<uint64_t>::max()};
  std::atomic<uint64_t> receipt{std::numeric_limits<uint64_t>::max()};

  for (uint64_t epoch : {0u, 1u, 2u})
  {
    for (auto *warning : {&current, &target, &ingress, &receipt})
    {
      EXPECT_TRUE(cryptonote::epose_warning_once_per_epoch(*warning, epoch));
      EXPECT_FALSE(cryptonote::epose_warning_once_per_epoch(*warning, epoch));
    }
  }
}

TEST(epose_warning_throttle, invalid_enrollment_counts_each_retry_and_warns_once_per_epoch)
{
  std::atomic<uint64_t> warning_epoch{std::numeric_limits<uint64_t>::max()};
  std::atomic<uint64_t> failures{0};
  using cryptonote::epose_enrollment_failure_action;
  using qwertycoin::epose::service_producer_status_v2;
  const auto outcome = [&](service_producer_status_v2 status, bool contracts_active,
                           uint64_t epoch) {
    return cryptonote::epose_enrollment_failure_result(
        status, contracts_active, warning_epoch, epoch, failures);
  };
  EXPECT_EQ(epose_enrollment_failure_action::error,
      outcome(service_producer_status_v2::invalid_endpoint, false, 4));
  EXPECT_EQ(epose_enrollment_failure_action::error,
      outcome(service_producer_status_v2::signing_failed, true, 4));
  EXPECT_EQ(0u, failures.load());
  EXPECT_EQ(epose_enrollment_failure_action::retry_with_warning,
      outcome(service_producer_status_v2::invalid_endpoint, true, 4));
  EXPECT_EQ(epose_enrollment_failure_action::retry_quietly,
      outcome(service_producer_status_v2::invalid_endpoint, true, 4));
  EXPECT_EQ(2u, failures.load());
  EXPECT_EQ(epose_enrollment_failure_action::retry_with_warning,
      outcome(service_producer_status_v2::invalid_endpoint, true, 5));
  EXPECT_EQ(3u, failures.load());
}
