// Copyright (c) 2014-2026, The Monero Project
// All rights reserved. See LICENSE for details.

#include "gtest/gtest.h"
#include "rpc/rpc_args.h"

TEST(RpcArgs, CanonicalBanExemptAddress)
{
  const auto ipv4 = cryptonote::canonical_rpc_ban_exempt_address("172.18.0.10");
  ASSERT_TRUE(ipv4);
  EXPECT_EQ("172.18.0.10", *ipv4);

  const auto ipv6 = cryptonote::canonical_rpc_ban_exempt_address("2001:0db8::1");
  ASSERT_TRUE(ipv6);
  EXPECT_EQ("2001:db8::1", *ipv6);
}

TEST(RpcArgs, BanExemptionRequiresExactIpLiteral)
{
  EXPECT_FALSE(cryptonote::canonical_rpc_ban_exempt_address("gateway.internal"));
  EXPECT_FALSE(cryptonote::canonical_rpc_ban_exempt_address("172.18.0.0/16"));
  EXPECT_FALSE(cryptonote::canonical_rpc_ban_exempt_address(""));
}

TEST(RpcArgs, BanExemptionMatchesOnlyConfiguredHost)
{
  const std::set<std::string> addresses {"172.18.0.10", "2001:db8::1"};
  EXPECT_TRUE(cryptonote::rpc_ban_exempt_address_matches(addresses, "172.18.0.10"));
  EXPECT_TRUE(cryptonote::rpc_ban_exempt_address_matches(addresses, "2001:db8::1"));
  EXPECT_FALSE(cryptonote::rpc_ban_exempt_address_matches(addresses, "172.18.0.11"));
  EXPECT_FALSE(cryptonote::rpc_ban_exempt_address_matches(addresses, "2001:db8::2"));
}
