#include <gtest/gtest.h>
#include "rpc/chainlock_rpc.h"
#include "rpc/epose_rpc_status.h"
#include "rpc/rpc_payment_costs.h"
#include "vm/gas.h"
#include "epose/chainlock_v1.h"
#include "storages/portable_storage_template_helper.h"

namespace
{
  crypto::public_key numbered_key(uint16_t number)
  {
    crypto::public_key key{};
    key.data[0] = static_cast<char>(number & 0xff);
    key.data[1] = static_cast<char>(number >> 8);
    return key;
  }

  qwertycoin::epose::chainlock_quorum_v1 derive_from_response(
      const cryptonote::COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::response &response,
      const qwertycoin::epose::chainlock_context_v1 &context)
  {
    std::vector<crypto::public_key> snapshot;
    std::vector<crypto::public_key> qualified;
    crypto::hash anchor{};
    EXPECT_TRUE(epee::string_tools::hex_to_pod(response.snapshot_block_id, anchor));
    for (const std::string &hex : response.snapshot_members)
    {
      crypto::public_key key{};
      EXPECT_TRUE(epee::string_tools::hex_to_pod(hex, key));
      snapshot.push_back(key);
    }
    for (const std::string &hex : response.qualified_members)
    {
      crypto::public_key key{};
      EXPECT_TRUE(epee::string_tools::hex_to_pod(hex, key));
      qualified.push_back(key);
    }
    const auto candidates = qwertycoin::epose::chainlock_candidates_v1(snapshot, qualified);
    return qwertycoin::epose::select_chainlock_quorum_v1(response.epoch, anchor, candidates, context);
  }
}

using namespace cryptonote;

TEST(chainlock_rpc, statuses)
{
  EXPECT_STREQ("No lock yet", chainlock_rpc_status(chainlock_proof_result::no_lock));
  EXPECT_STREQ("Height not locked", chainlock_rpc_status(chainlock_proof_result::height_not_locked));
  EXPECT_STREQ(VM_STATE_REBUILDING_STATUS, chainlock_rpc_status(chainlock_proof_result::rebuilding));
  EXPECT_STREQ(VM_STATE_REBUILDING_STATUS, vm_proof_rpc_status(vm_proof_result::rebuilding));
  EXPECT_STRNE(VM_PROOF_FAILED_STATUS, vm_proof_rpc_status(vm_proof_result::rebuilding));
  EXPECT_STRNE(CHAINLOCK_PROOF_FAILED_STATUS, chainlock_rpc_status(chainlock_proof_result::rebuilding));
  EXPECT_STREQ("Historical VM proof busy", vm_proof_rpc_status(vm_proof_result::busy));
  EXPECT_STREQ("Height outside VM undo retention", vm_proof_rpc_status(vm_proof_result::outside_retention));
}

TEST(chainlock_rpc, qualification_statuses)
{
  EXPECT_STREQ("OK", epose_qualification_rpc_status(false));
  EXPECT_STREQ("Qualification pruned", epose_qualification_rpc_status(true));
  EXPECT_STREQ("EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW",
      EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW);
}

