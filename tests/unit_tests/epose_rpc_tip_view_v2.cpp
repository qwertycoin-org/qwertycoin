// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// The read-only EPoSE RPC handlers against a real core and Blockchain. While
// another thread holds the chain lock, each must answer from the published
// tip view without waiting and without a reader lock violation. While the
// coordinator is inconsistent with the database they answer BUSY, and they
// answer from the replayed state after the next tip change.

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>

#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "epose/tip_view_v2.h"
#include "epose_fakechain_v2.h"
#include "p2p/net_node.h"
#include "rpc/core_rpc_server.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "string_tools.h"

namespace
{
  using protocol_t = cryptonote::t_cryptonote_protocol_handler<cryptonote::core>;
  using p2p_t = nodetool::node_server<protocol_t>;

  // How long a handler may take with the chain lock held elsewhere before
  // it counts as blocked.
  const std::chrono::seconds blocked_after(10);

  template <class T>
  std::string zero_hex()
  {
    return epee::string_tools::pod_to_hex(T{});
  }

  class rpc_tip_view : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      directory_ = epose_fakechain_v2::temp_directory("qwc-epose-rpc-tip-view-%%%%-%%%%");
      parameters_ = epose_fakechain_v2::parameters();
      // A pop of two blocks needs a replay.
      parameters_.limits.max_recent_undo_blocks = 1;
      ASSERT_TRUE(parameters_.valid());

      core_.reset(new cryptonote::core(nullptr));
      protocol_.reset(new protocol_t(*core_, nullptr, true));
      p2p_.reset(new p2p_t(*protocol_));
      rpc_.reset(new cryptonote::core_rpc_server(*core_, *p2p_));

      db_ = new epose_fakechain_v2::faulty_lmdb();
      relay_policy_ = epose_fakechain_v2::relay_policy();
      ASSERT_TRUE(epose_fakechain_v2::init(bc(), directory_, parameters_, db_, &relay_policy_));

      cryptonote::account_base miner;
      miner.generate();
      for (int i = 0; i < 3; ++i)
      {
        cryptonote::block_verification_context bvc{};
        ASSERT_TRUE(epose_fakechain_v2::add_block(
            bc(), epose_fakechain_v2::template_on_tip(bc(), miner), bvc));
        ASSERT_TRUE(bvc.m_added_to_main_chain);
      }
      const auto view = bc().get_epose_tip_view();
      ASSERT_NE(nullptr, view);
      ASSERT_TRUE(view->v2);
      ASSERT_EQ(bc().get_tail_id(), view->tip_hash);
    }

    void TearDown() override
    {
      rpc_.reset();
      p2p_.reset();
      protocol_.reset();
      if (core_)
        bc().deinit();
      core_.reset();
      boost::system::error_code ignored;
      boost::filesystem::remove_all(directory_, ignored);
    }

    cryptonote::Blockchain &bc() { return core_->get_blockchain_storage(); }

    // Runs `call` on another thread while this thread holds the chain lock.
    // Returns whether it finished before blocked_after; it always finishes
    // once the lock is released, so the thread is joined either way.
    template <class Call>
    bool finishes_while_chain_locked(Call call, std::chrono::seconds limit = blocked_after)
    {
      bc().lock();
      std::future<void> done = std::async(std::launch::async, call);
      const bool finished = done.wait_for(limit) == std::future_status::ready;
      bc().unlock();
      done.get();
      return finished;
    }

    boost::filesystem::path directory_;
    qwertycoin::epose::consensus_parameters_v2 parameters_{};
    qwertycoin::epose::relay_policy_v2 relay_policy_{};
    std::unique_ptr<cryptonote::core> core_;
    std::unique_ptr<protocol_t> protocol_;
    std::unique_ptr<p2p_t> p2p_;
    std::unique_ptr<cryptonote::core_rpc_server> rpc_;
    epose_fakechain_v2::faulty_lmdb *db_ = nullptr;
  };
}

