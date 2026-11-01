#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/asio/post.hpp>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/hardfork.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/alternative_child_index.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "blockchain_db/lmdb/db_lmdb.h"
#include "cryptonote_protocol/cryptonote_protocol_handler_common.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_config.h"
#include "epose_coordinator_fixture.h"
#include "epose_journal_test_helpers.h"
#include "blockchain_test_db.h"
#include "../common/blockchain_and_pool.h"

namespace cryptonote
{
  class synchronized_protocol_stub : public i_cryptonote_protocol
  {
  public:
    bool is_synchronized() const override { return true; }
    bool relay_block(NOTIFY_NEW_FLUFFY_BLOCK::request &, cryptonote_connection_context &) override { return true; }
    bool relay_transactions(NOTIFY_NEW_TRANSACTIONS::request &, const boost::uuids::uuid &, epee::net_utils::zone, relay_method) override { return true; }
    bool relay_epose_payloads(NOTIFY_NEW_EPOSE_PAYLOADS::request &, const boost::uuids::uuid &, epee::net_utils::zone) override { return true; }
    bool relay_epose_envelopes_v2(NOTIFY_NEW_EPOSE_ENVELOPES_V2::request &, const boost::uuids::uuid &, epee::net_utils::zone) override { return true; }
    bool relay_epose_endpoints_v2(NOTIFY_NEW_EPOSE_ENDPOINTS_V2::request &, const boost::uuids::uuid &, epee::net_utils::zone) override { return true; }
    bool relay_chainlock(NOTIFY_CHAINLOCK_V1::request &, const boost::uuids::uuid &, epee::net_utils::zone) override { return true; }
  };

  class BlockchainRecoveryHarness
  {
  public:
    static bool select_chainlock(Blockchain &chain, uint64_t height, blobdata &blob)
    {
      return chain.select_template_chainlock(height, blob);
    }

    static bool validate_chainlock(Blockchain &chain, const block &candidate,
        uint64_t height, bool signatures, chainlock_record &carrier, bool &has_carrier)
    {
      return chain.validate_block_chainlock(candidate, height, signatures, carrier, has_carrier);
    }

    static bool verify_chainlock(Blockchain &chain,
        const qwertycoin::epose::chainlock_v1 &lock)
    {
      return chain.verify_complete_chainlock(lock, true);
    }

    static void clear_chainlock_cache(Blockchain &chain)
    {
      chain.m_chainlock_verification_cache.clear();
    }

    static bool has_chainlock_cache(const Blockchain &chain)
    {
      return !chain.m_chainlock_verification_cache.empty();
    }

    static size_t chainlock_cache_size(const Blockchain &chain)
    {
      return chain.m_chainlock_verification_cache.size();
    }

    static uint64_t chainlock_signature_verifications(const Blockchain &chain)
    {
      return chain.m_chainlock_signature_verifications;
    }

    static std::vector<crypto::public_key> &chainlock_test_candidates(Blockchain &chain)
    {
      return chain.m_chainlock_test_quorum->candidates;
    }

    static void configure(Blockchain &blockchain, uint64_t epoch_length = 720)
    {
      auto *db = new BlockchainTestDB();
      db->has_tip = true;
      blockchain.m_db = db;
      blockchain.m_nettype = FAKECHAIN;
      blockchain.m_epose_v2_parameters = epose_coordinator_fixture::parameters(
          qwertycoin::epose::empty_qualification_policy_v2::miner_fallback);
      blockchain.m_epose_v2_parameters.timing.epoch_length = epoch_length;
      blockchain.m_epose_v2.reset(new qwertycoin::epose::consensus_coordinator_v2(
          blockchain.m_epose_v2_parameters));
    }

    static void configure_preview(Blockchain &blockchain)
    {
      configure(blockchain);
      blockchain.m_hardfork = new HardFork(*blockchain.m_db, HF_VERSION_QWC_CONTRACTS, 0);
      blockchain.m_hardfork->init();
    }

    static bool template_plan(Blockchain &blockchain,
        const account_public_address &miner_address,
        qwertycoin::epose::coordinator_reward_plan_v2 &plan)
    {
      block candidate{};
      difficulty_type difficulty{};
      uint64_t height = 0;
      uint64_t reward = 0;
      uint64_t seed_height = 0;
      crypto::hash seed_hash{};
      bool observed = false;
      blockchain.m_epose_template_plan_observer = [&](const auto &actual) {
        plan = actual;
        observed = true;
      };
      const bool created = blockchain.create_block_template(candidate,
          miner_address, difficulty, height, reward, {}, seed_height, seed_hash);
      blockchain.m_epose_template_plan_observer = {};
      return created && observed;
    }

    static void set_template_tx_key(Blockchain &blockchain)
    {
      blockchain.m_template_tx_key_override = [](uint64_t height) {
        crypto::secret_key seed{};
        seed.data[0] = static_cast<unsigned char>(height & 0xff);
        seed.data[1] = static_cast<unsigned char>((height >> 8) & 0xff);
        seed.data[2] = 1;
        crypto::public_key public_key{};
        crypto::secret_key secret_key{};
        crypto::generate_keys(public_key, secret_key, seed, true);
        return secret_key;
      };
    }

    static void settle_qualified_payee(Blockchain &blockchain)
    {
      epose_coordinator_fixture::contexts source;
      std::vector<epose_coordinator_fixture::enrollment> members;
      epose_coordinator_fixture::settle_single_qualified_payee(
          blockchain.m_epose_v2_parameters, *blockchain.m_epose_v2, source, members);
    }

    static bool align(Blockchain &blockchain)
    {
      const std::string image = blockchain.m_epose_v2->serialize_state();
      auto &db = static_cast<BlockchainTestDB &>(*blockchain.m_db);
      if (!blockchain.m_epose_v2->restore_state(db.chain_height - 1, image))
        return false;
      db.commitment.block_hash = db.get_block_hash_from_height(0);
      db.commitment.state_hash = blockchain.m_epose_v2->state().state_hash();
      db.has_commitment = true;
      ++blockchain.m_epose_state_validation_generation;
      return true;
    }

    static void set_height(Blockchain &blockchain, uint64_t height)
    {
      static_cast<BlockchainTestDB &>(*blockchain.m_db).chain_height = height + 1;
    }

    static bool add_member(Blockchain &blockchain)
    {
      const auto member = epose_coordinator_fixture::make_enrollment(
          blockchain.m_epose_v2_parameters, "snapshot-member");
      const auto record = epose_coordinator_fixture::lifecycle_record(
          blockchain.m_epose_v2_parameters, member);
      epose_coordinator_fixture::contexts source;
      qwertycoin::epose::semantic_apply_summary_v2 summary{};
      return blockchain.m_epose_v2->preview_state().apply_transaction(
          {record}, {1, false, nullptr}, source, summary)
          == qwertycoin::epose::semantic_status_v2::accepted;
    }

    static bool begin_import_batch(Blockchain &blockchain)
    {
      CRITICAL_REGION_LOCAL(blockchain.m_blockchain_lock);
      if (!blockchain.m_db->batch_start())
        return false;
      blockchain.m_epose_rpc_batch_open = true;
      return true;
    }

    static void end_import_batch(Blockchain &blockchain)
    {
      CRITICAL_REGION_LOCAL(blockchain.m_blockchain_lock);
      blockchain.m_db->batch_stop();
      blockchain.m_epose_rpc_batch_open = false;
    }

    static qwertycoin::epose::coordinator_status_v2 connect_next(Blockchain &blockchain)
    {
      epose_coordinator_fixture::contexts source;
      qwertycoin::epose::coordinator_result_v2 result{};
      return epose_coordinator_fixture::connect_empty(
          *blockchain.m_epose_v2, source, 721, 1025, result);
    }

    static bool close_qualification(Blockchain &blockchain, uint64_t epoch)
    {
      auto &state = blockchain.m_epose_v2->preview_state();
      const auto &timing = blockchain.m_epose_v2_parameters.timing;
      uint64_t start = 0;
      uint64_t deadline = 0;
      if (!timing.epoch_start(epoch, start)
          || !timing.evidence_deadline(epoch, deadline)
          || start < timing.anchor_depth)
        return false;
      return epose_test_journal::freeze_membership(state, epoch,
                 start - timing.anchor_depth,
                 epose_coordinator_fixture::hash_text("qualification-anchor"))
                 == qwertycoin::epose::pipeline_status_v2::accepted
          && epose_test_journal::close_qualification(state, epoch, deadline)
                 == qwertycoin::epose::pipeline_status_v2::accepted;
    }

    static bool prune_qualification(Blockchain &blockchain, uint64_t epoch)
    {
      qwertycoin::epose::semantic_state_v2::undo_journal undo{};
      return blockchain.m_epose_v2->preview_state().prune_membership_history(
          epoch, &undo);
    }

    static bool advance_window(Blockchain &blockchain, uint64_t epoch)
    {
      qwertycoin::epose::semantic_state_v2::undo_journal undo{};
      return blockchain.m_epose_v2->preview_state().advance_window(epoch, &undo);
    }

    static bool retain_chainlock_epoch(Blockchain &blockchain, uint64_t epoch)
    {
      qwertycoin::epose::semantic_state_v2::undo_journal undo{};
      return blockchain.m_epose_v2->preview_state().retain_chainlock_epoch(epoch, &undo);
    }

    static size_t chainlock_writes(const Blockchain &blockchain)
    {
      return static_cast<const BlockchainTestDB &>(*blockchain.m_db).chainlock_writes;
    }

    static void inject_failures(Blockchain &blockchain, std::vector<bool> &attempts)
    {
      blockchain.m_epose_rebuild_injection = [&attempts](bool full) {
        attempts.push_back(full);
        return false;
      };
    }

    static void start_recovery(Blockchain &blockchain)
    {
      blockchain.schedule_epose_recovery();
    }

    static void run_recovery(Blockchain &blockchain)
    {
      blockchain.m_async_service.run();
    }

    static bool fatal(const Blockchain &blockchain)
    {
      return blockchain.m_epose_rebuild_fatal;
    }

    static bool recovery_scheduled(const Blockchain &blockchain)
    {
      return blockchain.m_epose_rebuild_retry_scheduled;
    }

    static uint64_t state_hash_checks(const Blockchain &blockchain)
    {
      return blockchain.m_epose_v2->state_hash_calls_;
    }

    static void state_changed(Blockchain &blockchain)
    {
      ++blockchain.m_epose_state_validation_generation;
    }

    static uint64_t preflight_runs(const Blockchain &blockchain)
    {
      return blockchain.m_epose_preflight_runs;
    }

    static bool switch_branch(Blockchain &blockchain, const crypto::hash &tip,
        bool with_batch)
    {
      std::list<Blockchain::block_extended_info> branch;
      std::vector<uint64_t> timestamps;
      block_verification_context verdict{};
      if (!blockchain.build_alt_chain(tip, branch, timestamps, verdict)
          || branch.empty())
        return false;
      return with_batch
          ? blockchain.switch_to_alternative_blockchain_with_batch(branch)
          : blockchain.switch_to_alternative_blockchain(branch, false);
    }

    static void observe_idle_service_failures(core &node, uint64_t &epoch,
        unsigned &attempts, unsigned &warnings)
    {
      node.m_epose_idle_service_update_hook = [&] {
        ++attempts;
        return false;
      };
      node.m_epose_idle_epoch_hook = [&] { return epoch; };
      node.m_epose_idle_service_warning_hook = [&] { ++warnings; };
    }

    static bool set_signer_vote(core &node, synchronized_protocol_stub &protocol,
        const std::string &path, const crypto::public_key &public_key,
        const crypto::secret_key &secret_key, uint64_t height,
        const crypto::hash &hash)
    {
      node.set_cryptonote_protocol(&protocol);
      node.m_offline = false;
      node.m_epose_v2_service_ready = true;
      node.m_epose_v2_keystore.service_public_key = public_key;
      node.m_epose_v2_keystore.service_secret_key = secret_key;
      node.m_epose_v2_keystore_path = path;
      node.m_chainlock_signer_state = {height, hash};
      std::string error;
      return qwertycoin::epose::save_chainlock_signer_state_v1(
          path + ".chainlock", node.m_chainlock_signer_state, error);
    }

    static bool load_signer_vote(core &node, synchronized_protocol_stub &protocol,
        const std::string &path, const crypto::public_key &public_key,
        const crypto::secret_key &secret_key)
    {
      node.set_cryptonote_protocol(&protocol);
      node.m_offline = false;
      node.m_epose_v2_service_ready = true;
      node.m_epose_v2_keystore.service_public_key = public_key;
      node.m_epose_v2_keystore.service_secret_key = secret_key;
      node.m_epose_v2_keystore_path = path;
      std::string error;
      return qwertycoin::epose::load_chainlock_signer_state_v1(
          path + ".chainlock", node.m_chainlock_signer_state, error);
    }

    static uint64_t attempt_sign(core &node, uint64_t height,
        const crypto::hash &hash)
    {
      node.m_chainlock_queued_tip = std::make_pair(height, hash);
      node.sign_queued_chainlock();
      return node.m_chainlock_signer_state.height;
    }

