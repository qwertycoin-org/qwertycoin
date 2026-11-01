#include <gtest/gtest.h>

#include "storages/portable_storage_template_helper.h"
#include "epose/canonical_service_v2.h"
#include "rpc/epose_challenge_rpc_filter.h"
#include "rpc/core_rpc_server.h"
#include "rpc/epose_rpc_status.h"

namespace
{
  using filter_decision = cryptonote::epose_challenge_rpc_filter::decision;
  using clock = std::chrono::steady_clock;
  constexpr size_t reservation_refill_steps = QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES /
      QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL
      + (QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES %
          QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL != 0);
  static_assert(reservation_refill_steps <=
      static_cast<size_t>(std::numeric_limits<int64_t>::max()) /
          QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS, "duration overflow");
  constexpr auto reservation_advance = std::chrono::milliseconds(
      reservation_refill_steps * QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS);
  static_assert(QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX <=
      static_cast<size_t>(std::numeric_limits<clock::duration::rep>::max() /
          std::chrono::duration_cast<clock::duration>(reservation_advance).count()),
      "clock overflow");

  std::string ipv4_host(size_t host)
  {
    return "10.0." + std::to_string(host / 256) + "."
        + std::to_string(host % 256);
  }

  std::string prefix_host(size_t prefix, size_t host)
  {
    return "2001:db8:" + std::to_string(prefix) + ":"
        + std::to_string(host) + "::1";
  }

  template<typename HostName>
  bool pin_hosts(cryptonote::epose_challenge_rpc_filter &filter,
      size_t count, clock::time_point &now, HostName host_name)
  {
    for (size_t host = 0; host < count; ++host)
    {
      if (filter.check(host_name(host), cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI,
              false, now) != filter_decision::allowed)
        return false;
      now += reservation_advance;
    }
    return true;
  }

  filter_decision settled_check(cryptonote::epose_challenge_rpc_filter &filter,
      const std::string &host, clock::time_point now)
  {
    const auto result = filter.check(host, cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI,
        false, now);
    if (result == filter_decision::allowed)
      filter.settle_response_bytes(host, 0);
    return result;
  }

  struct filtered_test_server
  {
    using connection_context = cryptonote::core_rpc_server::connection_context;
    cryptonote::epose_challenge_rpc_filter filter;
    clock::time_point now{};
    size_t handler_calls = 0;
    size_t postcheck_calls = 0;

    bool precheck(const epee::net_utils::http::http_request_info &request,
        epee::net_utils::http::http_response_info &response,
        connection_context &context)
    {
      if (filter.check(context.m_remote_address.host_str(), request.m_URI, false, now)
          == filter_decision::allowed)
        return true;
      response.m_response_code = 429;
      return false;
    }

    void postcheck(const epee::net_utils::http::http_request_info &request,
        const epee::net_utils::http::http_response_info &response,
        connection_context &context)
    {
      ++postcheck_calls;
      if (request.m_URI == cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI)
        filter.settle_response_bytes(context.m_remote_address.host_str(),
            response.m_response_code == 200 ? response.m_body.size() : 0);
    }

    bool on_challenge(cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::request &,
        cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::response &,
        connection_context *)
    {
      ++handler_calls;
      return true;
    }

    CHAIN_HTTP_TO_MAP2_FILTERED_POST(connection_context,
        precheck(query_info, response, m_conn_context),
        postcheck(query_info, response, m_conn_context));
    BEGIN_URI_MAP2()
      MAP_URI_AUTO_JON2(cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI,
          on_challenge, cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2)
    END_URI_MAP2()
  };
}

TEST(epose_rpc_followups, challenge_route_exempts_p2p_ban_with_per_host_limit)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto now = std::chrono::steady_clock::time_point{};
  EXPECT_EQ(decision::banned, filter.check("192.0.2.1", "/get_height", false, now));
  EXPECT_EQ(decision::banned, filter.check("192.0.2.1", "/epose_service_challenge_v2/extra", false, now));
  for (size_t i = 0; i < QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY; ++i)
    EXPECT_EQ(decision::allowed, settled_check(filter, "192.0.2.1", now));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, "192.0.2.1", now));
  EXPECT_EQ(decision::allowed, filter.check("192.0.2.1", "/get_height", true, now));
  EXPECT_EQ(decision::allowed, settled_check(filter, "192.0.2.2", now));
  EXPECT_EQ(decision::allowed, settled_check(filter, "192.0.2.1", now + std::chrono::milliseconds(QWC_EPOSE_CHALLENGE_RPC_REFILL_MS)));
}

