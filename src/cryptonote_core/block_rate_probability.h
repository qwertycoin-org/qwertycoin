#pragma once

#include <cmath>

namespace cryptonote
{
  inline double block_rate_probability(unsigned int blocks, unsigned int expected)
  {
    if (expected == 0)
      return blocks == 0 ? 1.0 : 0.0;
    double probability = 0.0;
    const unsigned int lower = blocks <= expected ? 0 : blocks;
    const unsigned int upper = blocks <= expected ? blocks : expected * 3;
    for (unsigned int count = lower; count <= upper; ++count)
      probability += std::exp(count * std::log(static_cast<double>(expected)) - expected - std::lgamma(count + 1.0));
    return probability;
  }
}