    static unsigned retries_for(const Blockchain &blockchain,
        const crypto::hash &tip)
    {
      const auto found = blockchain.m_epose_retry_chain_attempts.find(tip);
      return found == blockchain.m_epose_retry_chain_attempts.end()
          ? 0 : found->second;
    }

    static bool retry_with_exhausted_tip(Blockchain &blockchain,
        const crypto::hash &tip)
    {
      blockchain.m_epose_retry_chain_attempts[tip] = 3;
      blockchain.m_epose_retry_best_chain = true;
      return blockchain.retry_best_epose_chain()
          && blockchain.m_epose_retry_exhausted_tip == tip;
    }

    static bool retry(Blockchain &blockchain)
    {
      blockchain.m_epose_retry_best_chain = true;
      return blockchain.retry_best_epose_chain();
    }

    static bool verify_alternative_lock(Blockchain &blockchain,
        const qwertycoin::epose::chainlock_v1 &lock,
        const crypto::hash &tip)
    {
      std::list<Blockchain::block_extended_info> branch;
      std::vector<uint64_t> timestamps;
      block_verification_context verdict{};
      return blockchain.build_alt_chain(tip, branch, timestamps, verdict)
          && blockchain.verify_alt_chainlock(lock, branch);
    }

    static size_t ancestor_walks(const Blockchain &blockchain)
    {
      return blockchain.m_alternative_ancestor_walks.load();
    }

    static void fail_alternative_removal_after_one(Blockchain &blockchain)
    {
      blockchain.m_alternative_prune_removal_hook = [](size_t removed) {
        if (removed == 1)
          throw std::runtime_error("injected alternative removal failure");
      };
    }

    static void allow_alternative_removal(Blockchain &blockchain)
    {
      blockchain.m_alternative_prune_removal_hook = {};
    }

    static bool alternative_prune_pending(const Blockchain &blockchain)
    {
      return blockchain.m_alternative_prune_pending;
    }

    static void abort_batch_after_next_block(Blockchain &blockchain)
    {
      blockchain.add_block_notify(BlockNotifyCallback(
          [&blockchain](uint64_t, epee::span<const block>) {
            blockchain.m_batch_success = false;
          }));
    }

    static void fail_all_rebuilds(Blockchain &blockchain)
    {
      blockchain.m_epose_rebuild_injection = [](bool) { return false; };
    }

    static void fail_next_rebuild(Blockchain &blockchain)
    {
      blockchain.m_epose_rebuild_injection = [remaining = 1](bool) mutable {
        if (remaining == 0)
          return true;
        --remaining;
        return false;
      };
    }

    static void delay_recovery(Blockchain &blockchain)
    {
      boost::asio::post(blockchain.m_async_service, [] {
        std::this_thread::sleep_for(std::chrono::seconds(6));
      });
    }

    static void recover_without_outer_batch(Blockchain &blockchain,
        bool &observed_unbatched)
    {
      blockchain.m_epose_recovery_intake = [&blockchain, &observed_unbatched](
          const std::function<void()> &operation) {
        observed_unbatched = !blockchain.m_db->batch_active();
        operation();
      };
    }

  };

  void configure_stale_epose_rpc_core(core &instance)
  {
    BlockchainRecoveryHarness::configure(instance.get_blockchain_storage());
    BlockchainRecoveryHarness::set_height(instance.get_blockchain_storage(), 720);
  }
}

TEST(epose_blockchain_recovery, stale_state_blocks_consumers_and_reports_retryable_intake)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  ASSERT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_TRUE(diagnostics.rebuilding);
  EXPECT_FALSE(diagnostics.fatal);
  std::vector<crypto::public_key> qualified_nodes;
  EXPECT_EQ(cryptonote::Blockchain::epose_query_status::absent,
      chain.bc.get_epose_qualified_service_nodes(0, qualified_nodes));
  EXPECT_TRUE(qualified_nodes.empty());
  qwertycoin::epose::chainlock_context_v1 context{};
  EXPECT_FALSE(chain.bc.get_chainlock_context(context));
  cryptonote::blobdata relay;
  EXPECT_EQ(cryptonote::Blockchain::chainlock_ingress::ignored,
      chain.bc.handle_incoming_chainlock("invalid", relay));
  EXPECT_EQ(0u, cryptonote::BlockchainRecoveryHarness::chainlock_writes(chain.bc));
  cryptonote::block candidate{};
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(chain.bc.add_new_block(candidate, verdict));
  EXPECT_TRUE(verdict.m_retryable_busy);
  EXPECT_FALSE(verdict.m_verifivation_failed);
  EXPECT_FALSE(verdict.m_marked_as_orphaned);
  EXPECT_FALSE(chain.bc.get_epose_rpc_snapshot().ready);
  cryptonote::Blockchain::epose_block_reward_mapping_v2 mapping{};
  bool outside_retained_window = false;
  bool busy = false;
  EXPECT_FALSE(chain.bc.get_epose_block_reward_mapping_v2(
      crypto::null_hash, mapping, outside_retained_window, busy));
  EXPECT_TRUE(busy);
  EXPECT_FALSE(outside_retained_window);
}

TEST(epose_blockchain_recovery, valid_state_has_current_rpc_epoch)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  const auto snapshot = chain.bc.get_epose_rpc_snapshot();
  EXPECT_TRUE(snapshot.ready);
  EXPECT_EQ(chain.bc.get_epose_current_epoch(), snapshot.epoch);
  EXPECT_TRUE(chain.bc.epose_rpc_ready());
}

TEST(epose_blockchain_recovery, import_batch_does_not_expose_uncommitted_state)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::add_member(chain.bc));
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 720);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  const auto committed = chain.bc.get_epose_rpc_snapshot();
  ASSERT_TRUE(committed.ready);
  ASSERT_FALSE(committed.descriptors.empty());

  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::begin_import_batch(chain.bc));
  const auto connected = cryptonote::BlockchainRecoveryHarness::connect_next(chain.bc);
  cryptonote::Blockchain::epose_rpc_snapshot during{};
  std::thread reader([&] { during = chain.bc.get_epose_rpc_snapshot(); });
  reader.join();
  cryptonote::BlockchainRecoveryHarness::end_import_batch(chain.bc);
  EXPECT_EQ(qwertycoin::epose::coordinator_status_v2::accepted, connected);
  EXPECT_TRUE(!during.ready || !during.descriptors.empty());
  EXPECT_FALSE(during.ready);
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 721);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  EXPECT_TRUE(chain.bc.get_epose_rpc_snapshot().ready);
}

TEST(epose_blockchain_recovery, latest_closed_qualification_follows_evidence_deadline)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  uint64_t qualification_epoch = 9;
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 0);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  EXPECT_EQ(cryptonote::Blockchain::epose_query_status::absent,
      chain.bc.get_epose_latest_closed_qualification_epoch(qualification_epoch));
  EXPECT_EQ(0u, qualification_epoch);

  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::close_qualification(chain.bc, 1));
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 2098);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  ASSERT_EQ(cryptonote::Blockchain::epose_query_status::found,
      chain.bc.get_epose_latest_closed_qualification_epoch(qualification_epoch));
  EXPECT_EQ(1u, qualification_epoch);

  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::close_qualification(chain.bc, 2));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  ASSERT_EQ(cryptonote::Blockchain::epose_query_status::found,
      chain.bc.get_epose_latest_closed_qualification_epoch(qualification_epoch));
  EXPECT_EQ(1u, qualification_epoch);

  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 2099);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  ASSERT_EQ(cryptonote::Blockchain::epose_query_status::found,
      chain.bc.get_epose_latest_closed_qualification_epoch(qualification_epoch));
  EXPECT_EQ(2u, qualification_epoch);

  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 2100);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  ASSERT_EQ(cryptonote::Blockchain::epose_query_status::found,
      chain.bc.get_epose_latest_closed_qualification_epoch(qualification_epoch));
  EXPECT_EQ(2u, qualification_epoch);
}

TEST(epose_blockchain_recovery, latest_closed_qualification_unavailable_after_pruning)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::close_qualification(chain.bc, 1));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::prune_qualification(chain.bc, 5));
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 5 * 720);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  uint64_t qualification_epoch = 9;
  EXPECT_EQ(cryptonote::Blockchain::epose_query_status::absent,
      chain.bc.get_epose_latest_closed_qualification_epoch(qualification_epoch));
  EXPECT_EQ(0u, qualification_epoch);
  std::vector<crypto::public_key> qualified_nodes;
  EXPECT_EQ(cryptonote::Blockchain::epose_query_status::absent,
      chain.bc.get_epose_qualified_service_nodes(1, qualified_nodes));
  EXPECT_TRUE(qualified_nodes.empty());
}

TEST(epose_blockchain_recovery, epoch_queries_distinguish_folded_and_retained_history)
{
  using status = cryptonote::Blockchain::epose_query_status;
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::close_qualification(chain.bc, 9));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::advance_window(chain.bc, 10));
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 10 * 720);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));

  qwertycoin::epose::membership_snapshot_v2 snapshot{};
  qwertycoin::epose::qualification_set_v2 qualification{};
  std::vector<crypto::public_key> nodes;
  std::vector<qwertycoin::epose::identity_descriptor_v2> descriptors;
  uint64_t latest = 0;
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_membership_snapshot_v2(1, snapshot));
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_qualification_v2(1, qualification));
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_qualified_service_nodes(1, nodes));
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_identity_descriptors_v2(1, descriptors));
  EXPECT_EQ(status::absent,
      chain.bc.get_epose_membership_snapshot_v2(8, snapshot));
  EXPECT_EQ(status::found,
      chain.bc.get_epose_membership_snapshot_v2(9, snapshot));
  EXPECT_EQ(status::found,
      chain.bc.get_epose_qualification_v2(9, qualification));
  EXPECT_EQ(status::found,
      chain.bc.get_epose_qualified_service_nodes(9, nodes));
  EXPECT_EQ(qualification.qualified_nodes, nodes);
  EXPECT_EQ(status::absent,
      chain.bc.get_epose_identity_descriptors_v2(9, descriptors));
  EXPECT_EQ(status::found,
      chain.bc.get_epose_latest_closed_qualification_epoch(latest));
  EXPECT_EQ(9u, latest);

  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::advance_window(chain.bc, 20));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_latest_closed_qualification_epoch(latest));
}

TEST(epose_blockchain_recovery, epoch_query_status_preserves_folded_records_until_pruned)
{
  using status = cryptonote::Blockchain::epose_query_status;
  using harness = cryptonote::BlockchainRecoveryHarness;
  cryptonote_test::BlockchainAndPool chain;
  constexpr uint64_t epoch_length = 120;
  harness::configure(chain.bc, epoch_length);
  ASSERT_TRUE(harness::add_member(chain.bc));
  ASSERT_TRUE(harness::close_qualification(chain.bc, 1));
  ASSERT_TRUE(harness::advance_window(chain.bc, 10));
  harness::set_height(chain.bc, 2 * epoch_length);
  ASSERT_TRUE(harness::align(chain.bc));
  ASSERT_GT(chain.bc.get_epose_oldest_retained_epoch_v2(), 1u);

  qwertycoin::epose::membership_snapshot_v2 snapshot{};
  qwertycoin::epose::qualification_set_v2 qualification{};
  std::vector<crypto::public_key> nodes;
  std::vector<qwertycoin::epose::identity_descriptor_v2> descriptors;
  const auto expect_membership = [&](uint64_t epoch, status expected) {
    EXPECT_EQ(expected, chain.bc.get_epose_membership_snapshot_v2(epoch, snapshot));
    EXPECT_EQ(expected, chain.bc.get_epose_qualification_v2(epoch, qualification));
    EXPECT_EQ(expected, chain.bc.get_epose_qualified_service_nodes(epoch, nodes));
  };
  expect_membership(1, status::found);
  EXPECT_EQ(1u, snapshot.epoch);
  EXPECT_EQ(1u, qualification.epoch);
  EXPECT_EQ(qualification.qualified_nodes, nodes);
  EXPECT_EQ(status::found, chain.bc.get_epose_identity_descriptors_v2(1, descriptors));
  ASSERT_EQ(1u, descriptors.size());
  EXPECT_EQ(1u, descriptors.front().effective_epoch);
  uint64_t latest = 0;
  EXPECT_EQ(status::found, chain.bc.get_epose_latest_closed_qualification_epoch(latest));
  EXPECT_EQ(1u, latest);

  harness::set_height(chain.bc, 2 * epoch_length - 1);
  ASSERT_TRUE(harness::align(chain.bc));
  EXPECT_EQ(status::found, chain.bc.get_epose_latest_closed_qualification_epoch(latest));
  EXPECT_EQ(1u, latest);

  expect_membership(2, status::absent);
  expect_membership(0, status::outside_retained_window);
  EXPECT_EQ(status::absent, chain.bc.get_epose_identity_descriptors_v2(3, descriptors));
  EXPECT_TRUE(descriptors.empty());
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_identity_descriptors_v2(0, descriptors));
  EXPECT_TRUE(descriptors.empty());

  ASSERT_TRUE(harness::prune_qualification(chain.bc, 4));
  harness::set_height(chain.bc, 2 * epoch_length);
  ASSERT_TRUE(harness::align(chain.bc));
  ASSERT_TRUE(chain.bc.epose_qualification_pruned_v2(1));
  nodes.push_back(crypto::public_key{});
  expect_membership(1, status::absent);
  EXPECT_TRUE(nodes.empty());
  EXPECT_EQ(status::absent, chain.bc.get_epose_latest_closed_qualification_epoch(latest));
  EXPECT_EQ(0u, latest);

  harness::set_height(chain.bc, epoch_length);
  ASSERT_TRUE(harness::align(chain.bc));
  EXPECT_EQ(status::outside_retained_window,
      chain.bc.get_epose_latest_closed_qualification_epoch(latest));
  EXPECT_EQ(0u, latest);
}

