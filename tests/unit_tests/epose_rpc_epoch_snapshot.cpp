#include <gtest/gtest.h>

#include <string>

#include "rpc/epose_rpc_status.h"
#include "storages/portable_storage_template_helper.h"

TEST(epose_rpc_epoch_snapshot, qualification_uses_snapshot)
{
  cryptonote::Blockchain::epose_rpc_snapshot snapshot{};
  snapshot.ready = true;
  snapshot.qualification_epoch = 12;
  snapshot.qualification_available = true;
  snapshot.qualified_nodes.resize(3);
  EXPECT_EQ(3u, cryptonote::epose_epoch_qualified_nodes(snapshot).size());

  cryptonote::COMMAND_RPC_GET_EPOSE_EPOCH::response response{};
  EXPECT_FALSE(cryptonote::epose_rpc_busy(snapshot, response));
  cryptonote::epose_set_qualification_context(
      response, snapshot.qualification_epoch, snapshot.qualification_available);
  response.qualified_count = cryptonote::epose_epoch_qualified_nodes(snapshot).size();
  response.status = CORE_RPC_STATUS_OK;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(response, json));
  EXPECT_NE(std::string::npos, json.find("qualified_count"));
  EXPECT_EQ(3u, response.qualified_count);

  snapshot.ready = false;
  cryptonote::COMMAND_RPC_GET_EPOSE_EPOCH::response busy{};
  ASSERT_TRUE(cryptonote::epose_rpc_busy(snapshot, busy));
  EXPECT_EQ(0u, busy.qualified_count);
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, busy.status);
  ASSERT_TRUE(epee::serialization::store_t_to_json(busy, json));
  EXPECT_NE(std::string::npos, json.find("BUSY"));
}
