#include <gtest/gtest.h>

#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "p2p/net_node.h"
#include "rpc/core_rpc_server.h"

namespace cryptonote
{
  void configure_stale_epose_rpc_core(core &instance);
}

namespace
{
  using protocol = cryptonote::t_cryptonote_protocol_handler<cryptonote::core>;

  class blocked_p2p : public nodetool::node_server<protocol>
  {
  public:
    explicit blocked_p2p(protocol &handler) : nodetool::node_server<protocol>(handler) {}
    bool is_host_blocked(const epee::net_utils::network_address &, time_t *) override
    {
      return true;
    }
  };

  struct rpc_fixture
  {
    cryptonote::core core{nullptr};
    protocol handler{core, nullptr, true};
    blocked_p2p p2p{handler};
    cryptonote::core_rpc_server rpc{core, p2p};
  };
}

TEST(epose_rpc_handler, banned_host_gets_403_and_challenge_passes)
{
  rpc_fixture fixture;
  cryptonote::core_rpc_server::connection_context context{boost::uuids::uuid{},
      epee::net_utils::ipv4_network_address{MAKE_IP(192, 0, 2, 1), 0}, true, false};
  epee::net_utils::http::http_request_info request{};
  request.m_http_method_str = "POST";
  request.m_body = "{}";
  epee::net_utils::http::http_response_info response{};

  request.m_URI = "/get_height";
  ASSERT_TRUE(fixture.rpc.handle_http_request(request, response, context));
  EXPECT_EQ(403, response.m_response_code);
  EXPECT_EQ("close", response.m_header_info.m_connection);

  request.m_URI = cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI;
  response = {};
  ASSERT_TRUE(fixture.rpc.handle_http_request(request, response, context));
  EXPECT_EQ(200, response.m_response_code);
}

TEST(epose_rpc_handler, challenge_reservation_is_settled_through_handler)
{
  rpc_fixture fixture;
  cryptonote::core_rpc_server::connection_context ipv4{boost::uuids::uuid{},
      epee::net_utils::ipv4_network_address{MAKE_IP(192, 0, 2, 1), 0}, true, false};
  cryptonote::core_rpc_server::connection_context ipv6{boost::uuids::uuid{},
      epee::net_utils::ipv6_network_address{
          boost::asio::ip::make_address_v6("2001:db8:1::1"), 0}, true, false};
  epee::net_utils::http::http_request_info request{};
  request.m_URI = cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI;
  request.m_http_method_str = "POST";
  request.m_body = "{}";
  for (auto *context : {&ipv4, &ipv6})
  {
    for (size_t attempt = 0; attempt < QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY; ++attempt)
    {
      epee::net_utils::http::http_response_info response{};
      ASSERT_TRUE(fixture.rpc.handle_http_request(request, response, *context));
      EXPECT_NE(429, response.m_response_code) << attempt;
    }
  }
}

TEST(epose_rpc_handler, challenge_response_over_byte_budget_is_429)
{
  rpc_fixture fixture;
  cryptonote::core_rpc_server::connection_context context{boost::uuids::uuid{},
      epee::net_utils::ipv4_network_address{MAKE_IP(192, 0, 2, 1), 0}, true, false};
  epee::net_utils::http::http_request_info request{};
  epee::net_utils::http::http_response_info response{};
  const std::string body(QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY + 1, 'x');
  response.m_response_code = 200;
  response.m_body = body;
  request.m_URI = cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI;
  ASSERT_TRUE(fixture.rpc.check_epose_http_request(request, response, context));
  fixture.rpc.check_epose_http_response(request, response, "192.0.2.1");
  EXPECT_EQ(429, response.m_response_code);
  EXPECT_TRUE(response.m_body.empty());
  ASSERT_TRUE(fixture.rpc.check_epose_http_request(request, response, context));

  response.m_response_code = 200;
  response.m_body = body;
  request.m_URI = "/get_height";
  fixture.rpc.check_epose_http_response(request, response, "192.0.2.1");
  EXPECT_EQ(200, response.m_response_code);
  EXPECT_EQ(body.size(), response.m_body.size());

  response.m_response_code = 403;
  request.m_URI = cryptonote::EPOSE_SERVICE_CHALLENGE_V2_URI;
  fixture.rpc.check_epose_http_response(request, response, "192.0.2.1");
  EXPECT_EQ(403, response.m_response_code);
  EXPECT_EQ(body.size(), response.m_body.size());
  EXPECT_TRUE(fixture.rpc.check_epose_http_request(request, response, context));
}

TEST(epose_rpc_handler, get_epose_info_reports_busy_without_tip_state)
{
  rpc_fixture fixture;
  cryptonote::COMMAND_RPC_GET_EPOSE_INFO::request request{};
  cryptonote::COMMAND_RPC_GET_EPOSE_INFO::response response{};
  cryptonote::configure_stale_epose_rpc_core(fixture.core);
  ASSERT_FALSE(fixture.core.get_blockchain_storage().epose_rpc_ready());
  ASSERT_TRUE(fixture.rpc.on_get_epose_info(request, response));
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, response.status);
}

TEST(epose_rpc_handler, get_service_rewards_reports_busy_without_tip_state)
{
  rpc_fixture fixture;
  cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::request request{};
  cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::response response{};
  cryptonote::configure_stale_epose_rpc_core(fixture.core);
  ASSERT_TRUE(fixture.rpc.on_get_service_rewards(request, response));
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, response.status);
}