TEST(epose_blockchain_recovery, pruned_folded_snapshot_is_absent_with_retained_qualification)
{
  using status = cryptonote::Blockchain::epose_query_status;
  using harness = cryptonote::BlockchainRecoveryHarness;
  cryptonote_test::BlockchainAndPool chain;
  harness::configure(chain.bc, 120);
  ASSERT_TRUE(harness::close_qualification(chain.bc, 1));
  ASSERT_TRUE(harness::close_qualification(chain.bc, 2));
  ASSERT_TRUE(harness::retain_chainlock_epoch(chain.bc, 2));
  ASSERT_TRUE(harness::advance_window(chain.bc, 10));
  ASSERT_TRUE(harness::prune_qualification(chain.bc, 4));
  harness::set_height(chain.bc, 2 * 120);
  ASSERT_TRUE(harness::align(chain.bc));

  qwertycoin::epose::membership_snapshot_v2 snapshot{};
  qwertycoin::epose::qualification_set_v2 qualification{};
  std::vector<crypto::public_key> nodes;
  EXPECT_FALSE(chain.bc.epose_qualification_pruned_v2(1));
  EXPECT_EQ(status::absent, chain.bc.get_epose_membership_snapshot_v2(1, snapshot));
  EXPECT_EQ(status::found, chain.bc.get_epose_qualification_v2(1, qualification));
  EXPECT_EQ(status::found, chain.bc.get_epose_qualified_service_nodes(1, nodes));
  EXPECT_EQ(qualification.qualified_nodes, nodes);
}

TEST(epose_blockchain_recovery, state_validity_follows_height_and_commitment)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  ASSERT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_TRUE(diagnostics.rebuilding);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  ASSERT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_FALSE(diagnostics.rebuilding);
  qwertycoin::epose::chainlock_context_v1 context{};
  EXPECT_TRUE(chain.bc.get_chainlock_context(context));
}

TEST(epose_blockchain_recovery, persistent_rebuild_failure_stops_after_full_rebuild)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  std::vector<bool> attempts;
  cryptonote::BlockchainRecoveryHarness::inject_failures(chain.bc, attempts);
  cryptonote::BlockchainRecoveryHarness::start_recovery(chain.bc);
  cryptonote::BlockchainRecoveryHarness::run_recovery(chain.bc);
  EXPECT_EQ((std::vector<bool>{false, false, false, true}), attempts);
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::fatal(chain.bc));
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  ASSERT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_TRUE(diagnostics.rebuilding);
  EXPECT_TRUE(diagnostics.fatal);
}

TEST(epose_blockchain_recovery, shutdown_cancels_recovery_timer)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  cryptonote::BlockchainRecoveryHarness::start_recovery(chain.bc);
  const auto start = std::chrono::steady_clock::now();
  ASSERT_TRUE(chain.bc.deinit());
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, std::chrono::milliseconds(500));
}

TEST(epose_blockchain_recovery, repeated_gated_getters_reuse_state_validation)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  ASSERT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  const uint64_t first_checks = cryptonote::BlockchainRecoveryHarness::state_hash_checks(chain.bc);
  ASSERT_EQ(1u, first_checks);
  for (size_t index = 0; index < 8; ++index)
  {
    qwertycoin::epose::chainlock_context_v1 context{};
    EXPECT_TRUE(chain.bc.get_chainlock_context(context));
    EXPECT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  }
  EXPECT_EQ(first_checks, cryptonote::BlockchainRecoveryHarness::state_hash_checks(chain.bc));
  cryptonote::BlockchainRecoveryHarness::state_changed(chain.bc);
  ASSERT_TRUE(chain.bc.get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_EQ(first_checks + 1,
      cryptonote::BlockchainRecoveryHarness::state_hash_checks(chain.bc));
}

TEST(epose_blockchain_recovery, busy_intake_schedules_recovery)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure(chain.bc);
  cryptonote::block candidate{};
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(chain.bc.add_new_block(candidate, verdict));
  EXPECT_TRUE(verdict.m_retryable_busy);
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::recovery_scheduled(chain.bc));
}

namespace
{
  bool open_archive_core(cryptonote::core &node, const std::string &directory)
  {
    boost::program_options::options_description options;
    cryptonote::core::init_options(options);
    boost::program_options::variables_map values;
    const std::vector<std::string> args{
        "--devnet", "--data-dir", directory, "--offline",
        "--disable-dns-checkpoints", "--check-updates=disabled"};
    boost::program_options::store(
        boost::program_options::command_line_parser(args).options(options).run(), values);
    boost::program_options::notify(values);
    return node.init(values, nullptr, nullptr, true);
  }

  class seeded_recovery_chain
  {
  public:
    explicit seeded_recovery_chain(bool contracts_at_one = false,
        bool deterministic = false)
      : directory_(boost::filesystem::temp_directory_path()
          / boost::filesystem::unique_path("epose-recovery-%%%%%%%%%%%%%%%%")),
        node_(nullptr), contracts_at_one_(contracts_at_one),
        deterministic_(deterministic)
    {
      crypto::secret_key seed{};
      seed.data[0] = 17;
      miner_.generate(seed, deterministic_);
    }

    ~seeded_recovery_chain()
    {
      if (initialized_)
        node_.deinit();
      boost::system::error_code ignored;
      boost::filesystem::remove_all(directory_, ignored);
    }

    bool init()
    {
      if (!boost::filesystem::exists(directory_)
          && !boost::filesystem::create_directory(directory_))
        return false;
      boost::program_options::options_description options;
      cryptonote::core::init_options(options);
      boost::program_options::variables_map values;
      std::vector<std::string> args{
          "--regtest", "--data-dir", directory_.string(),
          "--disable-dns-checkpoints", "--check-updates=disabled",
          "--offline", "--fixed-difficulty=1", "--keep-fakechain"};
      if (contracts_at_one_)
        args.push_back("--regtest-fork-height=1");
      boost::program_options::store(
          boost::program_options::command_line_parser(args).options(options).run(),
          values);
      boost::program_options::notify(values);
      cryptonote::block genesis{};
      if (!cryptonote::generate_genesis_block(genesis,
              cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_TX,
              cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_NONCE,
              HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_EPOSE))
        return false;
      parameters_ = epose_coordinator_fixture::parameters(
          qwertycoin::epose::empty_qualification_policy_v2::miner_fallback);
      parameters_.nettype = cryptonote::FAKECHAIN;
      parameters_.genesis_hash = cryptonote::get_block_hash(genesis);
      initialized_ = node_.init(values, nullptr, nullptr, true, &parameters_);
      if (initialized_ && deterministic_)
        cryptonote::BlockchainRecoveryHarness::set_template_tx_key(
            node_.get_blockchain_storage());
      return initialized_ && node_.get_blockchain_storage().get_epose_state_hash()
          != crypto::null_hash;
    }

    bool reopen()
    {
      if (!initialized_ || !node_.deinit())
        return false;
      initialized_ = false;
      return init();
    }

    bool template_block(cryptonote::block &candidate)
    {
      cryptonote::difficulty_type difficulty{};
      uint64_t height = 0;
      uint64_t reward = 0;
      uint64_t seed_height = 0;
      crypto::hash seed_hash{};
      const bool created = node_.get_block_template(candidate,
          miner_.get_keys().m_account_address, difficulty, height, reward,
          {}, seed_height, seed_hash);
      if (created && deterministic_)
      {
        candidate.timestamp = 1700000000 + height;
        candidate.invalidate_hashes();
      }
      return created;
    }

    bool submit(const cryptonote::block &candidate,
        cryptonote::block_verification_context &verdict)
    {
      auto &chain = node_.get_blockchain_storage();
      chain.prepare_handle_incoming_block_no_preprocess(
          cryptonote::block_to_blob(candidate).size());
      const bool result = chain.add_new_block(candidate, verdict);
      return chain.cleanup_handle_incoming_blocks() && result;
    }

    bool submit_single(const cryptonote::block &candidate,
        cryptonote::block_verification_context &verdict)
    {
      const cryptonote::blobdata blob = cryptonote::block_to_blob(candidate);
      cryptonote::pool_supplement extra{};
      return node_.handle_single_incoming_block(blob, &candidate, verdict, extra, false);
    }

    cryptonote::Blockchain &blockchain()
    {
      return node_.get_blockchain_storage();
    }

    cryptonote::core &node() { return node_; }
    std::string signer_path() const { return (directory_ / "signer").string(); }

    crypto::hash original_first() const { return original_first_; }
    crypto::hash original_second() const { return original_second_; }

    bool pop_db_tip()
    {
      auto &chain = node_.get_blockchain_storage();
      if (chain.get_current_blockchain_height() <= 1)
        return false;
      chain.prepare_handle_incoming_block_no_preprocess(0);
      cryptonote::block popped{};
      std::vector<cryptonote::transaction> transactions;
      chain.get_db().pop_block(popped, transactions);
      return chain.cleanup_handle_incoming_blocks();
    }

    bool branch_tip(const cryptonote::block &parent,
        cryptonote::block &candidate)
    {
      if (!template_block(candidate))
        return false;
      candidate.prev_id = cryptonote::get_block_hash(parent);
      ++candidate.nonce;
      return true;
    }

    bool seed_branch(cryptonote::block &alternative_first,
        cryptonote::block &alternative_second,
        const std::function<bool(cryptonote::block &)> &modify_first,
        const std::function<bool(cryptonote::block &)> &modify_second = {})
    {
      cryptonote::block main_first{};
      if (!template_block(main_first))
        return false;
      alternative_first = main_first;
      ++alternative_first.nonce;
      if (!modify_first(alternative_first))
        return false;
      cryptonote::block_verification_context verdict{};
      if (!submit(main_first, verdict) || !verdict.m_added_to_main_chain)
        return false;
      original_first_ = cryptonote::get_block_hash(main_first);
      cryptonote::block main_second{};
      if (!template_block(main_second))
        return false;
      alternative_second = main_second;
      alternative_second.prev_id = cryptonote::get_block_hash(alternative_first);
      ++alternative_second.nonce;
      if (modify_second && !modify_second(alternative_second))
        return false;
      verdict = {};
      if (!submit(main_second, verdict) || !verdict.m_added_to_main_chain)
        return false;
      original_second_ = cryptonote::get_block_hash(main_second);
      verdict = {};
      if (!submit(alternative_first, verdict))
        return false;
      verdict = {};
      return submit(alternative_second, verdict);
    }

  private:
    boost::filesystem::path directory_;
    cryptonote::core node_;
    cryptonote::account_base miner_;
    qwertycoin::epose::consensus_parameters_v2 parameters_{};
    bool initialized_ = false;
    bool contracts_at_one_ = false;
    bool deterministic_ = false;
    crypto::hash original_first_{};
    crypto::hash original_second_{};
  };