TEST(epose_rpc_followups, info_and_reward_mapping_return_busy_without_state)
{
  cryptonote::Blockchain::epose_rpc_snapshot snapshot{};
  cryptonote::COMMAND_RPC_GET_EPOSE_INFO::response info{};
  ASSERT_TRUE(cryptonote::epose_rpc_busy(snapshot, info));
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, info.status);
  EXPECT_EQ(0u, info.service_node_count);

  cryptonote::COMMAND_RPC_GET_EPOSE_BLOCK_REWARD::response mapping{};
  ASSERT_TRUE(cryptonote::epose_rpc_busy(false, mapping));
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, mapping.status);
  EXPECT_FALSE(mapping.mapping_available);
}

TEST(epose_rpc_followups, service_rewards_busy_when_height_moves_or_state_is_stale)
{
  cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::response response{};
  EXPECT_TRUE(cryptonote::epose_service_rewards_busy(0, 721, 722, true, response));
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, response.status);
  response.status.clear();
  EXPECT_TRUE(cryptonote::epose_service_rewards_busy(0, 721, 721, false, response));
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, response.status);
  response.status.clear();
  EXPECT_FALSE(cryptonote::epose_service_rewards_busy(721, 721, 722, true, response));
  EXPECT_TRUE(response.status.empty());
}

TEST(epose_rpc_followups, next_height_preview_uses_v2_plan)
{
  qwertycoin::epose::coordinator_reward_plan_v2 plan{};
  cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::response response{};
  crypto::secret_key payee_secret{};
  crypto::generate_keys(plan.expectation.payee_service_public_key,
      payee_secret);
  crypto::secret_key spend_secret{};
  crypto::secret_key view_secret{};
  crypto::generate_keys(plan.expectation.reward_address.m_view_public_key,
      view_secret);
  crypto::generate_keys(plan.expectation.reward_address.m_spend_public_key,
      spend_secret);
  plan.has_service_payee = true;
  cryptonote::epose_set_reward_preview(plan, response);
  EXPECT_TRUE(response.preview_available);
  EXPECT_TRUE(response.service_reward_active);
  EXPECT_EQ(epee::string_tools::pod_to_hex(plan.expectation.payee_service_public_key),
      response.expected_payee_service_public_key);
  EXPECT_EQ(epee::string_tools::pod_to_hex(plan.expectation.reward_address.m_view_public_key),
      response.expected_reward_view_public_key);
  EXPECT_EQ(epee::string_tools::pod_to_hex(plan.expectation.reward_address.m_spend_public_key),
      response.expected_reward_spend_public_key);

  plan.has_service_payee = false;
  cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::response inactive{};
  cryptonote::epose_set_reward_preview(plan, inactive);
  EXPECT_TRUE(inactive.preview_available);
  EXPECT_FALSE(inactive.service_reward_active);
  EXPECT_TRUE(inactive.expected_payee_service_public_key.empty());
}

TEST(epose_rpc_followups, ipv6_prefix_shares_bucket)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto now = std::chrono::steady_clock::time_point{};
  for (size_t i = 0; i < QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY; ++i)
    EXPECT_EQ(decision::allowed, settled_check(filter, "2001:db8:1:2::1", now));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, "2001:db8:1:2::ffff", now));
  EXPECT_EQ(decision::allowed, settled_check(filter, "2001:db8:1:3::1", now));
}

TEST(epose_rpc_followups, global_challenge_limit)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto now = std::chrono::steady_clock::time_point{};
  for (size_t i = 0; i < QWC_EPOSE_CHALLENGE_RPC_GLOBAL_CAPACITY; ++i)
    EXPECT_EQ(decision::allowed, settled_check(filter, ipv4_host(i), now));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, "198.51.100.1", now));
  EXPECT_EQ(decision::allowed, settled_check(filter, "198.51.100.1", now + std::chrono::milliseconds(QWC_EPOSE_CHALLENGE_RPC_REFILL_MS)));
}

