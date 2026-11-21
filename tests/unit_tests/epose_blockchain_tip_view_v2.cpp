// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// The published EPoSE tip view against a real Blockchain on LMDB, FAKECHAIN
// at QWC-HF17 with EPoSE-v2 parameters. Each step that moves the tip (init,
// add, pop, alternative switch, failed switch and its rollback, re-init on
// the stored chain) must leave a view that names the chain's tip and state.
// A coordinator left inconsistent by a database fault publishes no view and
// is replayed at the next tip change.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

#include <boost/filesystem.hpp>

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_core/tx_pool.h"
#include "epose/coordinator_v2.h"
#include "epose/service_producer_v2.h"
#include "epose/tip_view_v2.h"
#include "epose_fakechain_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  struct chain_and_pool
  {
    cryptonote::tx_memory_pool txpool;
    cryptonote::Blockchain bc;
    chain_and_pool() : txpool(bc), bc(txpool) {}
    ~chain_and_pool() { bc.deinit(); }
  };

  class blockchain_tip_view : public ::testing::Test
  {
  protected:
    void SetUp() override
    {
      directory_ = epose_fakechain_v2::temp_directory("qwc-epose-tip-view-%%%%-%%%%");
      parameters_ = epose_fakechain_v2::parameters();
      ASSERT_TRUE(parameters_.valid());
      miner_a_.generate();
      miner_b_.generate();
      open();
    }

    void TearDown() override
    {
      chain_.reset();
      boost::system::error_code ignored;
      boost::filesystem::remove_all(directory_, ignored);
    }

    void open()
    {
      chain_.reset();
      chain_.reset(new chain_and_pool());
      db_ = new epose_fakechain_v2::faulty_lmdb();
      ASSERT_TRUE(epose_fakechain_v2::init(chain_->bc, directory_, parameters_, db_));
    }

    cryptonote::Blockchain &bc() { return chain_->bc; }

    // The view must name the chain's tip, height and EPoSE-v2 state.
    void expect_view_is_tip(const char *step)
    {
      SCOPED_TRACE(step);
      const auto view = bc().get_epose_tip_view();
      ASSERT_NE(nullptr, view);
      EXPECT_EQ(bc().get_tail_id(), view->tip_hash);
      EXPECT_EQ(bc().get_current_blockchain_height(), view->height);
      ASSERT_TRUE(view->v2);
      ASSERT_NE(nullptr, view->state);
      EXPECT_EQ(bc().get_epose_state_hash(), view->state_hash);
      EXPECT_EQ(view->state->state_hash(), view->state_hash);
    }

    // No view, and nothing answers from the coordinator's state.
    void expect_inconsistent(const char *step)
    {
      SCOPED_TRACE(step);
      EXPECT_EQ(nullptr, bc().get_epose_tip_view());
      EXPECT_EQ(crypto::null_hash, bc().get_epose_state_hash());
      cryptonote::block b{};
      cryptonote::difficulty_type difficulty = 0;
      uint64_t height = 0, reward = 0, seed_height = 0;
      crypto::hash seed_hash{};
      EXPECT_FALSE(bc().create_block_template(b, miner_a_.get_keys().m_account_address,
          difficulty, height, reward, cryptonote::blobdata(), seed_height, seed_hash));
    }

    // Pops two blocks past a one-block undo window while stored commitments
    // cannot be read, so the replay the second pop needs fails. Returns the
    // popped blocks, oldest first.
    std::vector<cryptonote::block> trip_failed_replay()
    {
      parameters_.limits.max_recent_undo_blocks = 1;
      open();
      for (int i = 0; i < 4; ++i)
        mine(miner_a_, "add before failed replay");
      const std::vector<cryptonote::block> popped = top(2);
      db_->fail_commitment_reads = true;
      bc().pop_blocks(2);
      EXPECT_EQ(popped.front().prev_id, bc().get_tail_id());
      expect_inconsistent("replay failed");
      db_->fail_commitment_reads = false;
      return popped;
    }

    void add(const cryptonote::block &b, bool main_chain, const char *step)
    {
      SCOPED_TRACE(step);
      cryptonote::block_verification_context bvc{};
      ASSERT_TRUE(epose_fakechain_v2::add_block(bc(), b, bvc));
      ASSERT_FALSE(bvc.m_verifivation_failed);
      EXPECT_EQ(main_chain, bvc.m_added_to_main_chain);
    }

    cryptonote::block mine(const cryptonote::account_base &miner, const char *step)
    {
      const cryptonote::block b = epose_fakechain_v2::template_on_tip(bc(), miner);
      add(b, true, step);
      return b;
    }

    // The last `count` main-chain blocks, oldest first.
    std::vector<cryptonote::block> top(size_t count)
    {
      std::vector<cryptonote::block> out;
      const uint64_t height = bc().get_current_blockchain_height();
      for (uint64_t h = height - count; h < height; ++h)
        out.push_back(bc().get_db().get_block_from_height(h));
      return out;
    }

    boost::filesystem::path directory_;
    consensus_parameters_v2 parameters_{};
    cryptonote::account_base miner_a_;
    cryptonote::account_base miner_b_;
    std::unique_ptr<chain_and_pool> chain_;
    epose_fakechain_v2::faulty_lmdb *db_ = nullptr;
  };
}