  bool add_additional_coinbase_keys(cryptonote::block &candidate)
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
    crypto::generate_keys(public_key, secret_key);
    std::vector<crypto::public_key> keys(candidate.miner_tx.vout.size(), public_key);
    if (!cryptonote::add_additional_tx_pub_keys_to_extra(
            candidate.miner_tx.extra, keys))
      return false;
    candidate.miner_tx.invalidate_hashes();
    return true;
  }

  bool add_coinbase_padding(cryptonote::block &candidate)
  {
    candidate.miner_tx.extra.push_back(TX_EXTRA_TAG_PADDING);
    candidate.miner_tx.invalidate_hashes();
    return true;
  }

  bool overmint_coinbase(cryptonote::block &candidate)
  {
    if (candidate.miner_tx.vout.empty()
        || candidate.miner_tx.vout.front().amount
            == std::numeric_limits<uint64_t>::max())
      return false;
    ++candidate.miner_tx.vout.front().amount;
    candidate.miner_tx.invalidate_hashes();
    return true;
  }

  bool sign_retry_chainlock(cryptonote::Blockchain &chain,
      const std::vector<std::pair<crypto::public_key, crypto::secret_key>> &signers,
      uint64_t height, const crypto::hash &hash,
      qwertycoin::epose::chainlock_v1 &lock)
  {
    uint64_t epoch = 0;
    qwertycoin::epose::chainlock_quorum_v1 quorum;
    qwertycoin::epose::chainlock_context_v1 context{};
    if (!chain.get_chainlock_epoch(height, epoch)
        || !chain.get_chainlock_quorum(epoch, quorum)
        || !chain.get_chainlock_context(context))
      return false;
    lock = {};
    lock.height = height;
    lock.block_hash = hash;
    lock.epoch = epoch;
    for (const auto &member : quorum.members)
    {
      const auto signer = std::find_if(signers.begin(), signers.end(),
          [&](const auto &candidate) { return candidate.first == member; });
      if (signer == signers.end())
        return false;
      qwertycoin::epose::chainlock_v1 share{};
      if (!qwertycoin::epose::sign_chainlock_share_v1(height, hash,
              quorum, signer->first, signer->second, context, share)
          || !qwertycoin::epose::merge_chainlock_share_v1(
              lock, share, quorum.threshold()))
        return false;
      if (lock.signer_count() >= quorum.threshold())
        break;
    }
    return qwertycoin::epose::chainlock_complete_v1(lock, quorum)
        && qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, true);
  }

  using retry_lock_tuple = std::tuple<uint64_t, crypto::hash, bool, cryptonote::blobdata>;

  std::vector<retry_lock_tuple> retry_lock_tuples(cryptonote::Blockchain &chain)
  {
    std::vector<retry_lock_tuple> tuples;
    chain.get_db().for_chainlocks_descending([&](const cryptonote::chainlock_record &lock) {
      tuples.emplace_back(lock.height, lock.block_hash, lock.tombstoned, lock.lock);
      return true;
    });
    return tuples;
  }

  void verified_retry_order_case(bool same_height, bool reverse_order)
  {
    SCOPED_TRACE(same_height ? "same height" : "different heights");
    SCOPED_TRACE(reverse_order ? "reverse order" : "forward order");
    seeded_recovery_chain fixture(true);
    ASSERT_TRUE(fixture.init());
    while (fixture.blockchain().get_current_blockchain_height() < 4)
    {
      cryptonote::block next{};
      ASSERT_TRUE(fixture.template_block(next));
      cryptonote::block_verification_context advanced{};
      ASSERT_TRUE(fixture.submit(next, advanced));
      ASSERT_TRUE(advanced.m_added_to_main_chain);
    }
    cryptonote::block first{};
    cryptonote::block second{};
    ASSERT_TRUE(fixture.seed_branch(first, second, add_coinbase_padding));
    auto &chain = fixture.blockchain();
    auto &db = chain.get_db();
    cryptonote::block rival_first = db.get_block_from_height(4);
    rival_first.nonce += 1000;
    ASSERT_TRUE(add_coinbase_padding(rival_first));
    rival_first.invalidate_hashes();
    cryptonote::block rival_second = db.get_block_from_height(5);
    rival_second.nonce += 1001;
    rival_second.prev_id = cryptonote::get_block_hash(rival_first);
    ASSERT_TRUE(add_coinbase_padding(rival_second));
    rival_second.invalidate_hashes();
    cryptonote::block_verification_context verdict{};
    ASSERT_TRUE(fixture.submit(rival_first, verdict));
    ASSERT_FALSE(verdict.m_added_to_main_chain);
    verdict = {};
    ASSERT_TRUE(fixture.submit(rival_second, verdict));
    ASSERT_FALSE(verdict.m_added_to_main_chain);
    cryptonote::block difficulty_only = db.get_block_from_height(4);
    difficulty_only.nonce += 2000;
    ASSERT_TRUE(add_coinbase_padding(difficulty_only));
    difficulty_only.invalidate_hashes();
    verdict = {};
    ASSERT_TRUE(fixture.submit(difficulty_only, verdict));
    ASSERT_FALSE(verdict.m_added_to_main_chain);
    const crypto::hash first_tip = cryptonote::get_block_hash(second);
    const crypto::hash second_tip = cryptonote::get_block_hash(rival_second);
    const crypto::hash winner = same_height
        ? std::min(first_tip, second_tip) : first_tip;

    qwertycoin::epose::epoch_timing_v2 timing{};
    timing.activation_height = 0;
    timing.epoch_length = 4;
    timing.anchor_depth = 1;
    std::vector<std::pair<crypto::public_key, crypto::secret_key>> signers(10);
    std::vector<crypto::public_key> candidates;
    for (auto &signer : signers)
    {
      crypto::generate_keys(signer.first, signer.second);
      candidates.push_back(signer.first);
    }
    ASSERT_TRUE(chain.set_chainlock_test_quorum(timing, candidates));
    qwertycoin::epose::chainlock_v1 first_lock{};
    qwertycoin::epose::chainlock_v1 second_lock{};
    ASSERT_TRUE(sign_retry_chainlock(chain, signers, 5, first_tip, first_lock));
    ASSERT_TRUE(sign_retry_chainlock(chain, signers,
        same_height ? 5 : 4,
        same_height ? second_tip : cryptonote::get_block_hash(rival_first),
        second_lock));
    ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_alternative_lock(
        chain, first_lock, first_tip));
    ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_alternative_lock(
        chain, second_lock, second_tip));

    const auto score_tip = [&](const crypto::hash &hash, uint64_t score) {
      cryptonote::alt_block_data_t data{};
      cryptonote::blobdata blob;
      if (!db.get_alt_block(hash, &data, &blob))
        return false;
      data.cumulative_difficulty_high = 0;
      data.cumulative_difficulty_low = score;
      cryptonote::db_wtxn_guard guard(&db);
      db.remove_alt_block(hash);
      db.add_alt_block(hash, data, blob);
      return true;
    };
    ASSERT_TRUE(score_tip(first_tip,
        first_tip == winner ? (reverse_order ? 2000 : 1000)
            : (reverse_order ? 1000 : 2000)));
    ASSERT_TRUE(score_tip(second_tip,
        second_tip == winner ? (reverse_order ? 2000 : 1000)
            : (reverse_order ? 1000 : 2000)));
    ASSERT_TRUE(score_tip(cryptonote::get_block_hash(difficulty_only), 5000));
    const auto store_lock = [&](const qwertycoin::epose::chainlock_v1 &lock) {
      db.put_chainlock({lock.height, lock.block_hash, false,
          qwertycoin::epose::serialize_chainlock_v1(lock)});
    };
    if (reverse_order)
    {
      store_lock(second_lock);
      store_lock(first_lock);
    }
    else
    {
      store_lock(first_lock);
      store_lock(second_lock);
    }
    ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::retry(chain));
    ASSERT_EQ(winner, chain.get_tail_id());
    ASSERT_EQ(6u, chain.get_current_blockchain_height());
    const auto before_restart = retry_lock_tuples(chain);
    ASSERT_EQ(2u, before_restart.size());
    ASSERT_TRUE(fixture.reopen());
    auto &reopened = fixture.blockchain();
    ASSERT_EQ(winner, reopened.get_tail_id());
    ASSERT_EQ(6u, reopened.get_current_blockchain_height());
    EXPECT_EQ(before_restart, retry_lock_tuples(reopened));
    uint64_t active_height = 0;
    crypto::hash active_hash{};
    ASSERT_TRUE(reopened.get_highest_main_chainlock(active_height, active_hash));
    EXPECT_EQ(5u, active_height);
    EXPECT_EQ(winner, active_hash);
  }
}

TEST(epose_blockchain_recovery, seeded_direct_extension_accepts_additional_coinbase_keys)
{
  seeded_recovery_chain fixture;
  ASSERT_TRUE(fixture.init());
  cryptonote::block candidate{};
  ASSERT_TRUE(fixture.template_block(candidate));
  ASSERT_TRUE(add_additional_coinbase_keys(candidate));
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(candidate, verdict));
  EXPECT_TRUE(verdict.m_added_to_main_chain);
}

TEST(epose_blockchain_recovery, hf18_default_reopen_retains_recoverable_alternative)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      add_coinbase_padding));
  const crypto::hash first_hash = cryptonote::get_block_hash(alternative_first);
  const crypto::hash second_hash = cryptonote::get_block_hash(alternative_second);
  ASSERT_TRUE(fixture.blockchain().get_db().get_alt_block(first_hash, nullptr, nullptr));
  ASSERT_TRUE(fixture.blockchain().get_db().get_alt_block(second_hash, nullptr, nullptr));
  const crypto::hash canonical_tip = fixture.blockchain().get_tail_id();
  ASSERT_TRUE(fixture.reopen());
  EXPECT_EQ(canonical_tip, fixture.blockchain().get_tail_id());
  EXPECT_TRUE(fixture.blockchain().get_db().get_alt_block(first_hash, nullptr, nullptr));
  EXPECT_TRUE(fixture.blockchain().get_db().get_alt_block(second_hash, nullptr, nullptr));
}

TEST(epose_blockchain_recovery, hf18_retry_requires_write_batch_and_wrapper_switches)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      add_coinbase_padding));
  const crypto::hash original_tip = fixture.blockchain().get_tail_id();
  const crypto::hash alternative_tip = cryptonote::get_block_hash(alternative_second);
  ASSERT_FALSE(fixture.blockchain().get_db().batch_active());
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::switch_branch(
      fixture.blockchain(), alternative_tip, false));
  EXPECT_EQ(original_tip, fixture.blockchain().get_tail_id());
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::switch_branch(
      fixture.blockchain(), alternative_tip, true));
  EXPECT_EQ(alternative_tip, fixture.blockchain().get_tail_id());
}

TEST(epose_blockchain_recovery, alternative_child_index_visits_ten_thousand_blocks_linearly)
{
  constexpr size_t count = 10027;
  cryptonote::alternative_child_index children(count);
  std::unordered_map<crypto::hash, uint64_t> difficulty;
  std::vector<crypto::hash> hashes;
  hashes.reserve(count);
  for (uint64_t index = 0; index < count; ++index)
  {
    hashes.push_back(crypto::cn_fast_hash(&index, sizeof(index)));
    difficulty.emplace(hashes.back(), index);
    if (index != 0)
      children.add(hashes[index - 1], hashes.back());
  }
  EXPECT_EQ(hashes.back(), children.best_descendant(hashes.front(), difficulty));
  for (size_t index = 0; index < count; ++index)
    EXPECT_EQ(index + 1 != count, children.has_child(hashes[index]));
  const crypto::hash left = crypto::cn_fast_hash("left", 4);
  const crypto::hash right = crypto::cn_fast_hash("right", 5);
  children.add(hashes.back(), left);
  children.add(hashes.back(), right);
  difficulty.emplace(left, count);
  difficulty.emplace(right, count);
  EXPECT_EQ(std::min(left, right),
      children.best_descendant(hashes.front(), difficulty));
  EXPECT_LE(children.operations(), count * 4 + 8);
}

TEST(epose_blockchain_recovery, production_descendant_search_visits_ten_thousand_blocks_linearly)
{
  constexpr size_t count = 10027;
  cryptonote_test::BlockchainAndPool fixture;
  cryptonote::BlockchainRecoveryHarness::configure(fixture.bc);
  auto &db = static_cast<cryptonote::BlockchainTestDB &>(fixture.bc.get_db());
  cryptonote::block candidate{};
  ASSERT_TRUE(cryptonote::generate_genesis_block(candidate,
      cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_TX,
      cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_NONCE,
      HF_VERSION_QWC_CONTRACTS, HF_VERSION_QWC_CONTRACTS));
  crypto::hash first{};
  crypto::hash parent{};
  for (size_t index = 0; index < count; ++index)
  {
    candidate.nonce = index + 1;
    candidate.prev_id = parent;
    const crypto::hash hash = crypto::cn_fast_hash(&index, sizeof(index));
    if (index == 0)
      first = hash;
    const cryptonote::blobdata blob = cryptonote::block_to_blob(candidate);
    db.add_alt_block(hash, {index + 1, 0, index + 1, 0, 0}, blob);
    parent = hash;
  }
  size_t operations = 0;
  EXPECT_EQ(parent, fixture.bc.best_alternative_descendant(first, &operations));
  EXPECT_GE(operations, count * 2);
  EXPECT_LE(operations, count * 3);
}

namespace
{
  crypto::hash populate_shared_prefix_alternatives(cryptonote::BlockchainTestDB &db,
      crypto::hash parent, size_t prefix_count, size_t tip_count)
  {
    cryptonote::block genesis{};
    if (!cryptonote::generate_genesis_block(genesis,
            cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_TX,
            cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_NONCE,
            HF_VERSION_QWC_CONTRACTS, HF_VERSION_QWC_CONTRACTS))
      return crypto::null_hash;
    for (size_t index = 0; index < prefix_count; ++index)
    {
      cryptonote::block candidate = genesis;
      candidate.nonce = index + 1;
      candidate.prev_id = parent;
      candidate.invalidate_hashes();
      parent = cryptonote::get_block_hash(candidate);
      db.add_alt_block(parent, {index + 1, 0, index + 11, 0, 0},
          cryptonote::block_to_blob(candidate));
    }
    crypto::hash tip{};
    for (size_t index = 0; index < tip_count; ++index)
    {
      cryptonote::block candidate = genesis;
      candidate.nonce = prefix_count + index + 1;
      candidate.prev_id = parent;
      candidate.invalidate_hashes();
      tip = cryptonote::get_block_hash(candidate);
      db.add_alt_block(tip, {prefix_count + 1, 0,
          prefix_count + index + 11, 0, 0}, cryptonote::block_to_blob(candidate));
    }
    return tip;
  }
}

