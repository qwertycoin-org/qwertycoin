#pragma once

#include <gtest/gtest.h>
#include <iostream>
#include <string>

namespace unit_test
{
  inline void record_skip(const std::string &reason)
  {
    ::testing::Test::RecordProperty("skipped_reason", reason);
    std::cerr << "Skipping test: " << reason << std::endl;
  }
}