TEST_F(blockchain_tip_view, every_tip_change_publishes)
{
  expect_view_is_tip("init, genesis");
  ASSERT_EQ(1u, bc().get_current_blockchain_height());

  for (int i = 0; i < 6; ++i)
  {
    mine(miner_a_, "main-chain add");
    expect_view_is_tip("main-chain add");
  }

  // Pop, then put the same blocks back.
  const std::vector<cryptonote::block> popped = top(2);
  bc().pop_blocks(1);
  expect_view_is_tip("pop 1");
  bc().pop_blocks(1);
  expect_view_is_tip("pop 2");
  for (const auto &b : popped)
  {
    add(b, true, "re-add popped");
    expect_view_is_tip("re-add popped");
  }

  // Alternative switch: two main blocks lose to three on the same parent.
  // EPoSE-v2 templates exist only on the tip, so the branch is mined on the
  // tip and replayed later as alternative blocks.
  const crypto::hash fork_tip = bc().get_tail_id();
  const std::vector<cryptonote::block> main_branch = top(2);
  bc().pop_blocks(2);
  std::vector<cryptonote::block> alternative;
  for (int i = 0; i < 3; ++i)
    alternative.push_back(mine(miner_b_, "mine alternative"));
  bc().pop_blocks(3);
  for (const auto &b : main_branch)
    add(b, true, "restore main branch");
  ASSERT_EQ(fork_tip, bc().get_tail_id());
  add(alternative[0], false, "alternative 1");
  add(alternative[1], false, "alternative 2");
  expect_view_is_tip("alternative blocks below the main chain");
  EXPECT_EQ(fork_tip, bc().get_epose_tip_view()->tip_hash);
  add(alternative[2], true, "alternative 3 switches");
  EXPECT_EQ(cryptonote::get_block_hash(alternative[2]), bc().get_tail_id());
  expect_view_is_tip("switch to alternative");

  // Failed switch: the third block of the new branch over-pays its miner,
  // which only full validation on connect rejects. The switch connects the
  // first two, fails on the third and rolls back to the old branch.
  const crypto::hash before_failed = bc().get_tail_id();
  const uint64_t height_before_failed = bc().get_current_blockchain_height();
  const std::vector<cryptonote::block> kept = top(2);
  bc().pop_blocks(2);
  std::vector<cryptonote::block> failing;
  for (int i = 0; i < 2; ++i)
    failing.push_back(mine(miner_a_, "mine failing branch"));
  cryptonote::block bad = epose_fakechain_v2::template_on_tip(bc(), miner_a_);
  ASSERT_FALSE(bad.miner_tx.vout.empty());
  bad.miner_tx.vout.front().amount += 1;
  bad.miner_tx.invalidate_hashes();
  bad.invalidate_hashes();
  bc().pop_blocks(2);
  for (const auto &b : kept)
    add(b, true, "restore before failed switch");
  ASSERT_EQ(before_failed, bc().get_tail_id());
  add(failing[0], false, "failing 1");
  add(failing[1], false, "failing 2");
  {
    cryptonote::block_verification_context bvc{};
    epose_fakechain_v2::add_block(bc(), bad, bvc);
    EXPECT_FALSE(bvc.m_added_to_main_chain);
  }
  EXPECT_EQ(before_failed, bc().get_tail_id());
  EXPECT_EQ(height_before_failed, bc().get_current_blockchain_height());
  expect_view_is_tip("failed switch rolled back");

  // Re-init on the stored chain publishes the stored tip.
  open();
  EXPECT_EQ(before_failed, bc().get_tail_id());
  expect_view_is_tip("re-init on stored chain");
  mine(miner_b_, "add after re-init");
  expect_view_is_tip("add after re-init");
}

