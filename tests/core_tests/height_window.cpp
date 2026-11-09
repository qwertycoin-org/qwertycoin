
#include "chaingen.h"
#include "height_window.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/height_window.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/cryptonote_tx_utils.h"

using namespace cryptonote;

namespace
{
  const cryptonote::transaction* last_tx(const std::vector<test_event_entry>& events, size_t ev_index)
  {
    const cryptonote::transaction *tx = nullptr;
    for (size_t i = 0; i < ev_index && i < events.size(); ++i)
      if (events[i].type() == typeid(std::vector<cryptonote::transaction>))
        tx = &boost::get<std::vector<cryptonote::transaction>>(events[i]).front();
    return tx;
  }

  const cryptonote::block* last_block(const std::vector<test_event_entry>& events, size_t ev_index)
  {
    const cryptonote::block *blk = nullptr;
    for (size_t i = 0; i < ev_index && i < events.size(); ++i)
      if (events[i].type() == typeid(cryptonote::block))
        blk = &boost::get<cryptonote::block>(events[i]);
    return blk;
  }

  std::vector<const cryptonote::transaction*> all_txs(const std::vector<test_event_entry>& events)
  {
    std::vector<const cryptonote::transaction*> txs;
    for (const test_event_entry &ev : events)
      if (ev.type() == typeid(std::vector<cryptonote::transaction>))
        txs.push_back(&boost::get<std::vector<cryptonote::transaction>>(ev).front());
    return txs;
  }

  bool submit_tx(cryptonote::core& c, const cryptonote::transaction& tx, cryptonote::relay_method relay, cryptonote::tx_verification_context& tvc)
  {
    tvc = cryptonote::tx_verification_context{};
    return c.handle_incoming_tx(t_serializable_object_to_blob(tx), tvc, relay, false);
  }
}

bool gen_height_window_base::generate_ring_chain(std::vector<test_event_entry>& events, ring_chain& chain) const
{
  GENERATE_ACCOUNT(miner_account);
  test_generator& generator = chain.generator;
  cryptonote::block blk_0;
  generator.construct_block(blk_0, miner_account, 1338224400);
  events.push_back(blk_0);

  cryptonote::account_base miner_accounts[ring_size];
  cryptonote::block blocks[ring_size + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW];
  const cryptonote::block *prev_block = &blk_0;
  const int fork2_params = test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_timestamp | test_generator::bf_hf_version;
  for (size_t n = 0; n < ring_size + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW; ++n)
  {
    const cryptonote::account_base& miner = n < ring_size ? miner_accounts[n] : miner_account;
    if (n < ring_size)
      miner_accounts[n].generate();
    CHECK_AND_ASSERT_MES(generator.construct_block_manually(blocks[n], *prev_block, miner, fork2_params,
        2, 2, prev_block->timestamp + DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN * 2,
        crypto::hash(), 0, transaction(), std::vector<crypto::hash>(), 0, 0, 2),
        false, "Failed to generate block");
    events.push_back(blocks[n]);
    prev_block = blocks + n;
  }

  uint64_t spent_amount = 0;
  for (const tx_out& candidate : blocks[0].miner_tx.vout)
  {
    bool in_every_ring_block = true;
    for (size_t n = 0; n < ring_size; ++n)
    {
      bool found = false;
      for (const tx_out& out : blocks[n].miner_tx.vout)
        found = found || out.amount == candidate.amount;
      in_every_ring_block = in_every_ring_block && found;
    }
    if (in_every_ring_block && candidate.amount > spent_amount)
      spent_amount = candidate.amount;
  }
  CHECK_AND_ASSERT_MES(spent_amount != 0, false, "no amount shared by every ring block");

  uint64_t outputs_of_amount_before_ring = 0;
  for (const tx_out& out : blk_0.miner_tx.vout)
    outputs_of_amount_before_ring += out.amount == spent_amount ? 1 : 0;

  chain.spent_amount = spent_amount;
  chain.outputs_before_ring = outputs_of_amount_before_ring;
  chain.ring_blocks.assign(blocks, blocks + ring_size);
  chain.ring_miners.assign(miner_accounts, miner_accounts + ring_size);
  chain.tip = *prev_block;
  chain.miner = miner_account;
  return true;
}

