// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/service_node.h"

namespace
{
  cryptonote::account_public_address make_reward_address()
  {
    crypto::secret_key spend_secret;
    crypto::secret_key view_secret;
    cryptonote::account_public_address address{};
    crypto::generate_keys(address.m_spend_public_key, spend_secret);
    crypto::generate_keys(address.m_view_public_key, view_secret);
    return address;
  }

  uint64_t parse_u64(const char *value, const char *name)
  {
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (!value[0] || (end && *end))
      throw std::invalid_argument(std::string("invalid ") + name + ": " + value);
    return static_cast<uint64_t>(parsed);
  }
}

int main(int argc, char **argv)
{
  try
  {
    const uint64_t parsed_leading_zero_bits = argc > 1 ? parse_u64(argv[1], "leading-zero bits") : qwertycoin::epose::EPOSE_ADMISSION_LEADING_ZERO_BITS;
    if (parsed_leading_zero_bits > std::numeric_limits<uint8_t>::max())
      throw std::invalid_argument("leading-zero bits exceed uint8 range");
    const uint8_t leading_zero_bits = static_cast<uint8_t>(parsed_leading_zero_bits);
    const uint64_t identities = argc > 2 ? parse_u64(argv[2], "identity count") : 10;
    const uint64_t epoch = argc > 3 ? parse_u64(argv[3], "epoch") : 1;
    const uint64_t max_hashes_per_identity = argc > 4 ? parse_u64(argv[4], "maximum hashes per identity") : 0;
    const uint64_t steady_hashes = argc > 5 ? parse_u64(argv[5], "steady-state hash count") : 128;
    const crypto::hash previous_epoch_hash = crypto::cn_fast_hash("qwc-epose-admission-benchmark-seed", 34);

    qwertycoin::epose::service_node_identity probe{};
    crypto::secret_key probe_secret;
    crypto::generate_keys(probe.service_public_key, probe_secret);
    probe.reward_address = make_reward_address();
    probe.endpoint_commitment = qwertycoin::epose::make_endpoint_commitment("bench-steady.example:8196");
    probe.registration_epoch = epoch;
    probe.expiry_epoch = epoch + qwertycoin::epose::EPOSE_REGISTRATION_TTL_EPOCHS;

    const auto setup_started = std::chrono::steady_clock::now();
    probe.admission_hash = qwertycoin::epose::calculate_admission_hash(probe, cryptonote::TESTNET, previous_epoch_hash);
    const auto setup_time = std::chrono::steady_clock::now() - setup_started;
    const auto steady_started = std::chrono::steady_clock::now();
    for (uint64_t nonce = 0; nonce < steady_hashes; ++nonce)
    {
      probe.admission_nonce = nonce + 1;
      probe.admission_hash = qwertycoin::epose::calculate_admission_hash(probe, cryptonote::TESTNET, previous_epoch_hash);
    }
    const auto steady_time = std::chrono::steady_clock::now() - steady_started;

    uint64_t total_hashes = 0;
    uint64_t verified = 0;
    uint64_t solved = 0;
    const auto started = std::chrono::steady_clock::now();
    auto verify_time = std::chrono::nanoseconds::zero();

    for (uint64_t i = 0; i < identities; ++i)
    {
      qwertycoin::epose::service_node_identity identity{};
      crypto::secret_key service_secret;
      crypto::generate_keys(identity.service_public_key, service_secret);
      identity.reward_address = make_reward_address();
      identity.endpoint_commitment = qwertycoin::epose::make_endpoint_commitment("bench-node-" + std::to_string(i) + ".example:8196");
      identity.registration_epoch = epoch;
      identity.expiry_epoch = epoch + qwertycoin::epose::EPOSE_REGISTRATION_TTL_EPOCHS;

      bool found = false;
      for (uint64_t nonce = 0;; ++nonce)
      {
        identity.admission_nonce = nonce;
        identity.admission_hash = qwertycoin::epose::calculate_admission_hash(identity, cryptonote::TESTNET, previous_epoch_hash);
        ++total_hashes;
        if (qwertycoin::epose::admission_hash_meets_target(identity.admission_hash, leading_zero_bits))
        {
          found = true;
          ++solved;
          break;
        }
        if (max_hashes_per_identity && nonce + 1 >= max_hashes_per_identity)
          break;
      }
      if (!found)
        continue;

      qwertycoin::epose::sign_registration(identity, service_secret, cryptonote::TESTNET);
      const auto verify_started = std::chrono::steady_clock::now();
      if (qwertycoin::epose::verify_admission_proof(identity, cryptonote::TESTNET, previous_epoch_hash, leading_zero_bits))
        ++verified;
      verify_time += std::chrono::steady_clock::now() - verify_started;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    const auto verify_ms = std::chrono::duration_cast<std::chrono::milliseconds>(verify_time);
    const auto setup_us = std::chrono::duration_cast<std::chrono::microseconds>(setup_time);
    const auto steady_us = std::chrono::duration_cast<std::chrono::microseconds>(steady_time);
    const double seconds = elapsed.count() / 1000.0;
    const double hashes_per_second = seconds > 0.0 ? total_hashes / seconds : 0.0;
    const double steady_seconds = steady_us.count() / 1000000.0;
    const double steady_hashes_per_second = steady_seconds > 0.0 ? steady_hashes / steady_seconds : 0.0;

    std::cout << "leading_zero_bits=" << static_cast<unsigned int>(leading_zero_bits) << '\n';
    std::cout << "identities=" << identities << '\n';
    std::cout << "max_hashes_per_identity=" << max_hashes_per_identity << '\n';
    std::cout << "setup_first_hash_us=" << setup_us.count() << '\n';
    std::cout << "steady_hashes=" << steady_hashes << '\n';
    std::cout << "steady_elapsed_us=" << steady_us.count() << '\n';
    std::cout << "steady_hashes_per_second=" << steady_hashes_per_second << '\n';
    std::cout << "total_hashes=" << total_hashes << '\n';
    std::cout << "solved=" << solved << '\n';
    std::cout << "verified=" << verified << '\n';
    std::cout << "elapsed_ms=" << elapsed.count() << '\n';
    std::cout << "verify_ms=" << verify_ms.count() << '\n';
    std::cout << "avg_create_ms=" << (identities ? static_cast<double>(elapsed.count()) / identities : 0.0) << '\n';
    std::cout << "avg_verify_ms=" << (identities ? static_cast<double>(verify_ms.count()) / identities : 0.0) << '\n';
    std::cout << "hashes_per_second=" << hashes_per_second << '\n';
    if (steady_hashes_per_second > 0.0 && leading_zero_bits < 64)
      std::cout << "expected_create_ms_from_steady_rate=" << (std::ldexp(1.0, leading_zero_bits) / steady_hashes_per_second * 1000.0) << '\n';
    return max_hashes_per_identity ? 0 : (verified == identities ? 0 : 1);
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
