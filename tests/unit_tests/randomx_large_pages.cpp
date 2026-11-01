// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <thread>

#include "crypto/hash.h"

namespace
{
  crypto::hash hash_on_new_thread(const crypto::hash &seed, uint8_t payload)
  {
    crypto::hash result{};
    std::thread worker([&] {
      const crypto::rx_thread_state_guard randomx_state;
      const std::array<uint8_t, 4> data{payload, 1, 2, 3};
      crypto::rx_slow_hash(seed.data, data.data(), data.size(), result.data);
    });
    worker.join();
    return result;
  }
}

TEST(randomx_large_pages, an_unavailable_large_page_allocation_is_not_retried_on_every_thread)
{
  const crypto::hash seed = crypto::cn_fast_hash("large-page-seed", 15);
  const crypto::hash first = hash_on_new_thread(seed, 1);

  const unsigned long attempts_after_first = crypto::rx_large_page_attempts();
  const crypto::hash second = hash_on_new_thread(seed, 1);
  const crypto::hash third = hash_on_new_thread(seed, 2);
  if (crypto::rx_large_pages_unavailable())
    EXPECT_EQ(attempts_after_first, crypto::rx_large_page_attempts());
  else
    EXPECT_GT(crypto::rx_large_page_attempts(), attempts_after_first);
  EXPECT_EQ(first, second);
  EXPECT_NE(first, third);
}

TEST(randomx_large_pages, a_released_thread_state_is_rebuilt_with_the_same_result)
{
  const crypto::hash seed = crypto::cn_fast_hash("large-page-seed", 15);
  EXPECT_EQ(hash_on_new_thread(seed, 7), hash_on_new_thread(seed, 7));
}
