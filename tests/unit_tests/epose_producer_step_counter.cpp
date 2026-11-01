#include <gtest/gtest.h>

#include <atomic>
#include <string>

#include "cryptonote_core/epose_warning_throttle.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/portable_storage_template_helper.h"

TEST(epose_producer_step_counter, increments_and_is_exposed_in_status)
{
  std::atomic<uint64_t> failures{0};
  EXPECT_FALSE(cryptonote::epose_producer_step_failed(failures));
  EXPECT_FALSE(cryptonote::epose_producer_step_failed(failures));
  EXPECT_EQ(2u, failures.load());

  cryptonote::COMMAND_RPC_GET_EPOSE_INFO::response response{};
  response.local_service_node_step_failed_count = failures.load();
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(response, json));
  EXPECT_NE(std::string::npos, json.find("local_service_node_step_failed_count"));
  cryptonote::COMMAND_RPC_GET_EPOSE_INFO::response restored{};
  ASSERT_TRUE(epee::serialization::load_t_from_json(restored, json));
  EXPECT_EQ(2u, restored.local_service_node_step_failed_count);
}