tx_source_entry gen_height_window_base::ring_source(const ring_chain& chain, size_t real_block) const
{
  tx_source_entry source;
  source.amount = chain.spent_amount;
  for (size_t n = 0; n < chain.ring_blocks.size(); ++n)
  {
    const transaction& miner_tx = chain.ring_blocks[n].miner_tx;
    size_t out_index_in_tx = find_output_index_by_amount(miner_tx, chain.spent_amount);
    if (out_index_in_tx == miner_tx.vout.size())
      throw std::runtime_error("ring output amount wasn't found");
    source.push_output(chain.outputs_before_ring + n, boost::get<txout_to_key>(miner_tx.vout[out_index_in_tx].target).key, chain.spent_amount);
    if (n == real_block)
      source.real_output_in_tx_index = out_index_in_tx;
  }
  source.real_out_tx_key = cryptonote::get_tx_pub_key_from_extra(chain.ring_blocks[real_block].miner_tx);
  source.real_output = real_block;
  source.mask = rct::identity();
  source.rct = false;
  return source;
}

bool gen_height_window_base::construct_window_tx(const ring_chain& chain, const std::vector<uint8_t>& extra, cryptonote::transaction& tx, size_t spender, const std::vector<crypto::public_key>& external_contract_signers) const
{
  const cryptonote::account_base& sender = chain.ring_miners[spender];
  std::vector<tx_source_entry> sources{ring_source(chain, spender)};
  std::vector<tx_destination_entry> destinations;
  tx_destination_entry destination;
  destination.addr = sender.get_keys().m_account_address;
  destination.amount = 5000;
  destinations.push_back(destination);
  destinations.push_back(destination);

  crypto::secret_key tx_key;
  std::vector<crypto::secret_key> additional_tx_keys;
  std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
  subaddresses[sender.get_keys().m_account_address.m_spend_public_key] = {0, 0};
  const rct::RCTConfig rct_config = { rct::RangeProofPaddedBulletproof, 4 };
  CHECK_AND_ASSERT_MES(construct_tx_and_get_tx_key(sender.get_keys(), subaddresses, sources, destinations, boost::none, extra, tx, tx_key, additional_tx_keys, FAKECHAIN, true, rct_config, true, external_contract_signers),
      false, "failed to construct transaction");
  return true;
}

bool gen_height_window_base::construct_window_tx(const ring_chain& chain, uint64_t from, uint64_t until, cryptonote::transaction& tx) const
{
  std::vector<uint8_t> extra;
  CHECK_AND_ASSERT_MES(add_height_window_to_tx_extra(extra, {from, until}), false, "failed to add window");
  return construct_window_tx(chain, extra, tx);
}

bool gen_height_window_base::construct_next_block(ring_chain& chain, const cryptonote::block& prev, const cryptonote::account_base& miner, uint8_t hf_version,
    const std::vector<cryptonote::transaction>& txs, cryptonote::block& blk, const cryptonote::blobdata& chainlock) const
{
  std::vector<crypto::hash> tx_hashes;
  uint64_t fees = 0;
  for (const cryptonote::transaction& tx : txs)
  {
    uint64_t fee = 0;
    CHECK_AND_ASSERT_MES(get_tx_fee(tx, fee), false, "failed to read fee");
    fees += fee;
    tx_hashes.push_back(get_transaction_hash(tx));
  }
  CHECK_AND_ASSERT_MES(chain.generator.construct_block_manually(blk, prev, miner,
      test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_timestamp | test_generator::bf_tx_hashes | test_generator::bf_hf_version | test_generator::bf_max_outs | test_generator::bf_tx_fees,
      hf_version, hf_version, prev.timestamp + DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN * 2,
      crypto::hash(), 0, transaction(), tx_hashes, 0, 6, hf_version, fees, std::list<transaction>(txs.begin(), txs.end()), chainlock),
      false, "Failed to generate block");
  return true;
}