TEST(epose_blockchain_recovery, alternative_tip_index_shares_long_prefix_across_ten_thousand_blocks)
{
  constexpr size_t prefix_count = 5027;
  constexpr size_t tip_count = 5000;
  cryptonote_test::BlockchainAndPool fixture;
  cryptonote::BlockchainRecoveryHarness::configure(fixture.bc);
  auto &db = static_cast<cryptonote::BlockchainTestDB &>(fixture.bc.get_db());
  ASSERT_NE(crypto::null_hash,
      populate_shared_prefix_alternatives(db, crypto::null_hash,
          prefix_count, tip_count));
  size_t operations = 0;
  const auto tips = fixture.bc.get_alternative_tips(&operations);
  EXPECT_EQ(tip_count, tips.size());
  EXPECT_LE(operations, (prefix_count + tip_count) * 3);
}

TEST(epose_blockchain_recovery, verified_retry_priority_is_order_independent)
{
  const crypto::hash first_lock = epose_coordinator_fixture::hash_text("retry-first-lock");
  const crypto::hash second_lock = epose_coordinator_fixture::hash_text("retry-second-lock");
  const crypto::hash lower_lock = std::min(first_lock, second_lock);
  const crypto::hash higher_lock = std::max(first_lock, second_lock);
  const crypto::hash first_tip = epose_coordinator_fixture::hash_text("retry-first-tip");
  const crypto::hash second_tip = epose_coordinator_fixture::hash_text("retry-second-tip");
  const crypto::hash lower_tip = std::min(first_tip, second_tip);
  const crypto::hash higher_tip = std::max(first_tip, second_tip);
  std::vector<cryptonote::epose_retry_priority> candidates{
      {false, 0, {}, 999, higher_tip},
      {true, 1211, lower_lock, 500, higher_tip},
      {true, 1212, higher_lock, 400, higher_tip},
      {true, 1212, lower_lock, 50, higher_tip},
      {true, 1212, lower_lock, 60, higher_tip},
      {true, 1212, lower_lock, 60, lower_tip}};
  const auto winner = [](const auto &ordered) {
    auto best = ordered.front();
    for (const auto &candidate : ordered)
      if (cryptonote::better_epose_retry_priority(candidate, best))
        best = candidate;
    return best;
  };
  EXPECT_EQ(lower_tip, winner(candidates).tip);
  std::reverse(candidates.begin(), candidates.end());
  EXPECT_EQ(lower_tip, winner(candidates).tip);
  EXPECT_TRUE(cryptonote::better_epose_retry_priority(candidates.front(),
      candidates.back()));
}

TEST(epose_blockchain_recovery, verified_retry_orders_persist_across_restart)
{
  verified_retry_order_case(false, false);
  verified_retry_order_case(false, true);
  verified_retry_order_case(true, false);
  verified_retry_order_case(true, true);
}

TEST(epose_blockchain_recovery, retry_selection_reads_shared_prefix_once_for_many_tips)
{
  constexpr size_t prefix_count = 5027;
  constexpr size_t tip_count = 5000;
  cryptonote_test::BlockchainAndPool fixture;
  cryptonote::BlockchainRecoveryHarness::configure_preview(fixture.bc);
  cryptonote::BlockchainRecoveryHarness::set_height(fixture.bc,
      prefix_count + tip_count + 10);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(fixture.bc));
  auto &db = static_cast<cryptonote::BlockchainTestDB &>(fixture.bc.get_db());
  const crypto::hash expected_tip = populate_shared_prefix_alternatives(db,
      db.get_block_hash_from_height(0), prefix_count, tip_count);
  ASSERT_NE(crypto::null_hash, expected_tip);
  ASSERT_EQ(prefix_count + tip_count, db.get_alt_block_count());
  const size_t ancestor_walks =
      cryptonote::BlockchainRecoveryHarness::ancestor_walks(fixture.bc);
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::retry_with_exhausted_tip(
      fixture.bc, expected_tip));
  EXPECT_LE(db.alternative_reads, (prefix_count + tip_count) * 3);
  EXPECT_LE(cryptonote::BlockchainRecoveryHarness::ancestor_walks(fixture.bc)
          - ancestor_walks, (prefix_count + tip_count) * 3);
}

TEST(epose_blockchain_recovery, retry_skips_disconnected_high_difficulty_tip)
{
  cryptonote_test::BlockchainAndPool fixture;
  cryptonote::BlockchainRecoveryHarness::configure_preview(fixture.bc);
  cryptonote::BlockchainRecoveryHarness::set_height(fixture.bc, 3);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(fixture.bc));
  auto &db = static_cast<cryptonote::BlockchainTestDB &>(fixture.bc.get_db());
  cryptonote::block genesis{};
  ASSERT_TRUE(cryptonote::generate_genesis_block(genesis,
      cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_TX,
      cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_NONCE,
      HF_VERSION_QWC_CONTRACTS, HF_VERSION_QWC_CONTRACTS));
  cryptonote::block valid = genesis;
  valid.nonce = 1;
  valid.prev_id = db.get_block_hash_from_height(0);
  valid.invalidate_hashes();
  const crypto::hash valid_hash = cryptonote::get_block_hash(valid);
  db.add_alt_block(valid_hash, {1, 0, 20, 0, 0},
      cryptonote::block_to_blob(valid));
  cryptonote::block disconnected = valid;
  disconnected.nonce = 2;
  disconnected.prev_id = epose_coordinator_fixture::hash_text("missing-parent");
  disconnected.invalidate_hashes();
  db.add_alt_block(cryptonote::get_block_hash(disconnected),
      {1, 0, 100, 0, 0}, cryptonote::block_to_blob(disconnected));
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::retry_with_exhausted_tip(
      fixture.bc, valid_hash));
}

TEST(epose_blockchain_recovery, aborted_batch_is_not_reported_as_committed)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  auto &chain = fixture.blockchain();
  const uint64_t height = chain.get_current_blockchain_height();
  cryptonote::block next{};
  ASSERT_TRUE(fixture.template_block(next));
  cryptonote::BlockchainRecoveryHarness::abort_batch_after_next_block(chain);
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(fixture.submit(next, verdict));
  EXPECT_EQ(height, chain.get_current_blockchain_height());
}

TEST(epose_blockchain_recovery, single_intake_reports_batch_abort)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  auto &chain = fixture.blockchain();
  const uint64_t height = chain.get_current_blockchain_height();
  cryptonote::block next{};
  ASSERT_TRUE(fixture.template_block(next));
  cryptonote::BlockchainRecoveryHarness::abort_batch_after_next_block(chain);
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(fixture.submit_single(next, verdict));
  EXPECT_FALSE(verdict.m_added_to_main_chain);
  EXPECT_EQ(height, chain.get_current_blockchain_height());
}

TEST(epose_blockchain_recovery, mined_block_reports_batch_abort)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  auto &chain = fixture.blockchain();
  const uint64_t height = chain.get_current_blockchain_height();
  cryptonote::block next{};
  ASSERT_TRUE(fixture.template_block(next));
  cryptonote::BlockchainRecoveryHarness::abort_batch_after_next_block(chain);
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(fixture.node().handle_block_found(next, verdict));
  EXPECT_FALSE(verdict.m_added_to_main_chain);
  EXPECT_EQ(height, chain.get_current_blockchain_height());
}

TEST(epose_blockchain_recovery, hf18_prunes_old_irrelevant_alternatives_but_keeps_recovery_paths)
{
  cryptonote_test::BlockchainAndPool fixture;
  cryptonote::BlockchainRecoveryHarness::configure_preview(fixture.bc);
  cryptonote::BlockchainRecoveryHarness::set_height(fixture.bc, 1211);
  auto &db = static_cast<cryptonote::BlockchainTestDB &>(fixture.bc.get_db());
  cryptonote::block genesis{};
  ASSERT_TRUE(cryptonote::generate_genesis_block(genesis,
      cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_TX,
      cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_NONCE,
      HF_VERSION_QWC_CONTRACTS, HF_VERSION_QWC_CONTRACTS));
  const auto add = [&](uint64_t height, uint32_t nonce,
      const crypto::hash &parent) {
    cryptonote::block candidate = genesis;
    candidate.nonce = nonce;
    candidate.prev_id = parent;
    const crypto::hash hash = cryptonote::get_block_hash(candidate);
    const cryptonote::blobdata blob = cryptonote::block_to_blob(candidate);
    db.add_alt_block(hash, {height, 0, 0, 0, 0}, blob);
    return hash;
  };
  const crypto::hash stale = add(100, 1, crypto::null_hash);
  const crypto::hash malformed = crypto::cn_fast_hash("malformed", 9);
  db.add_alt_block(malformed, {100, 0, 0, 0, 0}, "invalid block");
  const crypto::hash protected_old = add(101, 2, crypto::null_hash);
  const crypto::hash recovery_root = add(1209, 3, crypto::null_hash);
  const crypto::hash recovery_child = add(1210, 4, recovery_root);
  const crypto::hash recovery_tip = add(1211, 5, recovery_child);
  db.stored_chainlocks.push_back({101, protected_old, false, "complete"});
  fixture.bc.prune_irrecoverable_alternatives();
  EXPECT_FALSE(db.get_alt_block(stale, nullptr, nullptr));
  EXPECT_FALSE(db.get_alt_block(malformed, nullptr, nullptr));
  EXPECT_FALSE(db.get_alt_block(protected_old, nullptr, nullptr));
  EXPECT_TRUE(db.get_alt_block(recovery_root, nullptr, nullptr));
  EXPECT_TRUE(db.get_alt_block(recovery_child, nullptr, nullptr));
  EXPECT_TRUE(db.get_alt_block(recovery_tip, nullptr, nullptr));
}

void verify_prune_failure_case(bool core_owned_batch)
{
  SCOPED_TRACE(core_owned_batch ? "core-owned batch" : "caller-owned batch");
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  cryptonote::block old_first{};
  cryptonote::block old_second{};
  ASSERT_TRUE(fixture.seed_branch(old_first, old_second, add_coinbase_padding));
  const crypto::hash old_hash = cryptonote::get_block_hash(old_first);
  const crypto::hash old_second_hash = cryptonote::get_block_hash(old_second);
  auto &chain = fixture.blockchain();
  ASSERT_TRUE(chain.get_db().get_alt_block(old_hash, nullptr, nullptr));
  while (chain.get_current_blockchain_height() < 720)
  {
    cryptonote::block next{};
    ASSERT_TRUE(fixture.template_block(next));
    cryptonote::block_verification_context verdict{};
    ASSERT_TRUE(fixture.submit(next, verdict));
    ASSERT_TRUE(verdict.m_added_to_main_chain);
  }
  cryptonote::block next{};
  ASSERT_TRUE(fixture.template_block(next));
  cryptonote::block recent = next;
  ++recent.nonce;
  ASSERT_TRUE(add_coinbase_padding(recent));
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(next, verdict));
  ASSERT_TRUE(verdict.m_added_to_main_chain);
  verdict = {};
  ASSERT_TRUE(fixture.submit(recent, verdict));
  const crypto::hash recent_hash = cryptonote::get_block_hash(recent);
  ASSERT_TRUE(chain.get_db().get_alt_block(recent_hash, nullptr, nullptr));
  cryptonote::BlockchainRecoveryHarness::fail_alternative_removal_after_one(chain);
  while (chain.get_current_blockchain_height() < 736)
  {
    cryptonote::block advanced{};
    ASSERT_TRUE(fixture.template_block(advanced));
    verdict = {};
    const bool accepted = core_owned_batch
        && chain.get_current_blockchain_height() == 735
        ? fixture.submit_single(advanced, verdict)
        : fixture.submit(advanced, verdict);
    ASSERT_TRUE(accepted);
    ASSERT_TRUE(verdict.m_added_to_main_chain);
  }
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::alternative_prune_pending(chain));
  EXPECT_TRUE(chain.get_db().get_alt_block(old_hash, nullptr, nullptr));
  EXPECT_TRUE(chain.get_db().get_alt_block(old_second_hash, nullptr, nullptr));
  chain.prune_after_canonical_advance(735);
  EXPECT_TRUE(chain.get_db().get_alt_block(old_hash, nullptr, nullptr));
  EXPECT_TRUE(chain.get_db().get_alt_block(old_second_hash, nullptr, nullptr));
  cryptonote::BlockchainRecoveryHarness::allow_alternative_removal(chain);
  chain.prune_after_canonical_advance(736);
  EXPECT_FALSE(chain.get_db().get_alt_block(old_hash, nullptr, nullptr));
  EXPECT_FALSE(chain.get_db().get_alt_block(old_second_hash, nullptr, nullptr));
  EXPECT_TRUE(chain.get_db().get_alt_block(recent_hash, nullptr, nullptr));
}