TEST(epose_rpc_followups, sustained_host_does_not_starve_another_host)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto start = std::chrono::steady_clock::time_point{};
  for (size_t i = 0; i < QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY; ++i)
    ASSERT_EQ(decision::allowed, settled_check(filter, "192.0.2.1", start));
  const size_t duration = QWC_EPOSE_CHALLENGE_RPC_GLOBAL_CAPACITY + 64;
  for (size_t second = 1; second <= duration; ++second)
  {
    const auto now = start + std::chrono::milliseconds(
        second * QWC_EPOSE_CHALLENGE_RPC_REFILL_MS);
    ASSERT_EQ(decision::allowed, settled_check(filter, "192.0.2.1", now)) << second;
    ASSERT_EQ(decision::allowed, settled_check(filter, "192.0.2.2", now)) << second;
  }
}

TEST(epose_rpc_followups, ban_log_rearms_after_unban)
{
  cryptonote::epose_challenge_rpc_filter filter;
  EXPECT_TRUE(filter.first_ban_for_host("192.0.2.1"));
  EXPECT_FALSE(filter.first_ban_for_host("192.0.2.1"));
  filter.clear_ban_for_host("192.0.2.1");
  EXPECT_TRUE(filter.first_ban_for_host("192.0.2.1"));
  EXPECT_FALSE(filter.first_ban_for_host("192.0.2.1"));
}

TEST(epose_rpc_followups, expired_ban_is_logged_again)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto now = std::chrono::steady_clock::time_point{};
  EXPECT_TRUE(filter.first_ban_for_host("192.0.2.1"));
  EXPECT_TRUE(filter.first_ban_for_host("192.0.2.2"));
  EXPECT_EQ(decision::allowed, filter.check("192.0.2.1", "/get_height", true, now));
  EXPECT_TRUE(filter.first_ban_for_host("192.0.2.1"));
  EXPECT_FALSE(filter.first_ban_for_host("192.0.2.2"));
}

TEST(epose_rpc_followups, many_hosts_at_rate_are_not_starved)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  auto now = std::chrono::steady_clock::time_point{};
  const size_t rounds = QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY + 1;
  for (size_t round = 0; round < rounds; ++round)
  {
    for (size_t host = 0; host < QWC_EPOSE_CHALLENGE_RPC_GLOBAL_HOSTS; ++host)
      ASSERT_EQ(decision::allowed, settled_check(filter, "198.51.100." + std::to_string(host), now));
    now += std::chrono::milliseconds(QWC_EPOSE_CHALLENGE_RPC_REFILL_MS);
  }
}

TEST(epose_rpc_followups, ipv6_prefix_churn_is_limited)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto now = std::chrono::steady_clock::time_point{};
  size_t allowed = 0;
  for (size_t prefix = 0; prefix < QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX + 1; ++prefix)
  {
    const auto host = "2001:db8:1:" + std::to_string(prefix) + "::1";
    if (filter.check(host, cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI,
            false, now) == decision::allowed)
      ++allowed;
  }
  EXPECT_LE(allowed, QWC_EPOSE_CHALLENGE_RPC_IPV6_48_CAPACITY);
  EXPECT_EQ(decision::allowed, filter.check("2001:db8:2::1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, response_byte_budget_refills)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  const std::string host = "192.0.2.1";
  constexpr size_t reservation = QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  constexpr size_t missing = reservation -
      (QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY - reservation);
  constexpr size_t steps = missing / QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL
      + (missing % QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL != 0);
  ASSERT_EQ(filter_decision::allowed, filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  filter.settle_response_bytes(host, reservation);
  EXPECT_EQ(filter_decision::rate_limited, filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds((steps - 1) * QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS)));
  EXPECT_EQ(filter_decision::allowed, filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds(steps * QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS)));
}