bool gen_height_window_base::generate_case_with_window(std::vector<test_event_entry>& events, uint8_t hf_version,
    uint64_t from, uint64_t until, bool tx_valid, bool keep_by_block, bool block_valid) const
{
  std::vector<uint8_t> extra;
  CHECK_AND_ASSERT_MES(add_height_window_to_tx_extra(extra, {from, until}), false, "failed to add window");
  return generate_case(events, hf_version, extra, tx_valid, keep_by_block, block_valid);
}

bool gen_height_window_base::generate_case(std::vector<test_event_entry>& events, uint8_t hf_version, const std::vector<uint8_t>& extra,
    bool tx_valid, bool keep_by_block, bool block_valid) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, extra, tx), false, "failed to construct transaction");

  if (keep_by_block)
    SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block);
  if (!tx_valid)
    DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{tx});
  if (keep_by_block)
    SET_EVENT_VISITOR_SETT(events, 0);

  cryptonote::block blk_txes;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, hf_version, {tx}, blk_txes), false, "failed to construct block");
  if (!block_valid)
    DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(blk_txes);

  if (!tx_valid && !block_valid)
    DO_CALLBACK(events, "check_block_rejected_with_supplement");
  if (tx_valid && block_valid)
    DO_CALLBACK(events, "check_unlock");
  return true;
}

bool gen_height_window_base::check_unlock(cryptonote::core& c, size_t, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_base::check_unlock");
  const cryptonote::transaction *tx = last_tx(events, events.size());
  CHECK_TEST_CONDITION(tx != nullptr);
  const crypto::hash h = get_transaction_hash(*tx);
  const BlockchainDB &db = c.get_blockchain_storage().get_db();
  const uint64_t expected_unlock = tx_effective_unlock_time(*tx, mined_height);
  CHECK_EQ(expected_unlock, db.get_tx_unlock_time(h));
  uint64_t tx_id = 0;
  CHECK_TEST_CONDITION(db.tx_exists(h, tx_id));
  const std::vector<std::vector<uint64_t>> per_tx = db.get_tx_amount_output_indices(tx_id, 1);
  CHECK_TEST_CONDITION(per_tx.size() == 1);
  const std::vector<uint64_t> &out_indices = per_tx.front();
  CHECK_TEST_CONDITION(!out_indices.empty());
  for (uint64_t gi : out_indices)
    CHECK_EQ(expected_unlock, db.get_output_key(0, gi, false).unlock_time);
  return true;
}

bool gen_height_window_base::check_block_rejected_with_supplement(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_base::check_block_rejected_with_supplement");
  const cryptonote::transaction *tx = last_tx(events, ev_index);
  const cryptonote::block *blk = last_block(events, ev_index);
  CHECK_TEST_CONDITION(tx != nullptr && blk != nullptr);
  const uint64_t height_before = c.get_blockchain_storage().get_current_blockchain_height();

  cryptonote::block_verification_context bvc{};
  CHECK_TEST_CONDITION(handle_block_with_supplement(c, *blk, *tx, false, bvc));

  CHECK_TEST_CONDITION(bvc.m_verifivation_failed || bvc.m_already_exists); // QWC-3
  CHECK_TEST_CONDITION(!bvc.m_missing_txs);
  CHECK_EQ(height_before, c.get_blockchain_storage().get_current_blockchain_height());
  return true;
}

bool gen_height_window_base::handle_block_with_supplement(cryptonote::core& c, const cryptonote::block& blk,
    const cryptonote::transaction& tx, bool preverified, cryptonote::block_verification_context& bvc)
{
  cryptonote::pool_supplement supplement;
  supplement.txs_by_txid[get_transaction_hash(tx)] = {tx, t_serializable_object_to_blob(tx)};
  if (preverified)
    supplement.nic_verified_hf_version = blk.major_version;
  const cryptonote::blobdata blob = t_serializable_object_to_blob(blk);
  cryptonote::block_complete_entry entry;
  entry.pruned = false;
  entry.block = blob;
  std::vector<cryptonote::block> parsed;
  if (!c.prepare_handle_incoming_blocks({entry}, parsed))
    return false;
  c.handle_incoming_block(blob, &blk, bvc, supplement);
  c.cleanup_handle_incoming_blocks();
  return true;
}