TEST(epose_blockchain_recovery, hf18_prunes_stale_alternatives_during_canonical_advance)
{
  verify_prune_failure_case(false);
}

TEST(epose_blockchain_recovery, hf18_prune_fault_core_owned_batch)
{
  verify_prune_failure_case(true);
}

TEST(epose_blockchain_recovery, hf18_prunes_after_alternative_reorganization)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  cryptonote::block old_first{};
  cryptonote::block old_second{};
  ASSERT_TRUE(fixture.seed_branch(old_first, old_second, add_coinbase_padding));
  auto &chain = fixture.blockchain();
  const crypto::hash stale_hash = cryptonote::get_block_hash(old_first);
  while (chain.get_current_blockchain_height() < 734)
  {
    cryptonote::block next{};
    ASSERT_TRUE(fixture.template_block(next));
    cryptonote::block_verification_context verdict{};
    ASSERT_TRUE(fixture.submit(next, verdict));
    ASSERT_TRUE(verdict.m_added_to_main_chain);
  }
  cryptonote::block main_first{};
  ASSERT_TRUE(fixture.template_block(main_first));
  cryptonote::block alternative_first = main_first;
  ++alternative_first.nonce;
  ASSERT_TRUE(add_coinbase_padding(alternative_first));
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(main_first, verdict));
  ASSERT_TRUE(verdict.m_added_to_main_chain);
  verdict = {};
  ASSERT_TRUE(fixture.submit(alternative_first, verdict));
  ASSERT_FALSE(verdict.m_added_to_main_chain);
  ASSERT_TRUE(chain.get_db().get_alt_block(stale_hash, nullptr, nullptr));
  cryptonote::block alternative_second{};
  ASSERT_TRUE(fixture.branch_tip(alternative_first, alternative_second));
  verdict = {};
  ASSERT_TRUE(fixture.submit(alternative_second, verdict));
  ASSERT_TRUE(verdict.m_added_to_main_chain);
  EXPECT_EQ(736u, chain.get_current_blockchain_height());
  EXPECT_EQ(cryptonote::get_block_hash(alternative_second), chain.get_tail_id());
  EXPECT_FALSE(chain.get_db().get_alt_block(stale_hash, nullptr, nullptr));
  EXPECT_TRUE(chain.get_db().get_alt_block(
      cryptonote::get_block_hash(main_first), nullptr, nullptr));
}

TEST(epose_warning_throttle, on_idle_retries_each_failure_and_logs_once_per_epoch)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  uint64_t epoch = 8;
  unsigned attempts = 0;
  unsigned warnings = 0;
  cryptonote::BlockchainRecoveryHarness::observe_idle_service_failures(
      fixture.node(), epoch, attempts, warnings);
  for (unsigned index = 0; index < 3; ++index)
    ASSERT_TRUE(fixture.node().on_idle());
  EXPECT_EQ(3u, attempts);
  EXPECT_EQ(1u, warnings);
  epoch = 9;
  for (unsigned index = 0; index < 2; ++index)
    ASSERT_TRUE(fixture.node().on_idle());
  EXPECT_EQ(5u, attempts);
  EXPECT_EQ(2u, warnings);
}

TEST(epose_blockchain_recovery, signer_guard_resumes_only_after_verified_canonical_supersession)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      add_coinbase_padding));
  cryptonote::block descendant{};
  ASSERT_TRUE(fixture.template_block(descendant));
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(descendant, verdict));
  ASSERT_TRUE(verdict.m_added_to_main_chain);

  qwertycoin::epose::epoch_timing_v2 timing{};
  timing.activation_height = 0;
  timing.epoch_length = 2;
  timing.anchor_depth = 1;
  std::vector<std::pair<crypto::public_key, crypto::secret_key>> signers(10);
  std::vector<crypto::public_key> candidates;
  for (auto &signer : signers)
  {
    crypto::generate_keys(signer.first, signer.second);
    candidates.push_back(signer.first);
  }
  auto &chain = fixture.blockchain();
  ASSERT_TRUE(chain.set_chainlock_test_quorum(timing, candidates));
  uint64_t descendant_epoch = 0;
  qwertycoin::epose::chainlock_quorum_v1 descendant_quorum;
  ASSERT_TRUE(chain.get_chainlock_epoch(3, descendant_epoch));
  ASSERT_TRUE(chain.get_chainlock_quorum(descendant_epoch, descendant_quorum));
  const auto selected_signer = std::find_if(signers.begin(), signers.end(),
      [&](const auto &candidate) {
        return std::find(descendant_quorum.members.begin(),
            descendant_quorum.members.end(), candidate.first)
            != descendant_quorum.members.end();
      });
  ASSERT_NE(signers.end(), selected_signer);
  cryptonote::synchronized_protocol_stub protocol;
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::set_signer_vote(
      fixture.node(), protocol, fixture.signer_path(),
      selected_signer->first, selected_signer->second, 1,
      cryptonote::get_block_hash(alternative_first)));
  const crypto::hash descendant_hash = cryptonote::get_block_hash(descendant);
  EXPECT_EQ(1u, cryptonote::BlockchainRecoveryHarness::attempt_sign(
      fixture.node(), 3, descendant_hash));

  qwertycoin::epose::chainlock_v1 complete{};
  ASSERT_TRUE(sign_retry_chainlock(chain, signers, 2,
      fixture.original_second(), complete));
  chain.get_db().put_chainlock({2, complete.block_hash, false,
      qwertycoin::epose::serialize_chainlock_v1(complete)});
  ASSERT_TRUE(chain.canonical_chainlock_supersedes(1));

  ASSERT_TRUE(fixture.reopen());
  ASSERT_TRUE(fixture.blockchain().set_chainlock_test_quorum(timing, candidates));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::load_signer_vote(
      fixture.node(), protocol, fixture.signer_path(),
      selected_signer->first, selected_signer->second));
  EXPECT_EQ(3u, cryptonote::BlockchainRecoveryHarness::attempt_sign(
      fixture.node(), 3, descendant_hash));
  qwertycoin::epose::chainlock_signer_state_v1 persisted{};
  std::string error;
  ASSERT_TRUE(qwertycoin::epose::load_chainlock_signer_state_v1(
      fixture.signer_path() + ".chainlock", persisted, error));
  EXPECT_EQ(3u, persisted.height);
  EXPECT_EQ(descendant_hash, persisted.block_hash);
}

TEST(epose_blockchain_recovery, archive_replay_blocks_only)
{
  const char *negative_dir = std::getenv("QWC_CHAINLOCK_REPLAY_NEGATIVE_DIR");
  if (!negative_dir)
    GTEST_SKIP() << "Archive replay inputs are supplied by chainlock_archive_replay.py";

  crypto::hash stale_hash{};
  uint64_t original_alternatives = 0;
  {
    cryptonote::BlockchainLMDB db;
    db.open((boost::filesystem::path(negative_dir) / "devnet" / "lmdb").string());
    original_alternatives = db.get_alt_block_count();
    cryptonote::block stale{};
    ASSERT_TRUE(cryptonote::generate_genesis_block(stale,
        cryptonote::get_config(cryptonote::DEVNET).GENESIS_TX,
        cryptonote::get_config(cryptonote::DEVNET).GENESIS_NONCE,
        HF_VERSION_QWC_CONTRACTS, HF_VERSION_QWC_CONTRACTS));
    stale.nonce += 777;
    stale_hash = cryptonote::get_block_hash(stale);
    const cryptonote::blobdata blob = cryptonote::block_to_blob(stale);
    {
      cryptonote::db_wtxn_guard guard(&db);
      db.add_alt_block(stale_hash, {100, 0, 0, 0, 0}, blob);
    }
    EXPECT_EQ(original_alternatives + 1, db.get_alt_block_count());
    db.close();
  }

  {
    cryptonote::core negative(nullptr);
    ASSERT_TRUE(open_archive_core(negative, negative_dir));
    auto &chain = negative.get_blockchain_storage();
    uint64_t active_height = 0;
    crypto::hash active_hash{};
    ASSERT_TRUE(chain.get_highest_main_chainlock(active_height, active_hash));
    EXPECT_EQ(1210u, active_height);
    EXPECT_EQ("cf7362c8eec286ba41564527f9f56aa3df5029ffe01f60587801d640a59d3ce7",
        epee::string_tools::pod_to_hex(active_hash));
    EXPECT_EQ(1211u, chain.get_current_blockchain_height());
    EXPECT_EQ(active_hash, chain.get_tail_id());
    EXPECT_GE(chain.get_db().get_alt_block_count(), 10027u);
    EXPECT_EQ(original_alternatives, chain.get_db().get_alt_block_count());
    EXPECT_FALSE(chain.get_db().get_alt_block(stale_hash, nullptr, nullptr));
    std::cout << "REPLAY_NEGATIVE height=" << chain.get_current_blockchain_height()
        << " tip=" << chain.get_tail_id() << " lock_height=" << active_height
        << " lock_hash=" << active_hash << '\n';
    ASSERT_TRUE(negative.deinit());
  }
}

TEST(epose_blockchain_recovery, archive_replay_chainlock_timing)
{
  const char *directory = std::getenv("QWC_CHAINLOCK_REPLAY_PERF_DIR");
  if (!directory)
    GTEST_SKIP() << "Archive copy is supplied by chainlock_archive_perf.py";
  cryptonote::core node(nullptr);
  ASSERT_TRUE(open_archive_core(node, directory));
  auto &chain = node.get_blockchain_storage();
  const auto timed = [](const char *name, const std::function<bool()> &call,
      int64_t *elapsed_us = nullptr) {
    const auto start = std::chrono::steady_clock::now();
    const bool result = call();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (elapsed_us)
      *elapsed_us = elapsed;
    std::cout << "CLPERF " << name << " us=" << elapsed << " result=" << result << '\n';
    return result;
  };
  cryptonote::chainlock_record stored{};
  ASSERT_TRUE(chain.get_db().get_chainlock(1210, chain.get_tail_id(), stored));
  qwertycoin::epose::chainlock_v1 lock{};
  ASSERT_TRUE(qwertycoin::epose::parse_chainlock_v1(stored.lock, lock));
  qwertycoin::epose::chainlock_quorum_v1 quorum{};
  ASSERT_TRUE(timed("quorum", [&] { return chain.get_chainlock_quorum(lock.epoch, quorum); }));
  ASSERT_TRUE(timed("verify_complete", [&] {
    return cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock);
  }));
  qwertycoin::epose::chainlock_context_v1 context{};
  ASSERT_TRUE(chain.get_chainlock_context(context));
  ASSERT_TRUE(timed("signatures", [&] {
    return qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, true);
  }));
  size_t scans = 0;
  ASSERT_TRUE(timed("db_scan", [&] {
    return chain.get_db().for_chainlocks_descending([&](const auto &) {
      ++scans;
      return true;
    });
  }));
  std::cout << "CLPERF db_scan_records=" << scans << '\n';
  cryptonote::chainlock_record point{};
  ASSERT_TRUE(timed("db_canonical_lookup", [&] {
    return chain.get_db().get_chainlock(1210, chain.get_tail_id(), point);
  }));
  uint64_t carrier_height = 0;
  cryptonote::chainlock_record last{};
  timed("db_last_carrier", [&] {
    return chain.get_db().get_last_chainlock_carrier(carrier_height, last);
  });
  cryptonote::blobdata selected{};
  cryptonote::block candidate{};
  cryptonote::chainlock_record accepted{};
  bool has_carrier = false;
  int64_t fastest_block_path_us = 0;
  for (unsigned attempt = 0; attempt < 5; ++attempt)
  {
    cryptonote::BlockchainRecoveryHarness::clear_chainlock_cache(chain);
    const uint64_t quorum_selections_before = chain.chainlock_quorum_selection_count();
    const uint64_t signature_verifications_before =
        cryptonote::BlockchainRecoveryHarness::chainlock_signature_verifications(chain);
    selected.clear();
    int64_t selection_us = 0;
    ASSERT_TRUE(timed("template_selection", [&] {
      if (attempt == 0 && std::getenv("QWC_CHAINLOCK_REPLAY_PERF_DELAY_FIRST"))
        std::this_thread::sleep_for(std::chrono::microseconds(2200));
      return cryptonote::BlockchainRecoveryHarness::select_chainlock(chain, 1211, selected);
    }, &selection_us));
    ASSERT_EQ(stored.lock, selected);
    candidate = {};
    candidate.major_version = HF_VERSION_QWC_CONTRACTS;
    ASSERT_TRUE(cryptonote::add_chainlock_to_tx_extra(candidate.miner_tx.extra, selected));
    accepted = {};
    has_carrier = false;
    int64_t validation_us = 0;
    ASSERT_TRUE(timed("carrier_validation", [&] {
      return cryptonote::BlockchainRecoveryHarness::validate_chainlock(
          chain, candidate, 1211, true, accepted, has_carrier);
    }, &validation_us));
    ASSERT_TRUE(has_carrier);
    const int64_t block_path_us = selection_us + validation_us;
    fastest_block_path_us = attempt == 0 ? block_path_us
        : std::min(fastest_block_path_us, block_path_us);
    std::cout << "CLPERF block_path_us=" << block_path_us << '\n';
    std::cout << "CLPERF quorum_selections_per_block="
        << chain.chainlock_quorum_selection_count() - quorum_selections_before << '\n';
    std::cout << "CLPERF signature_verifications_per_block="
        << cryptonote::BlockchainRecoveryHarness::chainlock_signature_verifications(chain)
            - signature_verifications_before << '\n';
    EXPECT_EQ(2u, chain.chainlock_quorum_selection_count() - quorum_selections_before);
    EXPECT_EQ(1u, cryptonote::BlockchainRecoveryHarness::chainlock_signature_verifications(chain)
        - signature_verifications_before);
    if (fastest_block_path_us < 1800)
      break;
  }
  EXPECT_LT(fastest_block_path_us, 1800);
  candidate.miner_tx.extra.clear();
  ASSERT_TRUE(timed("without_carrier", [&] {
    return cryptonote::BlockchainRecoveryHarness::validate_chainlock(
        chain, candidate, 1211, true, accepted, has_carrier);
  }));
  ASSERT_FALSE(has_carrier);
  ASSERT_TRUE(node.deinit());
}

