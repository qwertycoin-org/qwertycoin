#include "chaingen.h"
#include "contracts_deep_reorg.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "../unit_tests/vm_test_utils.h"
#include "cryptonote_core/blockchain.h"

using namespace cryptonote;

bool gen_contracts_deep_reorg_replay::push_empty_blocks(std::vector<test_event_entry>& events, ring_chain& chain, cryptonote::block& tip,
    const cryptonote::account_base& miner, uint64_t count) const
{
  for (uint64_t n = 0; n < count; ++n)
  {
    cryptonote::block next;
    CHECK_AND_ASSERT_MES(construct_next_block(chain, tip, miner, fork_version, {}, next), false, "empty block");
    events.push_back(next);
    tip = next;
  }
  return true;
}

bool gen_contracts_deep_reorg_replay::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, store_cd, store_xy;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store_cd, 1), false, "store tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "xy"), store_xy, 2), false, "store tx");

  push_kept_txs(events, {deploy, store_cd});
  cryptonote::block main_tip;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy, store_cd}, main_tip), false, "main contract block");
  events.push_back(main_tip);
  CHECK_AND_ASSERT_MES(push_empty_blocks(events, chain, main_tip, chain.miner, reorg_depth - 1), false, "main chain");
  DO_CALLBACK(events, "check_old_undo_expired");

  cryptonote::account_base miner_b;
  miner_b.generate();
  push_kept_txs(events, {store_xy});
  cryptonote::block alt_tip;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, miner_b, fork_version, {deploy}, alt_tip), false, "alt deploy block");
  events.push_back(alt_tip);
  cryptonote::block alt_store;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, alt_tip, miner_b, fork_version, {store_xy}, alt_store), false, "alt store block");
  events.push_back(alt_store);
  alt_tip = alt_store;
  CHECK_AND_ASSERT_MES(push_empty_blocks(events, chain, alt_tip, miner_b, reorg_depth - 1), false, "alt chain");
  DO_CALLBACK(events, "check_replayed_alt_chain");
  return true;
}

bool gen_contracts_deep_reorg_replay::check_old_undo_expired(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_deep_reorg_replay::check_old_undo_expired");
  const BlockchainDB& db = c.get_blockchain_storage().get_db();
  const uint64_t tip_height = db.height() - 1;
  CHECK_EQ(mined_height + reorg_depth - 1, tip_height);
  vm_block_commitment commitment;
  vm::write_set undo;
  CHECK_TEST_CONDITION(db.get_vm_commitment(mined_height, commitment));
  CHECK_TEST_CONDITION(!db.get_vm_undo(mined_height, undo));
  CHECK_TEST_CONDITION(db.get_vm_undo(tip_height, undo));
  return check_stored_cd(c, ev_index, events);
}

bool gen_contracts_deep_reorg_replay::check_replayed_alt_chain(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_deep_reorg_replay::check_replayed_alt_chain");
  CHECK_TEST_CONDITION(ev_index > 0 && events[ev_index - 1].type() == typeid(cryptonote::block));
  const cryptonote::block& alt_tip = boost::get<cryptonote::block>(events[ev_index - 1]);
  const Blockchain& chain = c.get_blockchain_storage();
  CHECK_EQ(get_block_hash(alt_tip), chain.get_tail_id());

  const BlockchainDB& db = chain.get_db();
  const uint64_t tip_height = db.height() - 1;
  CHECK_EQ(mined_height + reorg_depth, tip_height);
  for (uint64_t height = mined_height; height <= tip_height; ++height)
  {
    vm_block_commitment commitment;
    CHECK_TEST_CONDITION(db.get_vm_commitment(height, commitment));
    crypto::hash claimed_root;
    CHECK_EQ(1, get_tx_vm_state_root(db.get_block_from_height(height).miner_tx.extra, claimed_root));
    CHECK_EQ(claimed_root, commitment.root);
  }
  return check_stored_xy(c, ev_index, events);
}