bool gen_height_window_base::resubmit_last_tx(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events, cryptonote::tx_verification_context& tvc) const
{
  const cryptonote::transaction *tx = last_tx(events, ev_index);
  if (tx == nullptr)
    return false;
  submit_tx(c, *tx, cryptonote::relay_method::fluff, tvc);
  return !c.pool_has_tx(get_transaction_hash(*tx));
}

bool gen_height_window_base::check_pool_rejects_without_drop_offense(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_base::check_pool_rejects_without_drop_offense");
  cryptonote::tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed);
  CHECK_TEST_CONDITION(tvc.m_no_drop_offense);
  return true;
}

bool gen_height_window_base::check_last_tx_in_pool(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_base::check_last_tx_in_pool");
  const cryptonote::transaction *tx = last_tx(events, ev_index);
  CHECK_TEST_CONDITION(tx != nullptr);
  CHECK_TEST_CONDITION(c.pool_has_tx(get_transaction_hash(*tx)));
  return true;
}

bool gen_height_window_base::check_last_tx_not_in_pool(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_base::check_last_tx_not_in_pool");
  const cryptonote::transaction *tx = last_tx(events, ev_index);
  CHECK_TEST_CONDITION(tx != nullptr);
  CHECK_TEST_CONDITION(!c.pool_has_tx(get_transaction_hash(*tx)));
  return true;
}

bool gen_height_window_absent_ok::generate(std::vector<test_event_entry>& events) const
{ return generate_case(events, fork_version, std::vector<uint8_t>(), true, false, true); }

bool gen_height_window_in_range_ok::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height, mined_height + 7, true, false, true); }

bool gen_height_window_far_until_unlock_unchanged::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height, mined_height + 100, true, false, true); }

bool gen_height_window_until_boundary_ok::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height - 13, mined_height, true, false, true); }

bool gen_height_window_from_boundary_ok::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height, mined_height + 50, true, false, true); }

bool gen_height_window_until_plus_one_rejected_in_block::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height - 13, mined_height - 1, false, true, false); }

bool gen_height_window_expired_rejected_by_pool::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height - 13, mined_height - 1, false, false, false); }

bool gen_height_window_from_after_block_rejected_in_block::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height + 1, mined_height + 100, true, true, false); }

bool gen_height_window_pool_lead_boundary::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height + 10, mined_height + 100, true, false, false); }

bool gen_height_window_pool_too_early::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, fork_version, mined_height + 11, mined_height + 100, false, false, false); }

bool gen_height_window_before_fork_rejected::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, mined_height, mined_height + 7, tx), false, "failed to construct transaction");
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{tx});
  DO_CALLBACK(events, "check_pool_rejects_without_drop_offense");
  return true;
}

bool gen_height_window_before_fork_block_rejected::generate(std::vector<test_event_entry>& events) const
{ return generate_case_with_window(events, HF_VERSION_QWC_EPOSE, mined_height, mined_height + 7, false, true, false); }

bool gen_height_window_duplicate_rejected::generate(std::vector<test_event_entry>& events) const
{
  std::vector<uint8_t> extra;
  const tx_extra_height_window window{mined_height, mined_height + 7};
  CHECK_AND_ASSERT_MES(add_height_window_to_tx_extra(extra, window), false, "failed to add window");
  CHECK_AND_ASSERT_MES(add_height_window_to_tx_extra(extra, window), false, "failed to add window");
  return generate_case(events, fork_version, extra, false, false, false);
}

bool gen_height_window_timestamp_unlock_rejected::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, mined_height, mined_height + 7, tx), false, "failed to construct transaction");
  tx.unlock_time = CRYPTONOTE_MAX_BLOCK_NUMBER;
  tx.invalidate_hashes();

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{tx});
  SET_EVENT_VISITOR_SETT(events, 0);

  cryptonote::block blk;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {tx}, blk), false, "failed to construct block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(blk);
  DO_CALLBACK(events, "check_block_rejected_with_supplement");
  return true;
}