TEST_F(rpc_tip_view, read_only_handlers_skip_chain_lock)
{
  std::vector<std::pair<std::string, std::string>> statuses;
  const uint64_t violations_before =
      qwertycoin::epose::epose_view_reader_lock_violations();

  const bool finished = finishes_while_chain_locked([&]
  {
    cryptonote::core_rpc_server &rpc = *rpc_;
    {
      cryptonote::COMMAND_RPC_GET_EPOSE_INFO::request req{};
      cryptonote::COMMAND_RPC_GET_EPOSE_INFO::response res{};
      rpc.on_get_epose_info(req, res);
      statuses.emplace_back("get_epose_info", res.status);
    }
    {
      cryptonote::COMMAND_RPC_GET_SERVICE_NODES::request req{};
      cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response res{};
      rpc.on_get_service_nodes(req, res);
      statuses.emplace_back("get_service_nodes", res.status);
    }
    {
      cryptonote::COMMAND_RPC_GET_SERVICE_NODE_STATUS::request req{};
      req.service_public_key = zero_hex<crypto::public_key>();
      cryptonote::COMMAND_RPC_GET_SERVICE_NODE_STATUS::response res{};
      rpc.on_get_service_node_status(req, res);
      statuses.emplace_back("get_service_node_status", res.status);
    }
    {
      cryptonote::COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_PAYLOAD::request req{};
      cryptonote::COMMAND_RPC_GET_SERVICE_NODE_REGISTRATION_PAYLOAD::response res{};
      rpc.on_get_service_node_registration_payload(req, res);
      statuses.emplace_back("get_service_node_registration_payload", res.status);
    }
    {
      cryptonote::COMMAND_RPC_GET_EPOSE_EPOCH::request req{};
      cryptonote::COMMAND_RPC_GET_EPOSE_EPOCH::response res{};
      rpc.on_get_epose_epoch(req, res);
      statuses.emplace_back("get_epose_epoch", res.status);
    }
    {
      cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::request req{};
      cryptonote::COMMAND_RPC_GET_SERVICE_REWARDS::response res{};
      rpc.on_get_service_rewards(req, res);
      statuses.emplace_back("get_service_rewards", res.status);
    }
    {
      // No local service is configured: the handler reads the view, finds
      // no local descriptor and answers BUSY.
      cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::request req{};
      cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response res{};
      rpc.on_get_epose_service_endpoint_v2(req, res);
      statuses.emplace_back("get_epose_service_endpoint_v2", res.status);
    }
    {
      // Well-formed, so the handler enters its reader scope; with no
      // service key loaded the core refuses it.
      cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::request req{};
      req.version = 1;
      req.service_kind = 1;
      req.snapshot_hash = zero_hex<crypto::hash>();
      req.anchor_hash = zero_hex<crypto::hash>();
      req.subject_public_key = zero_hex<crypto::public_key>();
      req.verifier_public_key = zero_hex<crypto::public_key>();
      req.endpoint_descriptor_hash = zero_hex<crypto::hash>();
      req.nonce = zero_hex<crypto::hash>();
      req.requested_object_hash = zero_hex<crypto::hash>();
      cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::response res{};
      rpc.on_epose_service_challenge_v2(req, res);
      statuses.emplace_back("epose_service_challenge_v2", res.status);
    }
  });

  EXPECT_TRUE(finished) << "a read-only EPoSE handler waited for the chain lock";
  EXPECT_EQ(violations_before,
      qwertycoin::epose::epose_view_reader_lock_violations());
  ASSERT_EQ(8u, statuses.size());
  for (size_t i = 0; i < 6; ++i)
    EXPECT_EQ(CORE_RPC_STATUS_OK, statuses[i].second) << statuses[i].first;
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, statuses[6].second) << statuses[6].first;
  EXPECT_EQ("REJECTED", statuses[7].second) << statuses[7].first;
}

