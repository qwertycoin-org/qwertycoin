// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <limits>

#include "epose/resource_policy_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  endpoint_descriptor_v2 descriptor(crypto::secret_key &secret)
  {
    endpoint_descriptor_v2 out{};
    crypto::generate_keys(out.service_public_key, secret);
    out.transport = endpoint_transport_v2::dns;
    out.host = "seed-00.qwertycoin.org";
    out.port = 8197;
    out.service_kind = 1;
    out.service_version = 1;
    out.sequence = 7;
    out.expiry_epoch = 20;
    EXPECT_TRUE(sign_endpoint_descriptor_v2(cryptonote::TESTNET, hash_text("genesis"),
        hash_text("parameters"), out, secret));
    return out;
  }
}

TEST(epose_resource_policy_v2, signed_canonical_descriptor_is_context_bound)
{
  crypto::secret_key secret{};
  const auto value = descriptor(secret);
  EXPECT_EQ(resource_status_v2::accepted, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  EXPECT_EQ(resource_status_v2::invalid_signature, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("other-genesis"), hash_text("parameters"), value));
}

TEST(epose_resource_policy_v2, dns_and_literal_hosts_must_be_canonical)
{
  crypto::secret_key secret{};
  auto value = descriptor(secret);
  value.host = "Seed-00.qwertycoin.org";
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.host = "bad-.example.org";
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.host = "localhost";
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.transport = endpoint_transport_v2::tcp_ipv4;
  value.host = "8.8.8.8";
  ASSERT_TRUE(sign_endpoint_descriptor_v2(cryptonote::TESTNET, hash_text("genesis"),
      hash_text("parameters"), value, secret));
  EXPECT_EQ(resource_status_v2::accepted, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
}

TEST(epose_resource_policy_v2, private_metadata_and_mapped_addresses_are_prohibited)
{
  EXPECT_FALSE(public_probe_address_v2("127.0.0.1"));
  EXPECT_FALSE(public_probe_address_v2("10.0.0.1"));
  EXPECT_FALSE(public_probe_address_v2("169.254.169.254"));
  EXPECT_FALSE(public_probe_address_v2("192.168.1.1"));
  EXPECT_FALSE(public_probe_address_v2("::1"));
  EXPECT_FALSE(public_probe_address_v2("fc00::1"));
  EXPECT_FALSE(public_probe_address_v2("::ffff:127.0.0.1"));
  EXPECT_TRUE(public_probe_address_v2("8.8.8.8"));
  EXPECT_TRUE(public_probe_address_v2("2606:4700:4700::1111"));
}

TEST(epose_resource_policy_v2, dns_resolution_is_bounded_and_rechecked)
{
  EXPECT_EQ(resource_status_v2::resolution_empty, validate_resolved_targets_v2({}, 2));
  EXPECT_EQ(resource_status_v2::resolution_limit_exceeded,
      validate_resolved_targets_v2({"8.8.8.8", "1.1.1.1", "9.9.9.9"}, 2));
  EXPECT_EQ(resource_status_v2::accepted,
      validate_resolved_targets_v2({"8.8.8.8", "2606:4700:4700::1111"}, 2));
  EXPECT_EQ(resource_status_v2::prohibited_address,
      validate_resolved_targets_v2({"8.8.8.8", "169.254.169.254"}, 2));
}

TEST(epose_resource_policy_v2, probe_budget_bounds_bytes_timeout_concurrency_and_peers)
{
  probe_limits_v2 limits{2, 2, 1, 32, 64, 1000};
  probe_budget_v2 budget(limits);
  const crypto::hash a = hash_text("peer-a");
  const crypto::hash b = hash_text("peer-b");
  EXPECT_EQ(resource_status_v2::request_too_large, budget.acquire(a, 33, 64, 1000));
  EXPECT_EQ(resource_status_v2::response_too_large, budget.acquire(a, 32, 65, 1000));
  EXPECT_EQ(resource_status_v2::timeout_too_large, budget.acquire(a, 32, 64, 1001));
  ASSERT_EQ(resource_status_v2::accepted, budget.acquire(a, 32, 64, 1000));
  EXPECT_EQ(resource_status_v2::peer_limit_exceeded, budget.acquire(a, 1, 1, 1));
  ASSERT_EQ(resource_status_v2::accepted, budget.acquire(b, 1, 1, 1));
  EXPECT_EQ(resource_status_v2::concurrency_exceeded, budget.acquire(hash_text("peer-c"), 1, 1, 1));
  EXPECT_EQ(2u, budget.active());
  budget.release(a);
  EXPECT_EQ(1u, budget.active());
}

TEST(epose_resource_policy_v2, unknown_admission_context_never_allocates_cache)
{
  admission_context_cache_v2 cache(1);
  const crypto::hash allowed = hash_text("allowed");
  EXPECT_EQ(resource_status_v2::unknown_admission_context,
      cache.admit(hash_text("attacker"), {allowed}));
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(resource_status_v2::accepted, cache.admit(allowed, {allowed}));
  EXPECT_EQ(resource_status_v2::accepted, cache.admit(allowed, {allowed}));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(resource_status_v2::context_cache_full,
      cache.admit(hash_text("second"), {allowed, hash_text("second")}));
}

TEST(epose_resource_policy_v2, rpc_pages_are_bounded_without_integer_wrap)
{
  uint64_t end = 0;
  EXPECT_EQ(resource_status_v2::accepted, validate_rpc_page_v2(10, 20, 100, 1000, 200, end));
  EXPECT_EQ(30u, end);
  EXPECT_EQ(resource_status_v2::invalid_page, validate_rpc_page_v2(10, 101, 100, 1000, 200, end));
  EXPECT_EQ(resource_status_v2::scan_limit_exceeded, validate_rpc_page_v2(190, 20, 100, 1000, 200, end));
  EXPECT_EQ(resource_status_v2::scan_limit_exceeded,
      validate_rpc_page_v2(std::numeric_limits<uint64_t>::max(), 1,
          1, std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max(), end));
}

TEST(epose_resource_policy_v2, invalid_transport_and_literal_family_fail_closed)
{
  crypto::secret_key secret{};
  auto value = descriptor(secret);
  value.transport = static_cast<endpoint_transport_v2>(99);
  EXPECT_EQ(resource_status_v2::invalid_descriptor, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.transport = endpoint_transport_v2::tcp_ipv6;
  value.host = "8.8.8.8";
  ASSERT_TRUE(sign_endpoint_descriptor_v2(cryptonote::TESTNET, hash_text("genesis"),
      hash_text("parameters"), value, secret));
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
}