bool gen_height_window_pool_drops_on_expiry::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, mined_height - 13, mined_height + 1, tx), false, "failed to construct transaction");
  events.push_back(std::vector<transaction>{tx});

  cryptonote::block at_mined, at_until;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {}, at_mined), false, "block");
  events.push_back(at_mined);
  DO_CALLBACK(events, "check_last_tx_in_pool");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, at_mined, chain.miner, fork_version, {}, at_until), false, "block");
  events.push_back(at_until);
  DO_CALLBACK(events, "check_last_tx_not_in_pool");
  return true;
}

bool gen_height_window_reorg::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, mined_height - 13, mined_height + 5, tx), false, "failed to construct transaction");
  transaction expired_on_b;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, mined_height - 13, mined_height + 2, expired_on_b), false, "failed to construct transaction");
  events.push_back(std::vector<transaction>{tx});

  cryptonote::account_base miner_b;
  miner_b.generate();
  cryptonote::block a[4], b[3];
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {tx}, a[0]), false, "a0");
  events.push_back(a[0]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a[0], chain.miner, fork_version, {}, a[1]), false, "a1");
  events.push_back(a[1]);

  const cryptonote::block *prev = &chain.tip;
  for (cryptonote::block &blk : b)
  {
    CHECK_AND_ASSERT_MES(construct_next_block(chain, *prev, miner_b, fork_version, {}, blk), false, "b");
    events.push_back(blk);
    prev = &blk;
  }

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{expired_on_b});
  SET_EVENT_VISITOR_SETT(events, 0);
  DO_CALLBACK(events, "check_on_b");

  prev = &a[1];
  for (size_t n = 2; n < 4; ++n)
  {
    CHECK_AND_ASSERT_MES(construct_next_block(chain, *prev, chain.miner, fork_version, {}, a[n]), false, "a");
    events.push_back(a[n]);
    prev = &a[n];
  }
  DO_CALLBACK(events, "check_back_on_a");
  return true;
}

bool gen_height_window_reorg::check_on_b(cryptonote::core& c, size_t, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_reorg::check_on_b");
  const std::vector<const cryptonote::transaction*> txs = all_txs(events);
  CHECK_TEST_CONDITION(txs.size() == 2);
  const crypto::hash tx_hash = get_transaction_hash(*txs[0]);
  CHECK_EQ(mined_height + 3, c.get_blockchain_storage().get_current_blockchain_height());
  CHECK_TEST_CONDITION(!c.get_blockchain_storage().get_db().tx_exists(tx_hash));
  CHECK_TEST_CONDITION(c.pool_has_tx(tx_hash));

  cryptonote::tx_verification_context tvc;
  submit_tx(c, *txs[1], cryptonote::relay_method::block, tvc);
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed);
  CHECK_TEST_CONDITION(tvc.m_window_expired);
  CHECK_TEST_CONDITION(tvc.m_no_drop_offense);
  CHECK_TEST_CONDITION(!c.pool_has_tx(get_transaction_hash(*txs[1])));
  CHECK_TEST_CONDITION(c.pool_has_expired_window_tx(get_transaction_hash(*txs[1])));
  return true;
}

bool gen_height_window_reorg::check_back_on_a(cryptonote::core& c, size_t, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_reorg::check_back_on_a");
  const std::vector<const cryptonote::transaction*> txs = all_txs(events);
  CHECK_TEST_CONDITION(txs.size() == 2);
  const crypto::hash tx_hash = get_transaction_hash(*txs[0]);
  const BlockchainDB &db = c.get_blockchain_storage().get_db();
  CHECK_EQ(mined_height + 4, c.get_blockchain_storage().get_current_blockchain_height());
  CHECK_TEST_CONDITION(db.tx_exists(tx_hash));
  CHECK_EQ(mined_height, db.get_tx_block_height(tx_hash));
  CHECK_EQ(tx_effective_unlock_time(*txs[0], mined_height), db.get_tx_unlock_time(tx_hash));
  CHECK_TEST_CONDITION(!c.pool_has_tx(tx_hash));
  CHECK_TEST_CONDITION(!c.pool_has_tx(get_transaction_hash(*txs[1])));
  return true;
}