TEST(epose_blockchain_recovery, chainlock_verification_cache_checks_input_and_pop)
{
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  auto &chain = fixture.blockchain();
  while (chain.get_current_blockchain_height() < 5)
  {
    cryptonote::block next{};
    ASSERT_TRUE(fixture.template_block(next));
    cryptonote::block_verification_context verdict{};
    ASSERT_TRUE(fixture.submit(next, verdict));
    ASSERT_TRUE(verdict.m_added_to_main_chain);
  }
  qwertycoin::epose::epoch_timing_v2 timing{};
  timing.activation_height = 0;
  timing.epoch_length = 2;
  timing.anchor_depth = 1;
  std::vector<std::pair<crypto::public_key, crypto::secret_key>> signers(11);
  std::vector<crypto::public_key> candidates;
  for (auto &signer : signers)
  {
    crypto::generate_keys(signer.first, signer.second);
    candidates.push_back(signer.first);
  }
  const crypto::public_key replacement = candidates.back();
  candidates.pop_back();
  ASSERT_TRUE(chain.set_chainlock_test_quorum(timing, candidates));
  qwertycoin::epose::chainlock_v1 lock{};
  ASSERT_TRUE(sign_retry_chainlock(chain, signers, 2,
      chain.get_block_id_by_height(2), lock));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::has_chainlock_cache(chain));
  const uint64_t first_verifications =
      cryptonote::BlockchainRecoveryHarness::chainlock_signature_verifications(chain);
  qwertycoin::epose::chainlock_v1 other_lock{};
  ASSERT_TRUE(sign_retry_chainlock(chain, signers, 2,
      chain.get_block_id_by_height(3), other_lock));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, other_lock));
  EXPECT_EQ(first_verifications + 1,
      cryptonote::BlockchainRecoveryHarness::chainlock_signature_verifications(chain));
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock));
  EXPECT_EQ(first_verifications + 1,
      cryptonote::BlockchainRecoveryHarness::chainlock_signature_verifications(chain));

  auto changed = lock;
  reinterpret_cast<unsigned char *>(&changed.signatures.front())[0] ^= 1;
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, changed));
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock));
  changed = lock;
  changed.height = 3;
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, changed));
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock));

  qwertycoin::epose::chainlock_quorum_v1 original_quorum{};
  ASSERT_TRUE(chain.get_chainlock_quorum(lock.epoch, original_quorum));
  auto &test_candidates = cryptonote::BlockchainRecoveryHarness::chainlock_test_candidates(chain);
  auto selected = std::find(test_candidates.begin(), test_candidates.end(),
      original_quorum.members.front());
  ASSERT_NE(test_candidates.end(), selected);
  const crypto::public_key original_candidate = *selected;
  *selected = replacement;
  qwertycoin::epose::chainlock_quorum_v1 changed_quorum{};
  ASSERT_TRUE(chain.get_chainlock_quorum(lock.epoch, changed_quorum));
  ASSERT_NE(original_quorum.members, changed_quorum.members);
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock));
  *selected = original_candidate;
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, lock));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::has_chainlock_cache(chain));

  for (unsigned index = 1; index <= QWC_CHAINLOCK_QUORUM_MAX + 1; ++index)
  {
    crypto::hash hash{};
    hash.data[0] = static_cast<unsigned char>(index);
    qwertycoin::epose::chainlock_v1 distinct{};
    ASSERT_TRUE(sign_retry_chainlock(chain, signers, 2, hash, distinct));
    ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::verify_chainlock(chain, distinct));
    EXPECT_LE(cryptonote::BlockchainRecoveryHarness::chainlock_cache_size(chain),
        QWC_CHAINLOCK_QUORUM_MAX);
  }

  chain.pop_blocks(3);
  EXPECT_EQ(2u, chain.get_current_blockchain_height());
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::has_chainlock_cache(chain));
}

TEST(epose_blockchain_recovery, seeded_539_later_complete_lock_switches_and_survives_restart)
{
  seeded_recovery_chain fixture(true, true);
  ASSERT_TRUE(fixture.init());
  auto &chain = fixture.blockchain();
  while (chain.get_current_blockchain_height() < 1211)
  {
    cryptonote::block next{};
    ASSERT_TRUE(fixture.template_block(next));
    cryptonote::block_verification_context verdict{};
    ASSERT_TRUE(fixture.submit(next, verdict));
    ASSERT_TRUE(verdict.m_added_to_main_chain);
  }
  const crypto::hash stuck_tip = chain.get_tail_id();
  ASSERT_EQ(1211u, chain.get_current_blockchain_height());
  cryptonote::block later_template{};
  ASSERT_TRUE(fixture.template_block(later_template));
  qwertycoin::epose::epoch_timing_v2 timing{};
  timing.activation_height = 0;
  timing.epoch_length = 60;
  timing.anchor_depth = 1;
  std::vector<std::pair<crypto::public_key, crypto::secret_key>> signers(10);
  std::vector<crypto::public_key> candidates;
  for (size_t index = 0; index < signers.size(); ++index)
  {
    crypto::secret_key seed{};
    seed.data[0] = static_cast<unsigned char>(index + 1);
    crypto::generate_keys(signers[index].first, signers[index].second, seed, true);
    candidates.push_back(signers[index].first);
  }
  ASSERT_TRUE(chain.set_chainlock_test_quorum(timing, candidates));
  const auto submit_lock = [&](uint64_t height, const crypto::hash &hash) {
    qwertycoin::epose::chainlock_v1 lock{};
    if (!sign_retry_chainlock(chain, signers, height, hash, lock))
      return false;
    cryptonote::blobdata relay;
    return chain.handle_incoming_chainlock(
        qwertycoin::epose::serialize_chainlock_v1(lock), relay)
        == cryptonote::Blockchain::chainlock_ingress::accepted;
  };
  ASSERT_TRUE(submit_lock(1206, chain.get_block_id_by_height(1206)));
  ASSERT_TRUE(submit_lock(1209, chain.get_block_id_by_height(1209)));
  ASSERT_TRUE(submit_lock(1210, stuck_tip));

  const auto fork_block = [&](uint64_t height, const crypto::hash &parent,
      cryptonote::block &candidate) {
    candidate = chain.get_db().get_block_from_height(height);
    candidate.prev_id = parent;
    ++candidate.nonce;
    if (!add_coinbase_padding(candidate))
      return false;
    candidate.invalidate_hashes();
    cryptonote::block_verification_context verdict{};
    return fixture.submit(candidate, verdict) && !verdict.m_verifivation_failed;
  };
  cryptonote::block fork_1206{};
  ASSERT_TRUE(fork_block(1206, chain.get_block_id_by_height(1205), fork_1206));
  ASSERT_TRUE(submit_lock(1206, cryptonote::get_block_hash(fork_1206)));
  cryptonote::chainlock_record competing_lock{};
  ASSERT_TRUE(chain.get_db().get_chainlock(1206,
      cryptonote::get_block_hash(fork_1206), competing_lock));
  EXPECT_FALSE(competing_lock.tombstoned);
  crypto::hash alternative_parent = cryptonote::get_block_hash(fork_1206);
  for (uint64_t height = 1207; height < 1209; ++height)
  {
    cryptonote::block intermediate{};
    ASSERT_TRUE(fork_block(height, alternative_parent, intermediate));
    alternative_parent = cryptonote::get_block_hash(intermediate);
  }
  cryptonote::block fork_1209{};
  ASSERT_TRUE(fork_block(1209, alternative_parent, fork_1209));
  ASSERT_TRUE(submit_lock(1209, cryptonote::get_block_hash(fork_1209)));
  ASSERT_TRUE(chain.get_db().get_chainlock(1209,
      cryptonote::get_block_hash(fork_1209), competing_lock));
  EXPECT_FALSE(competing_lock.tombstoned);
  cryptonote::block fork_1210{};
  ASSERT_TRUE(fork_block(1210, cryptonote::get_block_hash(fork_1209), fork_1210));
  cryptonote::block later = later_template;
  later.prev_id = cryptonote::get_block_hash(fork_1210);
  ++later.nonce;
  ASSERT_TRUE(add_coinbase_padding(later));
  later.invalidate_hashes();
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(later, verdict));
  ASSERT_FALSE(verdict.m_added_to_main_chain);
  cryptonote::block forged_candidate = later;
  ++forged_candidate.nonce;
  ASSERT_TRUE(add_coinbase_padding(forged_candidate));
  forged_candidate.invalidate_hashes();
  verdict = {};
  ASSERT_TRUE(fixture.submit(forged_candidate, verdict));
  ASSERT_FALSE(verdict.m_added_to_main_chain);
  const crypto::hash forged_hash = cryptonote::get_block_hash(forged_candidate);
  qwertycoin::epose::chainlock_v1 forged_lock{};
  ASSERT_TRUE(sign_retry_chainlock(chain, signers, 1211, forged_hash, forged_lock));
  cryptonote::blobdata forged_blob = qwertycoin::epose::serialize_chainlock_v1(forged_lock);
  forged_blob.back() ^= 1;
  qwertycoin::epose::chainlock_v1 parsed_forged{};
  ASSERT_TRUE(qwertycoin::epose::parse_chainlock_v1(forged_blob, parsed_forged));
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::verify_alternative_lock(
      chain, parsed_forged, forged_hash));
  chain.get_db().put_chainlock({1211, forged_hash, false, forged_blob});
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::retry(chain));
  constexpr char stuck_hash[] =
      "60fee88b9807bf7fc3b76527483b42e73d8db8d4d1435c9cc9407b62d3ab0027";
  constexpr char recovered_hash[] =
      "965ecc97cabfc15a0257cd3dde7c76140b85385531f13f014c01cdd9d06137f5";
  EXPECT_EQ(1211u, chain.get_current_blockchain_height());
  EXPECT_EQ(stuck_tip, chain.get_tail_id());
  EXPECT_EQ(stuck_hash, epee::string_tools::pod_to_hex(chain.get_tail_id()));
  uint64_t active_height = 0;
  crypto::hash active_hash{};
  ASSERT_TRUE(chain.get_highest_main_chainlock(active_height, active_hash));
  EXPECT_EQ(1210u, active_height);
  EXPECT_EQ(stuck_tip, active_hash);
  EXPECT_EQ(stuck_hash, epee::string_tools::pod_to_hex(active_hash));
  std::cout << "REPLAY_SYNTHETIC_BEFORE height=" << chain.get_current_blockchain_height()
      << " tip=" << chain.get_tail_id() << " lock_height=" << active_height
      << " lock_hash=" << active_hash << '\n';
  const crypto::hash recovered_tip = cryptonote::get_block_hash(later);
  qwertycoin::epose::chainlock_v1 later_lock{};
  ASSERT_TRUE(sign_retry_chainlock(chain, signers, 1211, recovered_tip, later_lock));
  cryptonote::blobdata invalid_blob = qwertycoin::epose::serialize_chainlock_v1(later_lock);
  invalid_blob.back() ^= 1;
  cryptonote::blobdata relay;
  EXPECT_EQ(cryptonote::Blockchain::chainlock_ingress::ignored,
      chain.handle_incoming_chainlock(invalid_blob, relay));
  EXPECT_EQ(stuck_tip, chain.get_tail_id());
  EXPECT_EQ(cryptonote::Blockchain::chainlock_ingress::accepted,
      chain.handle_incoming_chainlock(
          qwertycoin::epose::serialize_chainlock_v1(later_lock), relay));
  EXPECT_EQ(1212u, chain.get_current_blockchain_height());
  EXPECT_EQ(recovered_tip, chain.get_tail_id());
  EXPECT_EQ(recovered_hash, epee::string_tools::pod_to_hex(chain.get_tail_id()));
  ASSERT_TRUE(chain.get_highest_main_chainlock(active_height, active_hash));
  EXPECT_EQ(1211u, active_height);
  EXPECT_EQ(recovered_tip, active_hash);
  EXPECT_EQ(recovered_hash, epee::string_tools::pod_to_hex(active_hash));
  std::cout << "REPLAY_SYNTHETIC_AFTER height=" << chain.get_current_blockchain_height()
      << " tip=" << chain.get_tail_id() << " lock_height=" << active_height
      << " lock_hash=" << active_hash << '\n';
  ASSERT_TRUE(fixture.reopen());
  auto &reopened = fixture.blockchain();
  ASSERT_TRUE(reopened.set_chainlock_test_quorum(timing, candidates));
  EXPECT_EQ(1212u, reopened.get_current_blockchain_height());
  EXPECT_EQ(recovered_tip, reopened.get_tail_id());
  EXPECT_EQ(recovered_hash, epee::string_tools::pod_to_hex(reopened.get_tail_id()));
  ASSERT_TRUE(reopened.get_highest_main_chainlock(active_height, active_hash));
  EXPECT_EQ(1211u, active_height);
  EXPECT_EQ(recovered_tip, active_hash);
  EXPECT_EQ(recovered_hash, epee::string_tools::pod_to_hex(active_hash));
  std::cout << "REPLAY_SYNTHETIC_RESTART height=" << reopened.get_current_blockchain_height()
      << " tip=" << reopened.get_tail_id() << " lock_height=" << active_height
      << " lock_hash=" << active_hash << '\n';
}