TEST(chainlock_rpc, historical_vm_proof_admission)
{
  std::atomic<bool> in_flight{false};
  {
    historical_vm_proof_guard first(in_flight, true);
    ASSERT_TRUE(first.acquired());
    historical_vm_proof_guard second(in_flight, true);
    EXPECT_FALSE(second.acquired());
    EXPECT_STREQ(HISTORICAL_VM_PROOF_BUSY_STATUS, "Historical VM proof busy");
    historical_vm_proof_guard tip(in_flight, false);
    EXPECT_TRUE(tip.acquired());
  }
  EXPECT_FALSE(in_flight.load());
  const uint64_t chain_height = RESTRICTED_VM_PROOF_MAX_DEPTH + 100;
  EXPECT_FALSE(vm_proof_depth_allowed(chain_height - RESTRICTED_VM_PROOF_MAX_DEPTH - 2, chain_height, true));
  EXPECT_STREQ(RESTRICTED_VM_PROOF_DEPTH_STATUS, "Height exceeds restricted VM proof depth");
  EXPECT_TRUE(vm_proof_depth_allowed(chain_height - RESTRICTED_VM_PROOF_MAX_DEPTH - 1, chain_height, true));
  EXPECT_TRUE(vm_proof_depth_allowed(boost::none, chain_height, true));
  EXPECT_FALSE(vm_proof_depth_allowed(0, chain_height, true));
  EXPECT_TRUE(vm_proof_depth_allowed(chain_height - RESTRICTED_VM_PROOF_MAX_DEPTH - 2, chain_height, false));
  const uint64_t proof_chain_height = 3000;
  EXPECT_EQ(vm_proof_depth(boost::none, proof_chain_height), 0u);
  EXPECT_EQ(vm_proof_depth(0, proof_chain_height), proof_chain_height - 1);
  EXPECT_EQ(vm_proof_depth(proof_chain_height - 1, proof_chain_height), 0u);
  EXPECT_EQ(vm_proof_depth(proof_chain_height - 1 - QWC_VM_UNDO_RETENTION_BLOCKS,
      proof_chain_height), QWC_VM_UNDO_RETENTION_BLOCKS);
  EXPECT_EQ(vm_proof_cost(boost::none, proof_chain_height), COST_PER_BLOCK);
  EXPECT_EQ(vm_proof_cost(proof_chain_height - 1 - QWC_VM_UNDO_RETENTION_BLOCKS,
      proof_chain_height), QWC_VM_UNDO_RETENTION_BLOCKS * COST_PER_BLOCK);
  EXPECT_FALSE(vm_proof_outside_retention(proof_chain_height - 1 - QWC_VM_UNDO_RETENTION_BLOCKS,
      proof_chain_height));
  EXPECT_TRUE(vm_proof_outside_retention(proof_chain_height - 2 - QWC_VM_UNDO_RETENTION_BLOCKS,
      proof_chain_height));
}

TEST(chainlock_rpc, vm_height_requests)
{
  COMMAND_RPC_GET_VM_STATE_ROOT::request root;
  COMMAND_RPC_GET_VM_STATE_PROOF::request proof;
  ASSERT_TRUE(epee::serialization::load_t_from_json(root, "{}"));
  ASSERT_TRUE(epee::serialization::load_t_from_json(proof, "{\"key\":\"00\"}"));
  EXPECT_FALSE(root.height);
  EXPECT_FALSE(proof.height);
  EXPECT_EQ(epee::serialization::store_t_to_json(root).find("\"height\""), std::string::npos);
  EXPECT_EQ(epee::serialization::store_t_to_json(proof).find("\"height\""), std::string::npos);
  ASSERT_TRUE(epee::serialization::load_t_from_json(root, "{\"height\":0}"));
  ASSERT_TRUE(epee::serialization::load_t_from_json(proof, "{\"key\":\"00\",\"height\":0}"));
  ASSERT_TRUE(root.height);
  ASSERT_TRUE(proof.height);
  EXPECT_EQ(*root.height, 0u);
  EXPECT_EQ(*proof.height, 0u);
  EXPECT_NE(epee::serialization::store_t_to_json(root).find("\"height\""), std::string::npos);
  EXPECT_NE(epee::serialization::store_t_to_json(proof).find("\"height\""), std::string::npos);
}

TEST(chainlock_rpc, restricted_vm_dry_run_gas)
{
  EXPECT_TRUE(vm_dry_run_gas_allowed(QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP, true));
  EXPECT_FALSE(vm_dry_run_gas_allowed(QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP + 1, true));
  EXPECT_TRUE(vm_dry_run_gas_allowed(QWC_VM_TX_GAS_CAP, false));
  EXPECT_EQ(vm_dry_run_cost(0), 1u);
  EXPECT_EQ(vm_dry_run_cost(10000), 1u);
  EXPECT_EQ(vm_dry_run_cost(10001), 2u);
  EXPECT_EQ(vm_dry_run_cost(QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP), 100u);
}