#ifdef NDEBUG
// A handler that still takes the chain lock blocks and, inside a reader
// scope, is counted. Debug builds assert on the violation instead.
TEST_F(rpc_tip_view, locked_handler_blocks_and_is_counted)
{
  const uint64_t violations_before =
      qwertycoin::epose::epose_view_reader_lock_violations();
  const std::string tip = epee::string_tools::pod_to_hex(bc().get_tail_id());
  std::string status;
  const bool finished = finishes_while_chain_locked([&]
  {
    const qwertycoin::epose::epose_view_reader_scope reader;
    cryptonote::COMMAND_RPC_GET_EPOSE_BLOCK_REWARD::request req{};
    req.block_hash = tip;
    cryptonote::COMMAND_RPC_GET_EPOSE_BLOCK_REWARD::response res{};
    rpc_->on_get_epose_block_reward(req, res);
    status = res.status;
  }, std::chrono::seconds(2));

  EXPECT_FALSE(finished);
  // Each acquisition counts, nested ones included.
  EXPECT_LT(violations_before,
      qwertycoin::epose::epose_view_reader_lock_violations());
  EXPECT_FALSE(status.empty());
}
#endif

TEST_F(rpc_tip_view, busy_while_inconsistent_then_recovers)
{
  const auto info = [&]
  {
    cryptonote::COMMAND_RPC_GET_EPOSE_INFO::request req{};
    cryptonote::COMMAND_RPC_GET_EPOSE_INFO::response res{};
    rpc_->on_get_epose_info(req, res);
    return res;
  };
  const auto service_nodes = [&]
  {
    cryptonote::COMMAND_RPC_GET_SERVICE_NODES::request req{};
    cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response res{};
    rpc_->on_get_service_nodes(req, res);
    return res;
  };
  // An undecodable envelope from a peer: the P2P handler drops the peer
  // when this returns false.
  const auto peer_envelope_acceptable = [&]
  {
    std::vector<cryptonote::blobdata> accepted;
    return core_->handle_incoming_epose_envelopes_v2({"not-an-envelope"}, accepted);
  };
  EXPECT_FALSE(peer_envelope_acceptable());
  const auto info_before = info();
  ASSERT_EQ(CORE_RPC_STATUS_OK, info_before.status);
  ASSERT_EQ(CORE_RPC_STATUS_OK, service_nodes().status);

  const uint64_t height = bc().get_current_blockchain_height();
  std::vector<cryptonote::block> popped;
  for (uint64_t h = height - 2; h < height; ++h)
    popped.push_back(bc().get_db().get_block_from_height(h));
  db_->fail_commitment_reads = true;
  bc().pop_blocks(2);
  ASSERT_EQ(nullptr, bc().get_epose_tip_view());
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, info().status);
  EXPECT_EQ(CORE_RPC_STATUS_BUSY, service_nodes().status);
  // The state is unknown: envelopes are not judged and no peer is dropped.
  {
    std::vector<cryptonote::blobdata> accepted;
    EXPECT_EQ(qwertycoin::epose::relay_ingress_status_v2::context_unavailable,
        bc().submit_epose_relay_envelopes_v2({"not-an-envelope"}, accepted));
    EXPECT_TRUE(accepted.empty());
  }
  EXPECT_TRUE(peer_envelope_acceptable());

  db_->fail_commitment_reads = false;
  for (const auto &b : popped)
  {
    cryptonote::block_verification_context bvc{};
    ASSERT_TRUE(epose_fakechain_v2::add_block(bc(), b, bvc));
    ASSERT_TRUE(bvc.m_added_to_main_chain);
  }
  const auto info_after = info();
  ASSERT_EQ(CORE_RPC_STATUS_OK, info_after.status);
  EXPECT_EQ(info_before.current_epoch, info_after.current_epoch);
  EXPECT_EQ(info_before.service_node_count, info_after.service_node_count);
  EXPECT_EQ(info_before.qualified_count, info_after.qualified_count);
  EXPECT_EQ(info_before.attestation_count, info_after.attestation_count);
  EXPECT_EQ(info_before.state_hash, info_after.state_hash);
  EXPECT_EQ(CORE_RPC_STATUS_OK, service_nodes().status);
  EXPECT_FALSE(peer_envelope_acceptable());
}