TEST_F(blockchain_tip_view, failed_replay_recovers_at_next_add)
{
  const std::vector<cryptonote::block> popped = trip_failed_replay();
  for (const auto &b : popped)
  {
    add(b, true, "re-add after failed replay");
    expect_view_is_tip("re-add after failed replay");
  }
  for (int i = 0; i < 2; ++i)
  {
    mine(miner_b_, "add after recovery");
    expect_view_is_tip("add after recovery");
  }
}

TEST_F(blockchain_tip_view, failed_replay_recovers_at_next_pop)
{
  trip_failed_replay();
  const cryptonote::block tip = top(1).front();
  bc().pop_blocks(1);
  EXPECT_EQ(tip.prev_id, bc().get_tail_id());
  expect_view_is_tip("pop after failed replay");
  mine(miner_b_, "add after recovery");
  expect_view_is_tip("add after recovery");
}

TEST_F(blockchain_tip_view, aborted_pop_restores_tip_and_state)
{
  for (int i = 0; i < 4; ++i)
    mine(miner_a_, "add before aborted pop");
  const crypto::hash tip = bc().get_tail_id();
  const uint64_t height = bc().get_current_blockchain_height();

  // The second pop throws; the batch abort restores the first.
  db_->pops_before_throw = 1;
  bc().pop_blocks(3);
  EXPECT_EQ(tip, bc().get_tail_id());
  EXPECT_EQ(height, bc().get_current_blockchain_height());
  expect_view_is_tip("aborted pop");
  mine(miner_b_, "add after aborted pop");
  expect_view_is_tip("add after aborted pop");
}

// A rejected block on an inconsistent coordinator: apply replays the
// coordinator before EPoSE-v2 validation rejects the block. The tip does not
// move, so the view must be published by the rejection itself.
TEST_F(blockchain_tip_view, rejected_block_after_replay_publishes)
{
  parameters_.limits.max_recent_undo_blocks = 1;
  open();
  for (int i = 0; i < 2; ++i)
    mine(miner_a_, "add before bad block");
  const crypto::hash parent = bc().get_tail_id();
  // An undecodable EPoSE-v2 envelope in the miner transaction passes miner
  // transaction validation and fails EPoSE-v2 validation.
  cryptonote::block bad = epose_fakechain_v2::template_on_tip(bc(), miner_a_);
  ASSERT_TRUE(cryptonote::add_epose_v2_to_tx_extra(bad.miner_tx.extra, "not-an-envelope"));
  bad.miner_tx.invalidate_hashes();
  bad.invalidate_hashes();
  for (int i = 0; i < 2; ++i)
    mine(miner_a_, "add over bad block's parent");
  db_->fail_commitment_reads = true;
  bc().pop_blocks(2);
  ASSERT_EQ(parent, bc().get_tail_id());
  expect_inconsistent("replay failed");
  db_->fail_commitment_reads = false;

  cryptonote::block_verification_context bvc{};
  epose_fakechain_v2::add_block(bc(), bad, bvc);
  EXPECT_TRUE(bvc.m_verifivation_failed);
  EXPECT_FALSE(bvc.m_added_to_main_chain);
  EXPECT_EQ(parent, bc().get_tail_id());
  expect_view_is_tip("rejected block after replay");
}

// A node that mines the next block itself sees no tip change; the block
// template retries the replay, at most once per heal interval.
TEST_F(blockchain_tip_view, block_template_heals_after_interval)
{
  trip_failed_replay();
  const crypto::hash tip = bc().get_tail_id();
  // Reads work again, but the last failed replay is too recent.
  expect_inconsistent("template inside the heal interval");
  bc().set_epose_template_heal_interval(std::chrono::steady_clock::duration::zero());
  const cryptonote::block b = epose_fakechain_v2::template_on_tip(bc(), miner_a_);
  EXPECT_EQ(tip, b.prev_id);
  EXPECT_EQ(tip, bc().get_tail_id());
  expect_view_is_tip("template healed without a tip change");
  add(b, true, "add healed template");
  expect_view_is_tip("add healed template");
}