TEST(chainlock_rpc, restricted_vm_dry_run_cap_fits_signed_calls)
{
  EXPECT_GE(QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP, 4 * vm::gas::check_signature);
  EXPECT_LT(QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP, QWC_VM_TX_GAS_CAP);
}

TEST(chainlock_rpc, wallet_retries_estimate_at_restricted_cap)
{
  EXPECT_TRUE(vm_dry_run_retry_at_restricted_cap(true, QWC_VM_TX_GAS_CAP, RESTRICTED_VM_DRY_RUN_GAS_STATUS));
  EXPECT_FALSE(vm_dry_run_retry_at_restricted_cap(false, QWC_VM_TX_GAS_CAP, RESTRICTED_VM_DRY_RUN_GAS_STATUS));
  EXPECT_FALSE(vm_dry_run_retry_at_restricted_cap(true, QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP, RESTRICTED_VM_DRY_RUN_GAS_STATUS));
  EXPECT_FALSE(vm_dry_run_retry_at_restricted_cap(true, QWC_VM_TX_GAS_CAP, CORE_RPC_STATUS_OK));
  EXPECT_FALSE(vm_dry_run_retry_at_restricted_cap(true, QWC_VM_TX_GAS_CAP, "Restricted"));
}

TEST(chainlock_rpc, vm_dry_run_wire_defaults)
{
  COMMAND_RPC_VM_CALL_DRY_RUN::request request;
  COMMAND_RPC_VM_CALL_DRY_RUN::response response;
  request.extra = "";
  const std::string request_json = epee::serialization::store_t_to_json(request);
  const std::string response_json = epee::serialization::store_t_to_json(response);
  EXPECT_EQ(request_json.find("\"client\""), std::string::npos);
  EXPECT_EQ(response_json.find("\"credits\""), std::string::npos);
  EXPECT_EQ(response_json.find("\"top_hash\""), std::string::npos);
}

TEST(chainlock_rpc, quorum_inputs_accepts_optional_payment_client)
{
  COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::request request;
  ASSERT_TRUE(epee::serialization::load_t_from_json(request,
      "{\"epoch\":2,\"limit\":128}"));
  EXPECT_EQ(request.epoch, 2u);
  EXPECT_TRUE(request.client.empty());
  ASSERT_TRUE(epee::serialization::load_t_from_json(request,
      "{\"epoch\":2,\"client\":\"paid-client\"}"));
  EXPECT_EQ(request.client, "paid-client");
}

TEST(chainlock_rpc, quorum_inputs_epoch_defaults_to_current)
{
  COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::request request;
  ASSERT_TRUE(epee::serialization::load_t_from_json(request, "{}"));
  EXPECT_EQ(request.epoch, CURRENT_EPOCH_REQUEST);
  EXPECT_EQ(request.offset, 0u);
  EXPECT_EQ(request.limit, 128u);
  EXPECT_EQ(resolve_requested_epoch(request.epoch, 57), 57u);
  ASSERT_TRUE(epee::serialization::load_t_from_json(request, "{\"epoch\":3}"));
  EXPECT_EQ(resolve_requested_epoch(request.epoch, 57), 3u);
  ASSERT_TRUE(epee::serialization::load_t_from_json(request, "{\"epoch\":0}"));
  EXPECT_EQ(resolve_requested_epoch(request.epoch, 57), 0u);
  EXPECT_STREQ(QUORUM_INPUTS_INVALID_EPOCH_STATUS, "Invalid epoch");
}

TEST(chainlock_rpc, pruned_quorum_inputs_require_unrestricted_rpc)
{
  EXPECT_TRUE(chainlock_reconstruction_allowed(false, true));
  EXPECT_FALSE(chainlock_reconstruction_allowed(true, true));
  EXPECT_FALSE(chainlock_reconstruction_allowed(false, false));
  EXPECT_FALSE(chainlock_quorum_inputs_allowed(true, true));
  EXPECT_TRUE(chainlock_quorum_inputs_allowed(true, false));
  EXPECT_TRUE(chainlock_quorum_inputs_allowed(false, true));
  EXPECT_STREQ(QUORUM_INPUTS_UNAVAILABLE_STATUS, "Quorum inputs unavailable");
}