TEST(epose_blockchain_recovery, seeded_retry_switches_heavier_branch_after_rebuild_failure)
{
  bool observed_unbatched = false;
  seeded_recovery_chain fixture(true);
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      add_coinbase_padding));
  cryptonote::block_verification_context verdict{};

  cryptonote::block alternative_third{};
  ASSERT_TRUE(fixture.branch_tip(alternative_second, alternative_third));
  cryptonote::BlockchainRecoveryHarness::fail_next_rebuild(fixture.blockchain());
  cryptonote::BlockchainRecoveryHarness::delay_recovery(fixture.blockchain());
  cryptonote::BlockchainRecoveryHarness::recover_without_outer_batch(
      fixture.blockchain(), observed_unbatched);
  verdict = {};
  EXPECT_FALSE(fixture.submit(alternative_third, verdict));
  EXPECT_TRUE(verdict.m_retryable_busy);
  EXPECT_FALSE(verdict.m_verifivation_failed);
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::recovery_scheduled(
      fixture.blockchain()));
  EXPECT_FALSE(fixture.blockchain().get_db().batch_active());
  const auto expected_tip = cryptonote::get_block_hash(alternative_third);
  ASSERT_TRUE(fixture.blockchain().get_db().get_alt_block(expected_tip, nullptr, nullptr));
  const crypto::hash original_tip = fixture.blockchain().get_tail_id();
  EXPECT_FALSE(cryptonote::BlockchainRecoveryHarness::switch_branch(
      fixture.blockchain(), expected_tip, false));
  EXPECT_EQ(original_tip, fixture.blockchain().get_tail_id());
  EXPECT_FALSE(fixture.blockchain().get_db().batch_active());
  ASSERT_FALSE(fixture.blockchain().get_alternative_tips().empty());
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  while (std::chrono::steady_clock::now() < deadline)
  {
    ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
    if (fixture.blockchain().get_tail_id() == expected_tip && !diagnostics.rebuilding)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT_EQ(cryptonote::get_block_hash(alternative_third), fixture.blockchain().get_tail_id());
  EXPECT_EQ(4u, fixture.blockchain().get_current_blockchain_height());
  EXPECT_EQ(cryptonote::get_block_hash(alternative_first),
      fixture.blockchain().get_block_id_by_height(1));
  EXPECT_EQ(cryptonote::get_block_hash(alternative_second),
      fixture.blockchain().get_block_id_by_height(2));
  EXPECT_EQ(cryptonote::get_block_hash(alternative_third),
      fixture.blockchain().get_block_id_by_height(3));
  ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_FALSE(diagnostics.rebuilding);
  EXPECT_FALSE(diagnostics.fatal);
  EXPECT_TRUE(observed_unbatched);
}

TEST(epose_blockchain_recovery, invalid_preflight_keeps_original_tip_and_state)
{
  seeded_recovery_chain fixture;
  ASSERT_TRUE(fixture.init());
  cryptonote::block invalid_first{};
  cryptonote::block invalid_second{};
  ASSERT_TRUE(fixture.seed_branch(invalid_first, invalid_second, overmint_coinbase));
  cryptonote::block_verification_context verdict{};
  cryptonote::block invalid_third{};
  ASSERT_TRUE(fixture.branch_tip(invalid_second, invalid_third));
  const crypto::hash original_tip = fixture.blockchain().get_tail_id();
  const crypto::hash original_state = fixture.blockchain().get_epose_state_hash();
  verdict = {};
  EXPECT_FALSE(fixture.submit(invalid_third, verdict));
  EXPECT_TRUE(verdict.m_verifivation_failed);
  EXPECT_EQ(original_tip, fixture.blockchain().get_tail_id());
  EXPECT_EQ(original_state, fixture.blockchain().get_epose_state_hash());
  EXPECT_EQ(fixture.original_first(),
      fixture.blockchain().get_block_id_by_height(1));
  EXPECT_EQ(fixture.original_second(),
      fixture.blockchain().get_block_id_by_height(2));
  const uint64_t first_preflight_count =
      cryptonote::BlockchainRecoveryHarness::preflight_runs(fixture.blockchain());
  verdict = {};
  EXPECT_FALSE(fixture.submit(invalid_first, verdict));
  EXPECT_EQ(first_preflight_count,
      cryptonote::BlockchainRecoveryHarness::preflight_runs(fixture.blockchain()));
}

TEST(epose_blockchain_recovery, seeded_apply_failure_restores_original_chain)
{
  seeded_recovery_chain fixture;
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  const auto unchanged = [](cryptonote::block &) { return true; };
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      unchanged, overmint_coinbase));
  const crypto::hash original_tip = fixture.blockchain().get_tail_id();
  const crypto::hash original_state = fixture.blockchain().get_epose_state_hash();
  cryptonote::block alternative_third{};
  ASSERT_TRUE(fixture.branch_tip(alternative_second, alternative_third));
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(fixture.submit(alternative_third, verdict));
  EXPECT_TRUE(verdict.m_verifivation_failed);
  EXPECT_EQ(original_tip, fixture.blockchain().get_tail_id());
  EXPECT_EQ(original_state, fixture.blockchain().get_epose_state_hash());
  EXPECT_EQ(fixture.original_first(),
      fixture.blockchain().get_block_id_by_height(1));
  EXPECT_EQ(fixture.original_second(),
      fixture.blockchain().get_block_id_by_height(2));
  EXPECT_EQ(3u, fixture.blockchain().get_current_blockchain_height());
}

TEST(epose_blockchain_recovery, seeded_retry_budget_is_per_alternative_tip)
{
  seeded_recovery_chain fixture;
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  const auto unchanged = [](cryptonote::block &) { return true; };
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      unchanged));
  cryptonote::block first_tip{};
  ASSERT_TRUE(fixture.branch_tip(alternative_second, first_tip));
  const crypto::hash first_id = cryptonote::get_block_hash(first_tip);
  cryptonote::BlockchainRecoveryHarness::fail_all_rebuilds(
      fixture.blockchain());
  cryptonote::block_verification_context verdict{};
  EXPECT_FALSE(fixture.submit(first_tip, verdict));
  EXPECT_TRUE(verdict.m_retryable_busy);
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  for (size_t attempt = 0; attempt < 60; ++attempt)
  {
    ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
    if (diagnostics.retry_exhausted)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  ASSERT_TRUE(diagnostics.retry_exhausted);
  EXPECT_EQ(first_id, diagnostics.retry_exhausted_tip);
  EXPECT_EQ(3u, cryptonote::BlockchainRecoveryHarness::retries_for(
      fixture.blockchain(), first_id));

  cryptonote::block second_tip = first_tip;
  ++second_tip.nonce;
  second_tip.invalidate_hashes();
  const crypto::hash second_id = cryptonote::get_block_hash(second_tip);
  ASSERT_NE(first_id, second_id);
  verdict = {};
  EXPECT_FALSE(fixture.submit(second_tip, verdict));
  EXPECT_TRUE(verdict.m_retryable_busy);
  EXPECT_EQ(0u, cryptonote::BlockchainRecoveryHarness::retries_for(
      fixture.blockchain(), second_id));
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::recovery_scheduled(
      fixture.blockchain()));
}

TEST(epose_blockchain_recovery, seeded_reorganization_refreshes_cached_state_once)
{
  seeded_recovery_chain fixture;
  ASSERT_TRUE(fixture.init());
  cryptonote::block alternative_first{};
  cryptonote::block alternative_second{};
  const auto unchanged = [](cryptonote::block &) { return true; };
  ASSERT_TRUE(fixture.seed_branch(alternative_first, alternative_second,
      unchanged));
  qwertycoin::epose::state_storage_diagnostics_v2 diagnostics{};
  ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
  const uint64_t before = cryptonote::BlockchainRecoveryHarness::state_hash_checks(
      fixture.blockchain());
  for (size_t index = 0; index < 8; ++index)
    ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_EQ(before, cryptonote::BlockchainRecoveryHarness::state_hash_checks(
      fixture.blockchain()));

  cryptonote::block alternative_third{};
  ASSERT_TRUE(fixture.branch_tip(alternative_second, alternative_third));
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(alternative_third, verdict));
  ASSERT_TRUE(verdict.m_added_to_main_chain);
  ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
  const uint64_t after = cryptonote::BlockchainRecoveryHarness::state_hash_checks(
      fixture.blockchain());
  EXPECT_GE(after, 1u);
  for (size_t index = 0; index < 8; ++index)
    ASSERT_TRUE(fixture.blockchain().get_epose_state_diagnostics_v2(diagnostics));
  EXPECT_EQ(after, cryptonote::BlockchainRecoveryHarness::state_hash_checks(
      fixture.blockchain()));
}

TEST(epose_blockchain_recovery, seeded_busy_intake_arms_recovery_timer)
{
  seeded_recovery_chain fixture;
  ASSERT_TRUE(fixture.init());
  cryptonote::block first{};
  ASSERT_TRUE(fixture.template_block(first));
  cryptonote::block_verification_context verdict{};
  ASSERT_TRUE(fixture.submit(first, verdict));
  cryptonote::block next{};
  ASSERT_TRUE(fixture.template_block(next));
  ASSERT_TRUE(fixture.pop_db_tip());
  verdict = {};
  EXPECT_FALSE(fixture.submit(next, verdict));
  EXPECT_TRUE(verdict.m_retryable_busy);
  EXPECT_FALSE(verdict.m_verifivation_failed);
  EXPECT_TRUE(cryptonote::BlockchainRecoveryHarness::recovery_scheduled(
      fixture.blockchain()));
}

TEST(epose_blockchain_recovery, reward_preview_requires_aligned_next_height)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure_preview(chain.bc);
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 720);
  qwertycoin::epose::coordinator_reward_plan_v2 preview{};
  EXPECT_FALSE(chain.bc.preview_epose_reward_v2(721, preview));
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  EXPECT_FALSE(chain.bc.preview_epose_reward_v2(720, preview));
  EXPECT_FALSE(chain.bc.preview_epose_reward_v2(722, preview));
  ASSERT_TRUE(chain.bc.preview_epose_reward_v2(721, preview));
  EXPECT_FALSE(preview.has_service_payee);
}

TEST(epose_blockchain_recovery, qualified_reward_preview_matches_template_plan)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainRecoveryHarness::configure_preview(chain.bc);
  ASSERT_NO_FATAL_FAILURE(
      cryptonote::BlockchainRecoveryHarness::settle_qualified_payee(chain.bc));
  cryptonote::BlockchainRecoveryHarness::set_height(chain.bc, 1439);
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::align(chain.bc));
  qwertycoin::epose::coordinator_reward_plan_v2 preview{};
  qwertycoin::epose::coordinator_reward_plan_v2 expected{};
  ASSERT_TRUE(chain.bc.preview_epose_reward_v2(1440, preview));
  cryptonote::account_base miner;
  miner.generate();
  ASSERT_TRUE(cryptonote::BlockchainRecoveryHarness::template_plan(chain.bc,
      miner.get_keys().m_account_address, expected));
  ASSERT_TRUE(preview.has_service_payee);
  ASSERT_TRUE(expected.has_service_payee);
  EXPECT_EQ(expected.expectation.payee_service_public_key,
      preview.expectation.payee_service_public_key);
  EXPECT_EQ(expected.expectation.reward_address.m_view_public_key,
      preview.expectation.reward_address.m_view_public_key);
  EXPECT_EQ(expected.expectation.reward_address.m_spend_public_key,
      preview.expectation.reward_address.m_spend_public_key);
  EXPECT_EQ(expected.allocation.service_reward, preview.allocation.service_reward);
}