TEST_F(blockchain_tip_view, local_enrollment_unknown_while_inconsistent)
{
  const crypto::hash identity = crypto::cn_fast_hash("local identity", 14);
  EXPECT_FALSE(read_local_enrollment_v2(nullptr, identity, 1).known);
  const epose_tip_view stateless{};
  EXPECT_FALSE(read_local_enrollment_v2(&stateless, identity, 1).known);

  mine(miner_a_, "add before enrollment read");
  const uint64_t epoch = bc().get_epose_current_epoch() + 1;
  const auto view = bc().get_epose_tip_view();
  ASSERT_NE(nullptr, view);
  const local_enrollment_v2 known = read_local_enrollment_v2(view.get(), identity, epoch);
  EXPECT_TRUE(known.known);
  // The locked readers answer the same at this tip.
  const auto descriptors = bc().get_epose_identity_descriptors_v2(epoch);
  const bool locked_enrolled = std::any_of(descriptors.begin(), descriptors.end(),
      [&](const identity_descriptor_v2 &d) { return d.identity_id == identity; });
  EXPECT_EQ(locked_enrolled, known.enrolled);
  EXPECT_EQ(bc().has_epose_admission_v2(identity, epoch), known.admitted);

  trip_failed_replay();
  // The locked readers answer "not enrolled" here; the view says unknown.
  EXPECT_TRUE(bc().get_epose_identity_descriptors_v2(epoch).empty());
  EXPECT_FALSE(bc().has_epose_admission_v2(identity, epoch));
  const auto none = bc().get_epose_tip_view();
  EXPECT_FALSE(read_local_enrollment_v2(none.get(), identity, epoch).known);
}

// A batch of two blocks whose second database write throws: the abort drops
// both blocks, and the coordinator, which connected the first, is replayed.
TEST_F(blockchain_tip_view, aborted_batch_replays_coordinator)
{
  for (int i = 0; i < 2; ++i)
    mine(miner_a_, "add before batch");
  const crypto::hash tip = bc().get_tail_id();
  const crypto::hash state = bc().get_epose_state_hash();
  const std::vector<cryptonote::block> batch = {
      mine(miner_a_, "batch block 1"), mine(miner_a_, "batch block 2")};
  bc().pop_blocks(2);
  ASSERT_EQ(tip, bc().get_tail_id());
  ASSERT_EQ(state, bc().get_epose_state_hash());

  // Adds the first `count` blocks in one batch; the database write of the
  // last one throws.
  const auto run_batch = [&](size_t count, bool fail_reads)
  {
    std::vector<cryptonote::block_complete_entry> entries(count);
    for (size_t i = 0; i < count; ++i)
      entries[i].block = cryptonote::block_to_blob(batch[i]);
    std::vector<cryptonote::block> parsed;
    EXPECT_TRUE(bc().prepare_handle_incoming_blocks(entries, parsed));
    for (size_t i = 0; i < count; ++i)
    {
      cryptonote::block_verification_context bvc{};
      if (i + 1 == count)
        db_->adds_before_throw = 0;
      bc().add_new_block(batch[i], bvc);
      EXPECT_EQ(i + 1 < count, bvc.m_added_to_main_chain);
    }
    EXPECT_EQ(-1, db_->adds_before_throw);
    db_->fail_commitment_reads = fail_reads;
    const bool cleaned = bc().cleanup_handle_incoming_blocks(true);
    db_->fail_commitment_reads = false;
    return cleaned;
  };

  EXPECT_TRUE(run_batch(2, false));
  EXPECT_EQ(tip, bc().get_tail_id());
  EXPECT_EQ(state, bc().get_epose_state_hash());
  expect_view_is_tip("aborted batch, replayed");
  add(batch[0], true, "re-add first batch block");
  expect_view_is_tip("re-add first batch block");
  bc().pop_blocks(1);
  ASSERT_EQ(tip, bc().get_tail_id());

  // The replay after the abort fails: cleanup reports it.
  EXPECT_FALSE(run_batch(1, true));
  EXPECT_EQ(tip, bc().get_tail_id());
  expect_inconsistent("aborted batch, replay failed");
}

// Pops on an inconsistent coordinator do not disconnect its stale state;
// only the replay of each pop's publish touches the database.
TEST_F(blockchain_tip_view, inconsistent_pop_skips_disconnect)
{
  parameters_.limits.max_recent_undo_blocks = 1;
  open();
  for (int i = 0; i < 4; ++i)
    mine(miner_a_, "add before aborted pop");
  const crypto::hash tip = bc().get_tail_id();
  db_->fail_commitment_reads = true;
  db_->pops_before_throw = 1;
  bc().pop_blocks(2);
  ASSERT_EQ(tip, bc().get_tail_id());
  expect_inconsistent("aborted pop, replay failed");

  db_->failed_commitment_reads = 0;
  bc().pop_blocks(1);
  bc().pop_blocks(1);
  // One failed replay per pop. A disconnect on the stale coordinator would
  // add a deep-rollback replay at the second pop.
  EXPECT_EQ(2u, db_->failed_commitment_reads);
  db_->fail_commitment_reads = false;
  const cryptonote::block parent = top(1).front();
  bc().pop_blocks(1);
  EXPECT_EQ(parent.prev_id, bc().get_tail_id());
  expect_view_is_tip("pop after reads recover");
}