TEST(epose_rpc_followups, global_byte_budget_refills)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  constexpr size_t reservation = QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  constexpr size_t allowed_hosts = QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY / reservation;
  constexpr size_t remainder = QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY % reservation;
  constexpr size_t missing = reservation - remainder;
  constexpr size_t steps = missing / QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL
      + (missing % QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL != 0);
  for (size_t host = 0; host < allowed_hosts; ++host)
    ASSERT_EQ(filter_decision::allowed, filter.check("198.51.100." + std::to_string(host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  const std::string next = "198.51.100." + std::to_string(allowed_hosts);
  EXPECT_EQ(filter_decision::rate_limited, filter.check(next,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, filter.check(next,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds((steps - 1) * QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS)));
  EXPECT_EQ(filter_decision::allowed, filter.check(next,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds(steps * QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS)));
}

TEST(epose_rpc_followups, endpoint_response_serializes_default_capabilities)
{
  for (const uint64_t version : {1u, 2u})
  {
    SCOPED_TRACE(version);
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response response{};
    response.version = version;
    response.dial_capabilities = 1;
    response.dns_address_families = 0;
    std::string body;
    ASSERT_TRUE(epee::serialization::store_t_to_json(response, body));
    epee::serialization::portable_storage json;
    ASSERT_TRUE(json.load_from_json(body));
    uint64_t value = 0;
    ASSERT_TRUE(json.get_value("dial_capabilities", value, nullptr));
    EXPECT_EQ(1u, value);
    ASSERT_TRUE(json.get_value("dns_address_families", value, nullptr));
    EXPECT_EQ(0u, value);
  }
}

TEST(epose_rpc_followups, endpoint_response_loads_missing_and_explicit_capabilities)
{
  for (const bool explicit_fields : {false, true})
  {
    SCOPED_TRACE(explicit_fields);
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response response{};
    response.dial_capabilities = 7;
    response.dns_address_families = 7;
    const std::string body = explicit_fields
        ? R"({"dial_capabilities":3,"dns_address_families":2})"
        : "{}";
    ASSERT_TRUE(epee::serialization::load_t_from_json(response, body));
    EXPECT_EQ(explicit_fields ? 3u : 1u, response.dial_capabilities);
    EXPECT_EQ(explicit_fields ? 2u : 0u, response.dns_address_families);
  }
}

TEST(epose_rpc_followups, maximum_block_response_fits_reservation)
{
  cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::response response{};
  response.status = CORE_RPC_STATUS_OK;
  response.block_blob.assign(
      QWC_EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES * 2, 'a');
  response.subject_signature = epee::string_tools::pod_to_hex(crypto::signature{});
  std::string body;
  ASSERT_TRUE(epee::serialization::store_t_to_json(response, body));
  EXPECT_LE(body.size(), QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES);
}

TEST(epose_rpc_followups, host_byte_budget_does_not_starve_another_prefix)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  ASSERT_EQ(filter_decision::allowed, filter.check("2001:db8:1::1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, filter.check("2001:db8:1::1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::allowed, filter.check("2001:db8:2::1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, prefix_byte_budget_does_not_starve_another_prefix)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  constexpr size_t count = QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY /
      QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  for (size_t host = 1; host <= count; ++host)
    ASSERT_EQ(filter_decision::allowed, filter.check(prefix_host(1, host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, filter.check(prefix_host(1, count + 1),
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::allowed, filter.check("2001:db8:2::1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, refused_byte_reservation_preserves_every_bucket)
{
  const auto now = clock::time_point{};
  constexpr size_t reservation = QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  constexpr size_t prefix_count = QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY / reservation;
  cryptonote::epose_challenge_rpc_filter prefix_filter;
  for (size_t host = 1; host <= prefix_count; ++host)
    ASSERT_EQ(filter_decision::allowed, prefix_filter.check(prefix_host(1, host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  const std::string refused = prefix_host(1, prefix_count + 1);
  ASSERT_EQ(filter_decision::rate_limited, prefix_filter.check(refused,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  for (size_t host = 1; host <= prefix_count; ++host)
    prefix_filter.settle_response_bytes(prefix_host(1, host), 0);
  for (size_t request = prefix_count; request < QWC_EPOSE_CHALLENGE_RPC_IPV6_48_CAPACITY; ++request)
    ASSERT_EQ(filter_decision::allowed, settled_check(prefix_filter,
        prefix_host(1, prefix_count + 1 + (request - prefix_count) /
            QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY), now)) << request;
  EXPECT_EQ(filter_decision::rate_limited, settled_check(prefix_filter,
      prefix_host(1, prefix_count + 1 +
          (QWC_EPOSE_CHALLENGE_RPC_IPV6_48_CAPACITY - prefix_count) /
              QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY), now));
  for (size_t request = 0; request < QWC_EPOSE_CHALLENGE_RPC_GLOBAL_CAPACITY
      - QWC_EPOSE_CHALLENGE_RPC_IPV6_48_CAPACITY; ++request)
    ASSERT_EQ(filter_decision::allowed, settled_check(prefix_filter, ipv4_host(request), now));
  EXPECT_EQ(filter_decision::rate_limited, settled_check(prefix_filter,
      "198.51.100.99", now));
  constexpr size_t remaining_global = QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY
      - prefix_count * reservation;
  cryptonote::epose_challenge_rpc_filter byte_filter;
  for (size_t host = 1; host <= prefix_count; ++host)
    ASSERT_EQ(filter_decision::allowed, byte_filter.check(prefix_host(1, host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  ASSERT_EQ(filter_decision::rate_limited, byte_filter.check(refused,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  for (size_t host = 0; host < remaining_global / reservation; ++host)
    ASSERT_EQ(filter_decision::allowed, byte_filter.check("203.0.113." + std::to_string(host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, byte_filter.check("203.0.113.99",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  byte_filter.settle_response_bytes(refused, 0);
  EXPECT_EQ(filter_decision::rate_limited, byte_filter.check("203.0.113.99",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  byte_filter.settle_response_bytes(prefix_host(1, 1), 0);
  EXPECT_EQ(filter_decision::allowed, byte_filter.check(refused,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));

  cryptonote::epose_challenge_rpc_filter global_filter;
  constexpr size_t global_count = QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY / reservation;
  for (size_t host = 0; host < global_count; ++host)
    ASSERT_EQ(filter_decision::allowed, global_filter.check("198.51.100." + std::to_string(host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  const std::string global_refused = prefix_host(2, 1);
  ASSERT_EQ(filter_decision::rate_limited, global_filter.check(global_refused,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  global_filter.settle_response_bytes("198.51.100.0", 0);
  ASSERT_EQ(filter_decision::allowed, global_filter.check(global_refused,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  for (size_t host = 1; host < prefix_count; ++host)
  {
    global_filter.settle_response_bytes("198.51.100." + std::to_string(host), 0);
    ASSERT_EQ(filter_decision::allowed, global_filter.check(prefix_host(2, host + 1),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  }
}

TEST(epose_rpc_followups, byte_refund_restores_exactly_unused_bytes)
{
  const auto now = clock::time_point{};
  constexpr size_t reservation = QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  constexpr size_t host_count = QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY / reservation;
  constexpr size_t prefix_count = QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY / reservation;
  constexpr size_t global_count = QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY / reservation;
  constexpr size_t marker = reservation % QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL - 1;
  static_assert(reservation % QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL > 0, "host boundary required");
  static_assert(marker + 1 == reservation % QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_REFILL,
      "prefix boundary differs");
  static_assert(marker + 1 == reservation % QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL,
      "global boundary differs");
  const std::string first = prefix_host(1, 1);

  cryptonote::epose_challenge_rpc_filter host_filter;
  ASSERT_EQ(filter_decision::allowed, host_filter.check(first,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  host_filter.settle_response_bytes(first,
      QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY % reservation);
  for (size_t request = 0; request < host_count; ++request)
    ASSERT_EQ(filter_decision::allowed, host_filter.check(first,
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, host_filter.check(first,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  host_filter.settle_response_bytes(first, reservation - marker);
  constexpr size_t host_steps = reservation / QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL;
  EXPECT_EQ(filter_decision::rate_limited, host_filter.check(first,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds(host_steps * QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS)));
  EXPECT_EQ(filter_decision::allowed, host_filter.check(first,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds((host_steps + 1) * QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS)));

  cryptonote::epose_challenge_rpc_filter prefix_filter;
  ASSERT_EQ(filter_decision::allowed, prefix_filter.check(first,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  prefix_filter.settle_response_bytes(first,
      QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY % reservation);
  for (size_t host = 1; host <= prefix_count; ++host)
    ASSERT_EQ(filter_decision::allowed, prefix_filter.check(prefix_host(1, host + 1),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, prefix_filter.check(prefix_host(1, prefix_count + 2),
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  prefix_filter.settle_response_bytes(prefix_host(1, 2), reservation - marker);
  constexpr size_t prefix_steps = reservation / QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_REFILL;
  EXPECT_EQ(filter_decision::rate_limited, prefix_filter.check(prefix_host(1, prefix_count + 2),
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds(prefix_steps * QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS)));
  EXPECT_EQ(filter_decision::allowed, prefix_filter.check(prefix_host(1, prefix_count + 2),
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds((prefix_steps + 1) * QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS)));

  cryptonote::epose_challenge_rpc_filter global_filter;
  ASSERT_EQ(filter_decision::allowed, global_filter.check(first,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  global_filter.settle_response_bytes(first,
      QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY % reservation);
  for (size_t host = 0; host < global_count; ++host)
    ASSERT_EQ(filter_decision::allowed, global_filter.check("203.0.113." + std::to_string(host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, global_filter.check("203.0.113.99",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  global_filter.settle_response_bytes("203.0.113.0", reservation - marker);
  constexpr size_t global_steps = reservation / QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL;
  EXPECT_EQ(filter_decision::rate_limited, global_filter.check("203.0.113.99",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds(global_steps * QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS)));
  EXPECT_EQ(filter_decision::allowed, global_filter.check("203.0.113.99",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false,
      now + std::chrono::milliseconds((global_steps + 1) * QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS)));
}

TEST(epose_rpc_followups, double_settle_credits_nothing)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  const std::string host = "192.0.2.1";
  ASSERT_EQ(filter_decision::allowed, filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  filter.settle_response_bytes(host, QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES);
  filter.settle_response_bytes(host, 0);
  EXPECT_EQ(filter_decision::rate_limited, filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, in_flight_is_tracked_per_level)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  constexpr size_t reservation = QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  const std::string a = prefix_host(1, 1);
  const std::string b = prefix_host(1, 2);
  ASSERT_EQ(filter_decision::allowed, filter.check(a,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  ASSERT_EQ(filter_decision::allowed, filter.check(b,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  filter.settle_response_bytes(a, reservation);
  filter.settle_response_bytes(a, 0);
  constexpr size_t prefix_count = (QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY
      - 2 * reservation) / reservation;
  for (size_t host = 0; host < prefix_count; ++host)
    ASSERT_EQ(filter_decision::allowed, filter.check(prefix_host(1, host + 3),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, filter.check(prefix_host(1, prefix_count + 3),
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  constexpr size_t global_count = (QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY
      - 2 * reservation) / reservation - prefix_count;
  for (size_t host = 0; host < global_count; ++host)
    ASSERT_EQ(filter_decision::allowed, filter.check("203.0.113." + std::to_string(host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, filter.check("203.0.113.99",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, settle_for_unknown_host_credits_nothing)
{
  cryptonote::epose_challenge_rpc_filter filter;
  const auto now = clock::time_point{};
  constexpr size_t count = QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY /
      QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES;
  for (size_t host = 0; host < count; ++host)
    ASSERT_EQ(filter_decision::allowed, filter.check("192.0.2." + std::to_string(host),
        cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  filter.settle_response_bytes("203.0.113.1", 0);
  EXPECT_EQ(filter_decision::rate_limited, filter.check("203.0.113.2",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, full_pinned_map_fails_closed)
{
  cryptonote::epose_challenge_rpc_filter filter;
  auto now = clock::time_point{};
  static_assert(QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX % 2 == 0, "entry limit must be even");
  constexpr size_t pinned_hosts = QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX / 2;
  static_assert(pinned_hosts - 1 <= 9999,
      "IPv6 prefixes exceed one group");
  ASSERT_TRUE(pin_hosts(filter, pinned_hosts, now,
      [](size_t host) { return prefix_host(host, 1); }));
  EXPECT_EQ(filter_decision::rate_limited, filter.check("10.1.0.1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  filter.settle_response_bytes(prefix_host(0, 1), 0);
  ASSERT_EQ(filter_decision::allowed, filter.check("10.1.0.1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  now += reservation_advance;
  ASSERT_EQ(filter_decision::allowed, filter.check("10.1.0.2",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  now += reservation_advance;
  EXPECT_EQ(filter_decision::rate_limited, filter.check("10.1.0.3",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, two_entry_insert_is_all_or_nothing)
{
  cryptonote::epose_challenge_rpc_filter filter;
  auto now = clock::time_point{};
  ASSERT_TRUE(pin_hosts(filter, QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX - 1, now, ipv4_host));
  ASSERT_EQ(filter_decision::allowed, filter.check("10.1.0.2",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  filter.settle_response_bytes("10.1.0.2", QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES);
  EXPECT_EQ(filter_decision::rate_limited, filter.check("2001:db8:99::1",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::rate_limited, filter.check("10.1.0.2",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
  EXPECT_EQ(filter_decision::allowed, filter.check("10.1.0.3",
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, now));
}

TEST(epose_rpc_followups, prefix_refills_four_requests_per_second)
{
  cryptonote::epose_challenge_rpc_filter filter;
  using decision = cryptonote::epose_challenge_rpc_filter::decision;
  const auto start = std::chrono::steady_clock::time_point{};
  for (size_t host = 1; host <= 4; ++host)
    for (size_t request = 0; request < QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY; ++request)
      ASSERT_EQ(decision::allowed, settled_check(filter, prefix_host(1, host), start));
  const auto next = start + std::chrono::milliseconds(QWC_EPOSE_CHALLENGE_RPC_REFILL_MS);
  for (size_t host = 1; host <= 4; ++host)
    ASSERT_EQ(decision::allowed, settled_check(filter, prefix_host(1, host), next));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, prefix_host(1, 5), next));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, prefix_host(1, 1), next));
  const auto quarter_second = next + std::chrono::milliseconds(
      QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REFILL_MS);
  EXPECT_EQ(decision::allowed, settled_check(filter, prefix_host(1, 5), quarter_second));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, prefix_host(1, 6), quarter_second));
  EXPECT_EQ(decision::rate_limited, settled_check(filter, prefix_host(1, 1), quarter_second));
}

TEST(epose_rpc_followups, refused_reservation_skips_response_builder)
{
  filtered_test_server server;
  filtered_test_server::connection_context context{boost::uuids::uuid{},
      epee::net_utils::ipv4_network_address{MAKE_IP(192, 0, 2, 1), 0}, true, false};
  const std::string host = context.m_remote_address.host_str();
  ASSERT_EQ(filter_decision::allowed, server.filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, server.now));
  epee::net_utils::http::http_request_info request{};
  request.m_URI = cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI;
  request.m_http_method_str = "POST";
  request.m_body = "{}";
  epee::net_utils::http::http_response_info response{};
  ASSERT_TRUE(server.handle_http_request(request, response, context));
  EXPECT_EQ(429, response.m_response_code);
  EXPECT_TRUE(response.m_body.empty());
  EXPECT_EQ(0u, server.handler_calls);
  EXPECT_EQ(0u, server.postcheck_calls);
  server.filter.settle_response_bytes(host, 0);
  ASSERT_EQ(filter_decision::allowed, server.filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, server.now));
  server.filter.settle_response_bytes(host, QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES);
  EXPECT_EQ(filter_decision::rate_limited, server.filter.check(host,
      cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI, false, server.now));
}

TEST(epose_rpc_followups, unassigned_verifier_is_rejected_before_block_fetch)
{
  using namespace qwertycoin::epose;
  crypto::public_key subject{};
  crypto::secret_key subject_secret{};
  crypto::generate_keys(subject, subject_secret);
  crypto::public_key assigned_verifier{};
  crypto::secret_key assigned_secret{};
  crypto::generate_keys(assigned_verifier, assigned_secret);
  crypto::public_key unassigned_verifier{};
  crypto::secret_key unassigned_secret{};
  crypto::generate_keys(unassigned_verifier, unassigned_secret);
  const crypto::hash hash = crypto::cn_fast_hash("challenge", 9);
  service_challenge_v2 challenge{};
  challenge.epoch = 8;
  challenge.round = 1;
  challenge.snapshot_hash = hash;
  challenge.anchor_hash = hash;
  challenge.subject_public_key = subject;
  challenge.verifier_public_key = unassigned_verifier;
  challenge.endpoint_descriptor_hash = hash;
  challenge.nonce = hash;
  challenge.requested_object_hash = hash;
  const receipt_context_v2 context{cryptonote::TESTNET, hash, hash};
  ASSERT_TRUE(validate_service_challenge_v2(challenge, context));
  bool block_fetched = false;
  canonical_service_response_v2 response{};
  EXPECT_EQ(canonical_service_status_v2::unauthorized_challenge,
      answer_canonical_block_challenge_v2(challenge, context, {1024},
          subject_secret,
          [&assigned_verifier](const service_challenge_v2 &candidate,
              const receipt_context_v2 &) {
            return candidate.verifier_public_key == assigned_verifier;
          },
          [&block_fetched](const crypto::hash &, cryptonote::blobdata &) {
            block_fetched = true;
            return false;
          }, response));
  EXPECT_FALSE(block_fetched);
  EXPECT_TRUE(response.block_blob.empty());
}