TEST(chainlock_rpc, exposed_inputs_derive_consensus_quorum)
{
  for (const size_t count : {size_t{9}, size_t{10}, size_t{50}, size_t{80}})
  {
    cryptonote::chainlock_quorum_inputs inputs;
    inputs.epoch = 3;
    inputs.context.nettype = cryptonote::MAINNET;
    inputs.context.genesis_hash.data[0] = 1;
    inputs.snapshot_block_id.data[0] = 2;
    qwertycoin::epose::membership_snapshot_v2 snapshot;
    qwertycoin::epose::qualification_set_v2 qualified;
    for (size_t index = 0; index < count + 2; ++index)
    {
      const auto key = numbered_key(index);
      inputs.snapshot_members.push_back(key);
      qwertycoin::epose::frozen_member_v2 member{};
      member.service_public_key = key;
      snapshot.members.push_back(member);
      if (index < count)
      {
        inputs.qualified_members.push_back(key);
        qualified.qualified_nodes.push_back(key);
      }
    }
    inputs.qualified_members.push_back(numbered_key(1001));
    qualified.qualified_nodes.push_back(numbered_key(1001));
    inputs.snapshot_total = inputs.snapshot_members.size();
    inputs.qualification_total = inputs.qualified_members.size();
    cryptonote::COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::response response;
    cryptonote::fill_chainlock_quorum_inputs_response(inputs, 0, response);
    EXPECT_FALSE(response.test_override);
    const auto candidates = qwertycoin::epose::chainlock_candidates_v1(snapshot, qualified);
    ASSERT_EQ(candidates.size(), count);
    EXPECT_EQ(snapshot.members.size(), count + 2);
    EXPECT_EQ(qualified.qualified_nodes.size(), count + 1);
    const auto consensus = qwertycoin::epose::select_chainlock_quorum_v1(inputs.epoch, inputs.snapshot_block_id, candidates, inputs.context);
    const auto derived = derive_from_response(response, inputs.context);
    EXPECT_EQ(derived.members, consensus.members);
    EXPECT_EQ(derived.can_lock(), count >= QWC_CHAINLOCK_MIN_NODES);
    EXPECT_EQ(derived.members.size(), std::min(count, size_t{QWC_CHAINLOCK_QUORUM_MAX}));

    const std::string selected = epee::string_tools::pod_to_hex(consensus.members.front());
    const auto member = std::find(response.qualified_members.begin(), response.qualified_members.end(), selected);
    ASSERT_NE(member, response.qualified_members.end());
    *member = epee::string_tools::pod_to_hex(numbered_key(1000));
    EXPECT_NE(derive_from_response(response, inputs.context).members, consensus.members);
    cryptonote::fill_chainlock_quorum_inputs_response(inputs, 0, response);
    response.snapshot_block_id = epee::string_tools::pod_to_hex(inputs.context.genesis_hash);
    EXPECT_NE(derive_from_response(response, inputs.context).members, consensus.members);
  }
}

TEST(chainlock_rpc, quorum_input_pages_are_bounded)
{
  cryptonote::chainlock_quorum_inputs inputs;
  inputs.context.nettype = cryptonote::MAINNET;
  for (size_t index = 256; index < 512; ++index)
  {
    inputs.snapshot_members.push_back(numbered_key(index));
    inputs.qualified_members.push_back(numbered_key(index));
  }
  inputs.snapshot_total = 10000;
  inputs.qualification_total = 10000;
  cryptonote::COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::response response;
  cryptonote::fill_chainlock_quorum_inputs_response(inputs, 256, response);
  EXPECT_EQ(response.snapshot_total, 10000u);
  EXPECT_EQ(response.snapshot_members.size(), 256u);
  EXPECT_EQ(response.qualified_members.size(), 256u);
  EXPECT_EQ(response.snapshot_members.front(), epee::string_tools::pod_to_hex(numbered_key(256)));
  EXPECT_EQ(response.snapshot_members.back(), epee::string_tools::pod_to_hex(numbered_key(511)));
}
