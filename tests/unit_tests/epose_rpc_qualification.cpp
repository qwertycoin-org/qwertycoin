#include <gtest/gtest.h>

#include <string>

#include "rpc/epose_rpc_status.h"
#include "storages/portable_storage_template_helper.h"

namespace
{
  template<typename Response>
  void check_qualification_context()
  {
    Response response{};
    cryptonote::epose_set_qualification_context(response, 0, false);
    EXPECT_EQ(0u, response.qualification_epoch);
    EXPECT_FALSE(response.qualification_available);

    cryptonote::epose_set_qualification_context(response, 3, true);
    EXPECT_EQ(3u, response.qualification_epoch);
    EXPECT_TRUE(response.qualification_available);

    std::string json;
    ASSERT_TRUE(epee::serialization::store_t_to_json(response, json));
    EXPECT_NE(std::string::npos, json.find("qualification_epoch"));
    EXPECT_NE(std::string::npos, json.find("qualification_available"));
  }

  template<typename Response>
  void check_busy_response()
  {
    cryptonote::Blockchain::epose_rpc_snapshot snapshot{};
    Response response{};
    ASSERT_TRUE(cryptonote::epose_rpc_busy(snapshot, response));
    EXPECT_EQ(CORE_RPC_STATUS_BUSY, response.status);
    EXPECT_EQ(0u, response.qualification_epoch);
    EXPECT_FALSE(response.qualification_available);
    std::string json;
    ASSERT_TRUE(epee::serialization::store_t_to_json(response, json));
    EXPECT_NE(std::string::npos, json.find("BUSY"));
  }
}

TEST(epose_rpc_qualification, service_nodes)
{
  check_qualification_context<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response>();
  check_busy_response<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response>();
  cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response response{};
  cryptonote::Blockchain::epose_rpc_snapshot snapshot{};
  EXPECT_TRUE(cryptonote::epose_rpc_busy(snapshot, response));
  EXPECT_EQ(0u, response.total_count);
  EXPECT_EQ(0u, response.returned_count);
  EXPECT_TRUE(response.service_nodes.empty());
  EXPECT_NE(CORE_RPC_STATUS_OK, response.status);
}

TEST(epose_rpc_qualification, service_node_transport_and_rounds_follow_frozen_membership)
{
  qwertycoin::epose::identity_descriptor_v2 identity{};
  identity.service_public_key.data[0] = 1;
  identity.endpoint_transport = 3;
  identity.dial_capabilities = 3;
  identity.dns_address_families = 2;
  identity.effective_epoch = 2;
  identity.expiry_epoch = 5;
  cryptonote::Blockchain::epose_rpc_snapshot snapshot{};
  snapshot.epoch = 3;
  snapshot.frozen_membership_available = true;
  snapshot.frozen_membership.transport_aware = true;
  snapshot.round_count = 3;
  snapshot.committee_size = 9;
  snapshot.min_compatible_committee = 5;
  qwertycoin::epose::frozen_member_v2 subject{};
  subject.service_public_key = identity.service_public_key;
  subject.endpoint_transport = 3;
  subject.dns_address_families = 2;
  snapshot.frozen_membership.members.push_back(subject);
  for (uint8_t index = 2; index <= 7; ++index)
  {
    qwertycoin::epose::frozen_member_v2 verifier{};
    verifier.service_public_key.data[0] = index;
    verifier.dial_capabilities = index == 7 ? 1 : 2;
    snapshot.frozen_membership.members.push_back(verifier);
  }
  const std::vector<crypto::public_key> qualified{identity.service_public_key};
  auto entry = cryptonote::make_epose_service_node_entry(identity, snapshot, qualified);
  EXPECT_EQ(3u, entry.endpoint_transport);
  EXPECT_EQ(3u, entry.dial_capabilities);
  EXPECT_EQ(2u, entry.dns_address_families);
  EXPECT_EQ(3u, entry.rounds_judged);
  EXPECT_EQ(0u, entry.rounds_unjudged);
  EXPECT_TRUE(entry.active);
  EXPECT_TRUE(entry.qualified);
  snapshot.frozen_membership.members[5].dial_capabilities = 1;
  entry = cryptonote::make_epose_service_node_entry(identity, snapshot, qualified);
  EXPECT_EQ(0u, entry.rounds_judged);
  EXPECT_EQ(3u, entry.rounds_unjudged);
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(entry, json));
  EXPECT_NE(std::string::npos, json.find("dial_capabilities"));
  EXPECT_NE(std::string::npos, json.find("dns_address_families"));
  EXPECT_NE(std::string::npos, json.find("rounds_judged"));
  EXPECT_NE(std::string::npos, json.find("rounds_unjudged"));
}

TEST(epose_rpc_qualification, service_node_status)
{
  check_qualification_context<cryptonote::COMMAND_RPC_GET_SERVICE_NODE_STATUS::response>();
  check_busy_response<cryptonote::COMMAND_RPC_GET_SERVICE_NODE_STATUS::response>();
}

TEST(epose_rpc_qualification, diagnostics_serializes_transport_and_rounds)
{
  qwertycoin::epose::qualification_diagnostics_v2 qualification{};
  qualification.rounds_judged = 2;
  qualification.rounds_unjudged = 1;
  qualification.dial_capabilities = 3;
  qualification.dns_address_families = 2;
  cryptonote::COMMAND_RPC_GET_EPOSE_DIAGNOSTICS::response response{};
  cryptonote::set_epose_diagnostic_transport_v2(response, qualification);
  EXPECT_EQ(2u, response.rounds_judged);
  EXPECT_EQ(1u, response.rounds_unjudged);
  EXPECT_EQ(3u, response.dial_capabilities);
  EXPECT_EQ(2u, response.dns_address_families);
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(response, json));
  EXPECT_NE(std::string::npos, json.find("rounds_judged"));
  EXPECT_NE(std::string::npos, json.find("rounds_unjudged"));
  EXPECT_NE(std::string::npos, json.find("dial_capabilities"));
  EXPECT_NE(std::string::npos, json.find("dns_address_families"));
}

TEST(epose_rpc_qualification, epoch)
{
  check_qualification_context<cryptonote::COMMAND_RPC_GET_EPOSE_EPOCH::response>();
  check_busy_response<cryptonote::COMMAND_RPC_GET_EPOSE_EPOCH::response>();
}