bool gen_height_window_reorg_after_expiry::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, mined_height - 13, mined_height + 1, tx), false, "failed to construct transaction");
  events.push_back(std::vector<transaction>{tx});

  cryptonote::account_base miner_b;
  miner_b.generate();
  cryptonote::block a[2], b[3];
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {}, a[0]), false, "a0");
  events.push_back(a[0]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a[0], chain.miner, fork_version, {}, a[1]), false, "a1");
  events.push_back(a[1]);
  DO_CALLBACK(events, "check_dropped_into_cache");

  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, miner_b, fork_version, {tx}, b[0]), false, "b0");
  events.push_back(b[0]);
  const cryptonote::block *prev = &b[0];
  for (size_t n = 1; n < 3; ++n)
  {
    CHECK_AND_ASSERT_MES(construct_next_block(chain, *prev, miner_b, fork_version, {}, b[n]), false, "b");
    events.push_back(b[n]);
    prev = &b[n];
  }
  DO_CALLBACK(events, "check_switched_with_cached_tx");
  return true;
}

bool gen_height_window_reorg_after_expiry::check_dropped_into_cache(cryptonote::core& c, size_t, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_reorg_after_expiry::check_dropped_into_cache");
  const std::vector<const cryptonote::transaction*> txs = all_txs(events);
  CHECK_TEST_CONDITION(txs.size() == 1);
  const crypto::hash tx_hash = get_transaction_hash(*txs[0]);
  CHECK_EQ(mined_height + 2, c.get_blockchain_storage().get_current_blockchain_height());
  CHECK_TEST_CONDITION(!c.pool_has_tx(tx_hash));
  CHECK_TEST_CONDITION(c.pool_has_expired_window_tx(tx_hash));
  return true;
}

bool gen_height_window_reorg_after_expiry::check_switched_with_cached_tx(cryptonote::core& c, size_t, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_height_window_reorg_after_expiry::check_switched_with_cached_tx");
  const std::vector<const cryptonote::transaction*> txs = all_txs(events);
  CHECK_TEST_CONDITION(txs.size() == 1);
  const crypto::hash tx_hash = get_transaction_hash(*txs[0]);
  const BlockchainDB &db = c.get_blockchain_storage().get_db();
  CHECK_EQ(mined_height + 3, c.get_blockchain_storage().get_current_blockchain_height());
  CHECK_TEST_CONDITION(db.tx_exists(tx_hash));
  CHECK_EQ(mined_height, db.get_tx_block_height(tx_hash));
  CHECK_TEST_CONDITION(!c.pool_has_tx(tx_hash));
  CHECK_TEST_CONDITION(!c.pool_has_expired_window_tx(tx_hash));
  return true;
}

bool gen_height_window_coinbase_rejected::generate(std::vector<test_event_entry>& events) const
{
  uint64_t ts_start = 1338224400;
  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK_HF(events, blk_1, blk_0, miner_account, 2);
  MAKE_NEXT_BLOCK_CONTRACTS(events, blk_2, blk_1, miner_account);

  cryptonote::block blk_3;
  CHECK_AND_ASSERT_MES(generator.construct_block(blk_3, blk_2, miner_account, std::list<cryptonote::transaction>(), (uint8_t)HF_VERSION_QWC_CONTRACTS), false, "construct_block failed");
  CHECK_AND_ASSERT_MES(add_height_window_to_tx_extra(blk_3.miner_tx.extra, {3, 3}), false, "failed to add window");
  blk_3.miner_tx.invalidate_hashes();
  generator.fill_nonce(blk_3, get_test_difficulty(boost::optional<uint8_t>((uint8_t)HF_VERSION_QWC_CONTRACTS)), 3);
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(blk_3);
  return true;
}
