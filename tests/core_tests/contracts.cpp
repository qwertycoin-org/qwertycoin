#include <algorithm>
#include <future>
#include <thread>

#include "blockchain_test_access.h"
#include "chaingen.h"
#include "contracts.h"
#include "../unit_tests/vm_test_utils.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/miner.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/contract_state.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "device/device.hpp"

using namespace cryptonote;

namespace
{
  bool tip_root(const BlockchainDB& db, crypto::hash& root)
  {
    const cryptonote::block tip = db.get_block_from_height(db.height() - 1);
    return get_tx_vm_state_root(tip.miner_tx.extra, root) == 1;
  }

  bool mine_template(core& c, Blockchain& chain, block& candidate, difficulty_type difficulty, uint64_t height)
  {
    candidate.nonce = 0;
    while (!miner::find_nonce_for_given_block([&chain](const block& b, uint64_t h, const crypto::hash* seed, unsigned int threads, crypto::hash& hash) {
      return get_block_longhash(&chain, b, hash, h, seed, threads);
    }, candidate, difficulty, height, NULL))
      ++candidate.timestamp;
    block_verification_context bvc{};
    pool_supplement no_extra_txs;
    return c.handle_single_incoming_block(block_to_blob(candidate), &candidate, bvc, no_extra_txs, false)
        && !bvc.m_verifivation_failed && bvc.m_added_to_main_chain;
  }

  bool check_zero_output_inputs(core& c, transaction tx, bool accepted)
  {
    uint64_t max_height = 0;
    crypto::hash max_hash{};
    tx_verification_context tvc{};
    Blockchain &chain = c.get_blockchain_storage();
    const bool valid = chain.check_tx_inputs(tx, max_height, max_hash, tvc,
        false, chain.get_current_hard_fork_version());
    return valid == accepted && (accepted || tvc.m_verifivation_failed);
  }
}

const std::string gen_contracts_base::stored_key = "ab";

std::string gen_contracts_base::contract_code()
{
  return vm_test::test_contract();
}

crypto::hash gen_contracts_base::contract_id()
{
  return vm::contract_id_for(contract_code(), {});
}

tx_extra_vm_call gen_contracts_base::deploy_call()
{
  tx_extra_vm_call call{};
  call.kind = tx_extra_vm_call::kind_deploy;
  call.contract_id = crypto::null_hash;
  call.code = contract_code();
  call.gas_limit = QWC_VM_TX_GAS_CAP;
  call.gas_price = 1;
  return call;
}

tx_extra_vm_call gen_contracts_base::invoke_call(uint32_t method, const std::string& args)
{
  tx_extra_vm_call call{};
  call.kind = tx_extra_vm_call::kind_invoke;
  call.contract_id = contract_id();
  call.method = method;
  call.args = args;
  call.gas_limit = QWC_VM_TX_GAS_CAP;
  call.gas_price = 1;
  return call;
}

const transaction* gen_contracts_base::find_contract_tx(const std::vector<test_event_entry>& events, uint32_t method)
{
  for (const test_event_entry& event : events)
  {
    const transaction* tx = boost::get<transaction>(&event);
    if (tx)
    {
      tx_extra_vm_call call{};
      if (get_tx_vm_call(tx->extra, call) == 1 && call.method == method)
        return tx;
    }
    const auto* txs = boost::get<std::vector<transaction>>(&event);
    if (txs)
      for (const transaction& candidate : *txs)
      {
        tx_extra_vm_call call{};
        if (get_tx_vm_call(candidate.extra, call) == 1 && call.method == method)
          return &candidate;
      }
  }
  return nullptr;
}

bool gen_contracts_base::construct_call_tx(const ring_chain& chain, const tx_extra_vm_call& call, transaction& tx, size_t spender, const std::vector<crypto::public_key>& external_contract_signers) const
{
  tx_extra_vm_call prepared = call;
  if (!prepared.signers.empty())
  {
    if (prepared.signer_contracts.empty())
      prepared.signer_contracts.assign(prepared.signers.size(), prepared.contract_id);
    if (prepared.signer_methods.empty())
      prepared.signer_methods.assign(prepared.signers.size(), prepared.method);
    if (prepared.signer_key_ids.empty())
      prepared.signer_key_ids.assign(prepared.signers.size(), crypto::null_hash);
    if (prepared.signer_nested.empty())
      prepared.signer_nested.assign(prepared.signers.size(), 0);
  }
  std::vector<uint8_t> extra;
  CHECK_AND_ASSERT_MES(add_vm_call_to_tx_extra(extra, prepared), false, "failed to add call");
  return construct_window_tx(chain, extra, tx, spender, external_contract_signers);
}

bool gen_contracts_base::push_kept_txs(std::vector<test_event_entry>& events, const std::vector<transaction>& txs) const
{
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block);
  for (const transaction& tx : txs)
    events.push_back(std::vector<transaction>{tx});
  SET_EVENT_VISITOR_SETT(events, 0);
  return true;
}

bool gen_contracts_base::generate_rejected_calls(std::vector<test_event_entry>& events, const std::vector<tx_extra_vm_call>& calls, bool deploy_first) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  cryptonote::block prev = chain.tip;
  size_t spender = 0;
  if (deploy_first)
  {
    transaction deploy;
    CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, spender++), false, "deploy tx");
    push_kept_txs(events, {deploy});
    cryptonote::block with_deploy;
    CHECK_AND_ASSERT_MES(construct_next_block(chain, prev, chain.miner, fork_version, {deploy}, with_deploy), false, "deploy block");
    events.push_back(with_deploy);
    prev = with_deploy;
  }
  std::vector<transaction> txs;
  for (const tx_extra_vm_call& call : calls)
  {
    txs.emplace_back();
    CHECK_AND_ASSERT_MES(construct_call_tx(chain, call, txs.back(), spender++), false, "call tx");
  }
  push_kept_txs(events, txs);
  cryptonote::block rejected;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, prev, chain.miner, fork_version, txs, rejected), false, "rejected block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(rejected);
  DO_CALLBACK(events, "check_height_unchanged");
  if (deploy_first)
    DO_CALLBACK(events, "check_vm_mirror");
  return true;
}

bool gen_contracts_base::generate_rejected_coinbase_root(std::vector<test_event_entry>& events, uint8_t hf_version, const boost::optional<crypto::hash>& root) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  cryptonote::block valid;
  test_generator scratch = chain.generator;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, hf_version, {}, valid), false, "template block");
  transaction miner_tx = valid.miner_tx;
  remove_field_from_tx_extra(miner_tx.extra, typeid(tx_extra_vm_state_root));
  if (root)
    CHECK_AND_ASSERT_MES(add_vm_state_root_to_tx_extra(miner_tx.extra, *root), false, "failed to add root");
  cryptonote::block rejected;
  CHECK_AND_ASSERT_MES(scratch.construct_block_manually(rejected, chain.tip, chain.miner,
      test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_timestamp | test_generator::bf_hf_version | test_generator::bf_miner_tx,
      hf_version, hf_version, valid.timestamp, crypto::hash(), 0, miner_tx, std::vector<crypto::hash>(), 0, 0, hf_version),
      false, "rejected block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(rejected);
  DO_CALLBACK(events, "check_height_unchanged");
  return true;
}

bool gen_contracts_base::check_vm_mirror(cryptonote::core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_vm_mirror");
  const Blockchain& chain = c.get_blockchain_storage();
  const BlockchainDB& db = chain.get_db();
  crypto::hash root;
  CHECK_TEST_CONDITION(tip_root(db, root));
  CHECK_EQ(root, chain.get_vm_state_root());
  vm_block_commitment commitment;
  CHECK_TEST_CONDITION(db.get_vm_commitment(db.height() - 1, commitment));
  CHECK_EQ(root, commitment.root);
  return true;
}

bool gen_contracts_base::check_stored(cryptonote::core& c, const std::string& value)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_stored");
  const BlockchainDB& db = c.get_blockchain_storage().get_db();
  std::string code;
  CHECK_TEST_CONDITION(db.get_vm_state_value(vm::contract_code_key(contract_id()), code));
  std::string stored;
  CHECK_TEST_CONDITION(db.get_vm_state_value(vm_test::var_key(contract_id(), 0, stored_key), stored));
  CHECK_EQ(value, stored);
  return true;
}

bool gen_contracts_base::check_stored_cd(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  return check_vm_mirror(c, ev_index, events) && check_stored(c, "cd");
}

bool gen_contracts_base::check_stored_xy(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  return check_vm_mirror(c, ev_index, events) && check_stored(c, "xy");
}

bool gen_contracts_base::check_dry_run_and_proof(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_dry_run_and_proof");
  const Blockchain& chain = c.get_blockchain_storage();
  const std::string key = vm_test::var_key(contract_id(), 0, stored_key);
  crypto::hash root;
  boost::optional<std::string> value;
  vm::state_proof proof;
  CHECK_TEST_CONDITION(chain.get_vm_state_proof(key, root, value, proof));
  CHECK_EQ(root, chain.get_vm_state_root());
  CHECK_TEST_CONDITION(value && *value == "cd");
  CHECK_TEST_CONDITION(vm::state_tree::verify_proof(root, key, value, proof));
  CHECK_TEST_CONDITION(!vm::state_tree::verify_proof(root, key, std::string("xy"), proof));
  const std::string absent = vm_test::var_key(contract_id(), 0, "zz");
  CHECK_TEST_CONDITION(chain.get_vm_state_proof(absent, root, value, proof));
  CHECK_TEST_CONDITION(!value && vm::state_tree::verify_proof(root, absent, boost::none, proof));

  uint64_t height = 0;
  vm_block_commitment commitment;
  CHECK_TEST_CONDITION(chain.get_vm_tip_commitment(height, commitment));
  CHECK_EQ(height, chain.get_current_blockchain_height() - 1);
  CHECK_EQ(root, commitment.root);

  tx_extra_vm_call call = invoke_call(vm_test::m_store, stored_key + "ef");
  contract_effects effects;
  const auto dry_run = [&](uint64_t gas_limit) {
    call.gas_limit = gas_limit;
    transaction tx{};
    uint64_t fee = 0;
    if (!add_vm_call_to_tx_extra(tx.extra, call) || !contract_call_fee_floor(tx.extra, fee))
      return contract_call_verdict::malformed;
    return chain.dry_run_contract_tx(tx, fee, effects);
  };
  CHECK_TEST_CONDITION(dry_run(QWC_VM_TX_GAS_CAP) == contract_call_verdict::executed);
  const uint64_t gas_used = effects.gas_used;
  CHECK_TEST_CONDITION(gas_used > 0 && gas_used < QWC_VM_TX_GAS_CAP);
  CHECK_TEST_CONDITION(dry_run(contract_call_gas_limit(gas_used)) == contract_call_verdict::executed);
  CHECK_TEST_CONDITION(dry_run(gas_used) == contract_call_verdict::executed);
  CHECK_EQ(gas_used, effects.gas_used);
  CHECK_TEST_CONDITION(dry_run(gas_used - 1) == contract_call_verdict::failed);
  call.gas_limit = gas_used;
  transaction snapshot_tx{};
  CHECK_TEST_CONDITION(add_vm_call_to_tx_extra(snapshot_tx.extra, call));
  uint64_t snapshot_fee = 0;
  CHECK_TEST_CONDITION(contract_call_fee_floor(snapshot_tx.extra, snapshot_fee));
  contract_effects expected_effects{};
  uint64_t expected_height = 0;
  const contract_call_verdict expected_verdict = chain.dry_run_contract_tx(snapshot_tx, snapshot_fee, expected_effects, true, &expected_height);
  std::promise<void> captured;
  std::promise<void> release;
  std::future<void> resume = release.get_future();
  contract_effects concurrent_effects{};
  contract_call_verdict concurrent_verdict = contract_call_verdict::failed;
  uint64_t concurrent_height = 0;
  std::thread worker([&] {
    concurrent_verdict = chain.dry_run_contract_tx(snapshot_tx, snapshot_fee, concurrent_effects, true, &concurrent_height, [&] {
      captured.set_value();
      resume.wait();
    });
  });
  captured.get_future().wait();
  auto &block_lock = blockchain_test_access::blockchain_lock(c.get_blockchain_storage());
  const bool block_can_lock = block_lock.tryLock();
  if (block_can_lock)
    block_lock.unlock();
  bool block_handled = false;
  if (block_can_lock)
  {
    account_base miner;
    miner.generate();
    block candidate;
    difficulty_type difficulty;
    uint64_t height = 0, reward = 0, seed_height = 0;
    crypto::hash seed_hash;
    block_handled = c.get_blockchain_storage().create_block_template(candidate, miner.get_keys().m_account_address,
        difficulty, height, reward, blobdata(), seed_height, seed_hash)
        && mine_template(c, c.get_blockchain_storage(), candidate, difficulty, height);
  }
  release.set_value();
  worker.join();
  CHECK_TEST_CONDITION(block_can_lock);
  CHECK_TEST_CONDITION(block_handled);
  CHECK_EQ(concurrent_height, expected_height);
  CHECK_TEST_CONDITION(concurrent_verdict == expected_verdict);
  CHECK_EQ(concurrent_effects.gas_used, expected_effects.gas_used);
  CHECK_EQ(concurrent_effects.deposited, expected_effects.deposited);
  CHECK_EQ(concurrent_effects.withdrawn, expected_effects.withdrawn);
  return check_stored(c, "cd");
}

bool gen_contracts_base::check_height_unchanged(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_height_unchanged");
  size_t accepted = 0;
  bool invalid_next = false;
  for (size_t i = 0; i < ev_index && i < events.size(); ++i)
  {
    if (events[i].type() == typeid(callback_entry))
      invalid_next = boost::get<callback_entry>(events[i]).callback_name == "mark_invalid_block";
    else if (events[i].type() == typeid(cryptonote::block))
    {
      accepted += invalid_next ? 0 : 1;
      invalid_next = false;
    }
  }
  CHECK_EQ(accepted, c.get_blockchain_storage().get_current_blockchain_height());
  return true;
}

bool gen_contracts_base::check_main_chain_kept(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_main_chain_kept");
  const cryptonote::block* main_tip = nullptr;
  for (size_t i = 0; i < ev_index && i < events.size(); ++i)
    if (events[i].type() == typeid(callback_entry) && boost::get<callback_entry>(events[i]).callback_name == "check_stored_cd")
      for (size_t j = i; j-- > 0;)
        if (events[j].type() == typeid(cryptonote::block))
        {
          main_tip = &boost::get<cryptonote::block>(events[j]);
          break;
        }
  CHECK_TEST_CONDITION(main_tip != nullptr);
  CHECK_EQ(get_block_height(*main_tip) + 1, c.get_blockchain_storage().get_current_blockchain_height());
  CHECK_EQ(get_block_hash(*main_tip), c.get_blockchain_storage().get_tail_id());
  return check_stored_cd(c, ev_index, events);
}

bool gen_contracts_base::check_template_drops_failing_call(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_template_drops_failing_call");
  Blockchain& chain = c.get_blockchain_storage();
  uint8_t miner_data_version = 0;
  uint64_t miner_data_height = 0, median_weight = 0, generated_coins = 0;
  crypto::hash miner_data_prev, miner_data_seed;
  difficulty_type miner_data_difficulty;
  std::vector<tx_block_template_backlog_entry> backlog;
  CHECK_TEST_CONDITION(!chain.get_miner_data(miner_data_version, miner_data_height, miner_data_prev, miner_data_seed, miner_data_difficulty, median_weight, generated_coins, backlog));

  std::vector<transaction> pool;
  CHECK_TEST_CONDITION(c.get_pool_transactions(pool, true));
  CHECK_EQ(2, pool.size());

  cryptonote::account_base miner;
  miner.generate();
  cryptonote::block blk;
  difficulty_type difficulty;
  uint64_t height = 0, expected_reward = 0, seed_height = 0;
  crypto::hash seed_hash;
  CHECK_TEST_CONDITION(chain.create_block_template(blk, miner.get_keys().m_account_address, difficulty, height, expected_reward, blobdata(), seed_height, seed_hash));
  CHECK_EQ(1, blk.tx_hashes.size());
  for (const transaction& tx : pool)
  {
    tx_extra_vm_call call{};
    CHECK_EQ(1, get_tx_vm_call(tx.extra, call));
    const bool included = get_transaction_hash(tx) == blk.tx_hashes.front();
    CHECK_TEST_CONDITION(included == (call.method == static_cast<uint32_t>(vm_test::m_store)));
  }
  crypto::hash root;
  CHECK_EQ(1, get_tx_vm_state_root(blk.miner_tx.extra, root));

  blk.nonce = 0;
  while (!miner::find_nonce_for_given_block([&chain](const cryptonote::block& b, uint64_t h, const crypto::hash* seed, unsigned int threads, crypto::hash& hash) {
    return get_block_longhash(&chain, b, hash, h, seed, threads);
  }, blk, difficulty, height, NULL))
    ++blk.timestamp;
  block_verification_context bvc{};
  pool_supplement no_extra_txs;
  CHECK_TEST_CONDITION(c.handle_single_incoming_block(block_to_blob(blk), &blk, bvc, no_extra_txs, false));
  CHECK_TEST_CONDITION(!bvc.m_verifivation_failed && bvc.m_added_to_main_chain);
  return check_stored_cd(c, ev_index, events);
}

bool gen_contracts_base::check_pool_rejects_call(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_pool_rejects_call");
  tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_contract_call_rejected && tvc.m_no_drop_offense && !tvc.m_fee_too_low);
  return true;
}

bool gen_contracts_base::check_pool_rejects_signature(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_pool_rejects_signature");
  tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_contract_call_rejected && !tvc.m_no_drop_offense);
  return true;
}

bool gen_contracts_base::check_pool_rejects_signer_mismatch(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_pool_rejects_signer_mismatch");
  tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_contract_call_rejected && tvc.m_no_drop_offense);
  return true;
}

bool gen_contracts_base::check_pool_rejects_forged_input(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_pool_rejects_forged_input");
  tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_invalid_input && !tvc.m_contract_call_rejected);
  return true;
}

bool gen_contracts_base::check_pool_rejects_malformed_extra(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_pool_rejects_malformed_extra");
  tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && !tvc.m_no_drop_offense);
  return true;
}

bool gen_contracts_base::check_pool_rejects_underpaid(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_pool_rejects_underpaid");
  tx_verification_context tvc;
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_fee_too_low && !tvc.m_contract_call_rejected);
  return true;
}

bool gen_contracts_base::check_zero_output_input_rejected(core& c, size_t ev_index,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_zero_output_input_rejected");
  CHECK_TEST_CONDITION(ev_index > 0);
  const auto *txs = boost::get<std::vector<transaction>>(&events[ev_index - 1]);
  CHECK_TEST_CONDITION(txs != nullptr && txs->size() == 1);
  CHECK_TEST_CONDITION(check_zero_output_inputs(c, txs->front(), false));
  return true;
}

bool gen_contracts_base::check_zero_output_input_accepted(core& c, size_t ev_index,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_zero_output_input_accepted");
  CHECK_TEST_CONDITION(ev_index >= 2);
  const auto *tx = boost::get<transaction>(&events[ev_index - 2]);
  CHECK_TEST_CONDITION(tx != nullptr);
  CHECK_TEST_CONDITION(check_zero_output_inputs(c, *tx, true));
  return true;
}

bool gen_contracts_deploy_and_invoke::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, store;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store, 1), false, "store tx");
  push_kept_txs(events, {deploy, store});
  cryptonote::block blk;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy, store}, blk), false, "block");
  events.push_back(blk);
  DO_CALLBACK(events, "check_stored_cd");
  cryptonote::block next;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, blk, chain.miner, fork_version, {}, next), false, "next block");
  events.push_back(next);
  DO_CALLBACK(events, "check_stored_cd");
  DO_CALLBACK(events, "check_dry_run_and_proof");
  return true;
}

bool gen_contracts_zero_output_call::generate(std::vector<test_event_entry>& events) const
{
  return generate_zero_output_case(events, false);
}

bool gen_contracts_zero_output_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_zero_output_case(events, true);
}

bool gen_contracts_base::generate_zero_output_case(std::vector<test_event_entry>& events,
    bool reject) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  transaction deployment;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deployment, 0), false, "deployment");
  push_kept_txs(events, {deployment});
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version,
      {deployment}, deployed), false, "deployment block");
  events.push_back(deployed);

  const account_keys &sender = chain.ring_miners[1].get_keys();
  crypto::public_key signer{};
  crypto::secret_key signer_secret{};
  CHECK_AND_ASSERT_MES(derive_contract_key(sender, contract_id(), crypto::null_hash,
      signer, signer_secret), false, "signer");
  const tx_extra_vm_call call = invoke_call(vm_test::m_addsig,
      std::string(reinterpret_cast<const char *>(&signer), sizeof(signer)));
  const tx_source_entry source = ring_source(chain, 1);
  transaction unsigned_call;
  CHECK_AND_ASSERT_MES(construct_signed_zero_output_call_v1(sender, source,
      call, contract_id(), unsigned_call), false, "zero output call");
  CHECK_AND_ASSERT_MES(unsigned_call.version == 1 && unsigned_call.vout.empty(), false, "output shape");
  uint64_t fee = 0;
  CHECK_AND_ASSERT_MES(get_tx_fee(unsigned_call, fee) && fee == chain.spent_amount,
      false, "fee");
  tx_extra_vm_call signed_call{};
  CHECK_AND_ASSERT_MES(get_tx_vm_call(unsigned_call.extra, signed_call) == 1,
      false, "signed call");
  crypto::hash signature_hash{};
  CHECK_AND_ASSERT_MES(get_contract_signature_hash(unsigned_call, FAKECHAIN, fee,
      contract_id(), signature_hash), false, "signature hash");
  CHECK_AND_ASSERT_MES(crypto::check_signature(signature_hash, signer,
      signed_call.signatures.front().value), false, "fee signature");
  crypto::hash other_hash{};
  CHECK_AND_ASSERT_MES(get_contract_signature_hash(unsigned_call, FAKECHAIN, fee - 1,
      contract_id(), other_hash)
      && !crypto::check_signature(other_hash, signer,
          signed_call.signatures.front().value), false, "different fee signature");

  if (reject)
  {
    transaction control = unsigned_call;
    CHECK_AND_ASSERT_MES(resign_constructed_tx_v1(control, sender, source),
        false, "control signature");
    push_unsubmitted_tx(events, control);
    DO_CALLBACK(events, "check_zero_output_input_accepted");
    const auto adjust_gas_price = [](transaction &tx, bool increase) {
      tx_extra_vm_call adjusted{};
      if (get_tx_vm_call(tx.extra, adjusted) != 1)
        return false;
      if (increase)
        ++adjusted.gas_price;
      else
        --adjusted.gas_price;
      return remove_field_from_tx_extra(tx.extra, typeid(tx_extra_vm_call))
          && add_vm_call_to_tx_extra(tx.extra, adjusted)
          && sort_tx_extra(tx.extra, tx.extra, false, true, true);
    };
    const auto submit_invalid = [&](transaction tx) {
      if (!resign_constructed_tx_v1(tx, sender, source))
        return false;
      DO_CALLBACK(events, "mark_invalid_tx");
      events.push_back(std::vector<transaction>{tx});
      DO_CALLBACK(events, "check_zero_output_input_rejected");
      DO_CALLBACK(events, "check_last_tx_not_in_pool");
      return true;
    };
    transaction above = unsigned_call;
    CHECK_AND_ASSERT_MES(adjust_gas_price(above, false), false, "lower floor");
    CHECK_AND_ASSERT_MES(submit_invalid(above), false, "fee above floor");
    transaction below = unsigned_call;
    CHECK_AND_ASSERT_MES(adjust_gas_price(below, true), false, "raise floor");
    CHECK_AND_ASSERT_MES(submit_invalid(below), false, "fee below floor");
    transaction with_output = unsigned_call;
    CHECK_AND_ASSERT_MES(adjust_gas_price(with_output, false), false, "output floor");
    tx_out output{};
    output.amount = QWC_VM_TX_GAS_CAP;
    output.target = txout_to_tagged_key{signer, crypto::view_tag{}};
    with_output.vout.push_back(output);
    CHECK_AND_ASSERT_MES(submit_invalid(with_output), false, "output with call");
    transaction without_call = unsigned_call;
    CHECK_AND_ASSERT_MES(remove_field_from_tx_extra(without_call.extra,
        typeid(tx_extra_vm_call)), false, "remove call");
    CHECK_AND_ASSERT_MES(submit_invalid(without_call), false, "empty without call");
    return true;
  }

  const account_keys &pool_sender = chain.ring_miners[2].get_keys();
  crypto::public_key pool_signer{};
  crypto::secret_key pool_secret{};
  CHECK_AND_ASSERT_MES(derive_contract_key(pool_sender, contract_id(),
      crypto::null_hash, pool_signer, pool_secret), false, "pool signer");
  const tx_extra_vm_call pool_call = invoke_call(vm_test::m_addsig,
      std::string(reinterpret_cast<const char *>(&pool_signer), sizeof(pool_signer)));
  transaction pool_tx;
  CHECK_AND_ASSERT_MES(construct_signed_zero_output_call_v1(pool_sender,
      ring_source(chain, 2), pool_call, contract_id(), pool_tx), false, "pool call");
  events.push_back(std::vector<transaction>{pool_tx});
  DO_CALLBACK(events, "check_last_tx_in_pool");
  push_kept_txs(events, {unsigned_call});
  block accepted;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, deployed, chain.miner, fork_version,
      {unsigned_call}, accepted), false, "zero output block");
  events.push_back(accepted);
  DO_CALLBACK(events, "check_vm_mirror");
  return true;
}

bool gen_contracts_zero_output_before_fork_rejected::generate(
    std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  const account_keys &sender = chain.ring_miners[1].get_keys();
  crypto::public_key signer{};
  crypto::secret_key secret{};
  CHECK_AND_ASSERT_MES(derive_contract_key(sender, contract_id(), crypto::null_hash,
      signer, secret), false, "signer");
  const tx_extra_vm_call call = invoke_call(vm_test::m_addsig,
      std::string(reinterpret_cast<const char *>(&signer), sizeof(signer)));
  transaction tx;
  CHECK_AND_ASSERT_MES(construct_signed_zero_output_call_v1(sender,
      ring_source(chain, 1), call, contract_id(), tx), false, "pre-fork call");
  transaction control = tx;
  CHECK_AND_ASSERT_MES(resign_constructed_tx_v1(control, sender,
      ring_source(chain, 1)), false, "control signature");
  tx = control;
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{tx});
  DO_CALLBACK(events, "check_zero_output_input_rejected");
  DO_CALLBACK(events, "check_last_tx_not_in_pool");
  block before_activation;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner,
      HF_VERSION_QWC_EPOSE, {}, before_activation), false, "before activation");
  events.push_back(before_activation);
  push_unsubmitted_tx(events, control);
  DO_CALLBACK(events, "check_zero_output_input_accepted");
  block activation;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, before_activation, chain.miner,
      fork_version, {}, activation), false, "activation");
  events.push_back(activation);
  return true;
}

bool gen_contracts_failing_call_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_calls(events, {invoke_call(vm_test::m_unreachable, {})}, true);
}

bool gen_contracts_underpaid_rejected::generate(std::vector<test_event_entry>& events) const
{
  tx_extra_vm_call call = deploy_call();
  call.gas_price = std::numeric_limits<uint64_t>::max() / call.gas_limit;
  return generate_rejected_calls(events, {call}, false);
}

bool gen_contracts_tx_gas_cap_rejected::generate(std::vector<test_event_entry>& events) const
{
  tx_extra_vm_call call = deploy_call();
  call.gas_limit = QWC_VM_TX_GAS_CAP + 1;
  return generate_rejected_calls(events, {call}, false);
}

bool gen_contracts_root_mismatch_rejected::generate(std::vector<test_event_entry>& events) const
{
  crypto::hash bogus;
  memset(&bogus, 0x5a, sizeof(bogus));
  return generate_rejected_coinbase_root(events, fork_version, bogus);
}

bool gen_contracts_root_missing_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_coinbase_root(events, fork_version, boost::none);
}

bool gen_contracts_root_before_fork_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_coinbase_root(events, HF_VERSION_QWC_EPOSE, crypto::null_hash);
}

bool gen_contracts_call_before_fork_rejected::generate(std::vector<test_event_entry>& events) const
{
  std::vector<uint8_t> extra;
  CHECK_AND_ASSERT_MES(add_vm_call_to_tx_extra(extra, deploy_call()), false, "failed to add call");
  CHECK_AND_ASSERT_MES(generate_case(events, HF_VERSION_QWC_EPOSE, extra, false, false, false), false, "before fork case");
  DO_CALLBACK(events, "check_pool_rejects_call");
  return true;
}

gen_contracts_fork_boundary::gen_contracts_fork_boundary()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_fork_boundary, check_before_boundary);
  REGISTER_CALLBACK_METHOD(gen_contracts_fork_boundary, check_boundary);
}

bool gen_contracts_fork_boundary::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  events.pop_back();
  const block parent = boost::get<block>(events.back());
  transaction prior;
  CHECK_AND_ASSERT_MES(construct_window_tx(chain, std::vector<uint8_t>{}, prior, 0), false, "prior tx");
  events.push_back(std::vector<transaction>{prior});
  transaction legacy_call;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), legacy_call, 1), false, "legacy call");
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{legacy_call});
  DO_CALLBACK(events, "check_before_boundary");
  block before_fork;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, parent, chain.miner,
      HF_VERSION_QWC_CONTRACTS - 1, {}, before_fork), false, "before fork block");
  events.push_back(before_fork);
  DO_CALLBACK(events, "check_boundary");
  return true;
}

bool gen_contracts_fork_boundary::check_before_boundary(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_fork_boundary::check_before_boundary");
  Blockchain& chain = c.get_blockchain_storage();
  account_base miner;
  miner.generate();
  block candidate;
  difficulty_type difficulty;
  uint64_t height = 0, reward = 0, seed_height = 0;
  crypto::hash seed_hash;
  CHECK_TEST_CONDITION(chain.create_block_template(candidate, miner.get_keys().m_account_address,
      difficulty, height, reward, blobdata(), seed_height, seed_hash));
  CHECK_EQ(HF_VERSION_QWC_CONTRACTS - 1, candidate.major_version);
  tx_verification_context tvc{};
  CHECK_TEST_CONDITION(resubmit_last_tx(c, ev_index, events, tvc));
  CHECK_TEST_CONDITION(tvc.m_verifivation_failed && tvc.m_contract_call_rejected && tvc.m_no_drop_offense && !tvc.m_fee_too_low);
  return true;
}

bool gen_contracts_fork_boundary::check_boundary(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_fork_boundary::check_boundary");
  Blockchain& chain = c.get_blockchain_storage();
  account_base miner;
  miner.generate();
  block candidate;
  difficulty_type difficulty;
  uint64_t height = 0, reward = 0, seed_height = 0;
  crypto::hash seed_hash;
  const transaction *prior = nullptr, *legacy_call = nullptr;
  for (size_t i = 0; i < ev_index; ++i)
    if (events[i].type() == typeid(std::vector<transaction>))
    {
      const transaction& tx = boost::get<std::vector<transaction>>(events[i]).front();
      if (!prior) prior = &tx;
      else legacy_call = &tx;
    }
  CHECK_TEST_CONDITION(prior && legacy_call);
  tx_verification_context tvc{};
  CHECK_TEST_CONDITION(c.handle_incoming_tx(t_serializable_object_to_blob(*legacy_call), tvc, relay_method::block, false));
  CHECK_TEST_CONDITION(!tvc.m_verifivation_failed && c.pool_has_tx(get_transaction_hash(*legacy_call)));
  CHECK_TEST_CONDITION(chain.create_block_template(candidate, miner.get_keys().m_account_address,
      difficulty, height, reward, blobdata(), seed_height, seed_hash));
  CHECK_EQ(HF_VERSION_QWC_CONTRACTS, candidate.major_version);
  CHECK_EQ(2, candidate.tx_hashes.size());
  CHECK_TEST_CONDITION(std::find(candidate.tx_hashes.begin(), candidate.tx_hashes.end(), get_transaction_hash(*prior)) != candidate.tx_hashes.end());
  CHECK_TEST_CONDITION(std::find(candidate.tx_hashes.begin(), candidate.tx_hashes.end(), get_transaction_hash(*legacy_call)) != candidate.tx_hashes.end());
  CHECK_TEST_CONDITION(mine_template(c, chain, candidate, difficulty, height));
  chain.pop_blocks(2);
  CHECK_EQ(mined_height - 1, chain.get_current_blockchain_height());
  CHECK_TEST_CONDITION(c.pool_has_tx(get_transaction_hash(*legacy_call)));
  CHECK_TEST_CONDITION(chain.create_block_template(candidate, miner.get_keys().m_account_address,
      difficulty, height, reward, blobdata(), seed_height, seed_hash));
  CHECK_EQ(HF_VERSION_QWC_CONTRACTS - 1, candidate.major_version);
  CHECK_TEST_CONDITION(std::find(candidate.tx_hashes.begin(), candidate.tx_hashes.end(), get_transaction_hash(*legacy_call)) == candidate.tx_hashes.end());
  CHECK_TEST_CONDITION(mine_template(c, chain, candidate, difficulty, height));
  CHECK_TEST_CONDITION(chain.create_block_template(candidate, miner.get_keys().m_account_address,
      difficulty, height, reward, blobdata(), seed_height, seed_hash));
  CHECK_EQ(HF_VERSION_QWC_CONTRACTS, candidate.major_version);
  CHECK_TEST_CONDITION(std::find(candidate.tx_hashes.begin(), candidate.tx_hashes.end(), get_transaction_hash(*legacy_call)) != candidate.tx_hashes.end());
  CHECK_TEST_CONDITION(mine_template(c, chain, candidate, difficulty, height));
  return true;
}

gen_contracts_block_rules::gen_contracts_block_rules()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_block_rules, check_preverified_block_rejected);
  REGISTER_CALLBACK_METHOD(gen_contracts_block_rules, check_supplement_block_accepted);
}

bool gen_contracts_block_rules::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  events.pop_back();
  const block parent = boost::get<block>(events.back());
  transaction call_tx;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), call_tx), false, "call tx");
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block);
  events.push_back(std::vector<transaction>{call_tx});
  SET_EVENT_VISITOR_SETT(events, 0);
  block rejected;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, parent, chain.miner,
      HF_VERSION_QWC_CONTRACTS - 1, {call_tx}, rejected), false, "preactivation block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(rejected);
  DO_CALLBACK(events, "check_preverified_block_rejected");
  block before_fork;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, parent, chain.miner,
      HF_VERSION_QWC_CONTRACTS - 1, {}, before_fork), false, "before fork block");
  events.push_back(before_fork);
  block accepted;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, before_fork, chain.miner,
      HF_VERSION_QWC_CONTRACTS, {call_tx}, accepted), false, "activation block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(accepted);
  DO_CALLBACK(events, "check_supplement_block_accepted");
  return true;
}

bool gen_contracts_block_rules::check_preverified_block_rejected(cryptonote::core& c, size_t ev_index,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_block_rules::check_preverified_block_rejected");
  const transaction* call_tx = find_contract_tx(events, 0);
  CHECK_TEST_CONDITION(call_tx != nullptr);
  CHECK_TEST_CONDITION(ev_index > 0 && events[ev_index - 1].type() == typeid(block));
  const block& supplement_before_fork = boost::get<block>(events[ev_index - 1]);
  CHECK_TEST_CONDITION(c.pool_has_tx(get_transaction_hash(*call_tx)));
  CHECK_TEST_CONDITION(c.get_blockchain_storage().flush_txes_from_pool({get_transaction_hash(*call_tx)}));
  CHECK_TEST_CONDITION(!c.pool_has_tx(get_transaction_hash(*call_tx)));
  c.get_blockchain_storage().flush_invalid_blocks();
  const uint64_t height_before = c.get_blockchain_storage().get_current_blockchain_height();
  block_verification_context bvc{};
  CHECK_TEST_CONDITION(handle_block_with_supplement(c, supplement_before_fork, *call_tx, true, bvc));
  CHECK_TEST_CONDITION(bvc.m_verifivation_failed && !bvc.m_missing_txs);
  CHECK_EQ(height_before, c.get_blockchain_storage().get_current_blockchain_height());
  CHECK_TEST_CONDITION(!c.pool_has_tx(get_transaction_hash(*call_tx)));
  return true;
}

bool gen_contracts_block_rules::check_supplement_block_accepted(cryptonote::core& c, size_t ev_index,
    const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_block_rules::check_supplement_block_accepted");
  const transaction* call_tx = find_contract_tx(events, 0);
  CHECK_TEST_CONDITION(call_tx != nullptr);
  const block *blk = nullptr;
  for (size_t i = 0; i < ev_index; ++i)
    if (events[i].type() == typeid(block))
      blk = &boost::get<block>(events[i]);
  CHECK_TEST_CONDITION(blk != nullptr);
  Blockchain& chain = c.get_blockchain_storage();
  block_verification_context bvc{};
  CHECK_TEST_CONDITION(handle_block_with_supplement(c, *blk, *call_tx, false, bvc));
  CHECK_TEST_CONDITION(!bvc.m_verifivation_failed && bvc.m_added_to_main_chain);
  chain.pop_blocks(1);
  CHECK_TEST_CONDITION(c.pool_has_tx(get_transaction_hash(*call_tx)));
  account_base miner;
  miner.generate();
  block candidate;
  difficulty_type difficulty;
  uint64_t height = 0, reward = 0, seed_height = 0;
  crypto::hash seed_hash;
  CHECK_TEST_CONDITION(chain.create_block_template(candidate, miner.get_keys().m_account_address,
      difficulty, height, reward, blobdata(), seed_height, seed_hash));
  CHECK_EQ(HF_VERSION_QWC_CONTRACTS, candidate.major_version);
  CHECK_TEST_CONDITION(std::find(candidate.tx_hashes.begin(), candidate.tx_hashes.end(), get_transaction_hash(*call_tx)) != candidate.tx_hashes.end());
  CHECK_TEST_CONDITION(mine_template(c, chain, candidate, difficulty, height));
  return true;
}

bool gen_contracts_reorg_undo::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, store_cd, store_xy;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store_cd, 1), false, "store tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "xy"), store_xy, 2), false, "store tx");
  push_kept_txs(events, {deploy, store_cd});

  cryptonote::account_base miner_b;
  miner_b.generate();
  cryptonote::block a[4], b[3];
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy, store_cd}, a[0]), false, "a0");
  events.push_back(a[0]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a[0], chain.miner, fork_version, {}, a[1]), false, "a1");
  events.push_back(a[1]);
  DO_CALLBACK(events, "check_stored_cd");

  push_kept_txs(events, {store_xy});
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, miner_b, fork_version, {deploy}, b[0]), false, "b0");
  events.push_back(b[0]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b[0], miner_b, fork_version, {store_xy}, b[1]), false, "b1");
  events.push_back(b[1]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b[1], miner_b, fork_version, {}, b[2]), false, "b2");
  events.push_back(b[2]);
  DO_CALLBACK(events, "check_stored_xy");

  const cryptonote::block *prev = &a[1];
  for (size_t n = 2; n < 4; ++n)
  {
    CHECK_AND_ASSERT_MES(construct_next_block(chain, *prev, chain.miner, fork_version, {}, a[n]), false, "a");
    events.push_back(a[n]);
    prev = &a[n];
  }
  DO_CALLBACK(events, "check_stored_cd");
  return true;
}

bool gen_contracts_template_drops_failing_call::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, store, failing;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store, 1), false, "store tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_unreachable, {}), failing, 2), false, "failing tx");
  push_kept_txs(events, {deploy});
  cryptonote::block blk;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy}, blk), false, "deploy block");
  events.push_back(blk);
  push_kept_txs(events, {store, failing});
  DO_CALLBACK(events, "check_template_drops_failing_call");
  return true;
}

bool gen_contracts_failing_alt_chain_keeps_main::generate(std::vector<test_event_entry>& events) const
{
  return generate_failing_reorg(events, false);
}

bool gen_contracts_failing_reorg_shared_tx::generate(std::vector<test_event_entry>& events) const
{
  return generate_failing_reorg(events, true);
}

bool gen_contracts_base::generate_failing_reorg(std::vector<test_event_entry>& events, bool shared_tx) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, store_cd, failing;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store_cd, 1), false, "store tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_unreachable, {}), failing, 2), false, "failing tx");
  push_kept_txs(events, {deploy, store_cd});

  cryptonote::account_base miner_b;
  miner_b.generate();
  cryptonote::block a[2], b[3];
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version,
      shared_tx ? std::vector<transaction>{deploy} : std::vector<transaction>{deploy, store_cd}, a[0]), false, "a0");
  events.push_back(a[0]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a[0], chain.miner, fork_version,
      shared_tx ? std::vector<transaction>{store_cd} : std::vector<transaction>{}, a[1]), false, "a1");
  events.push_back(a[1]);
  DO_CALLBACK(events, "check_stored_cd");

  push_kept_txs(events, {failing});
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, miner_b, fork_version, {deploy}, b[0]), false, "b0");
  events.push_back(b[0]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b[0], miner_b, fork_version,
      shared_tx ? std::vector<transaction>{} : std::vector<transaction>{failing}, b[1]), false, "b1");
  events.push_back(b[1]);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b[1], miner_b, fork_version,
      shared_tx ? std::vector<transaction>{store_cd, failing} : std::vector<transaction>{}, b[2]), false, "b2");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(b[2]);
  DO_CALLBACK(events, "check_main_chain_kept");
  return true;
}

bool gen_contracts_pool_rules::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, failing, probe, underpaid, store;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  push_kept_txs(events, {deploy});
  cryptonote::block blk;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy}, blk), false, "deploy block");
  events.push_back(blk);

  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_unreachable, {}), failing, 1), false, "failing tx");
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{failing});
  DO_CALLBACK(events, "check_pool_rejects_call");

  tx_extra_vm_call underpaid_call = invoke_call(vm_test::m_store, stored_key + "xy");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, underpaid_call, probe, 2), false, "probe tx");
  uint64_t fee = 0;
  CHECK_AND_ASSERT_MES(get_tx_fee(probe, fee) && fee > underpaid_call.gas_limit * underpaid_call.gas_price + 1, false, "probe fee");
  underpaid_call.deposit = fee - underpaid_call.gas_limit * underpaid_call.gas_price - 1;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, underpaid_call, underpaid, 2), false, "underpaid tx");
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{underpaid});
  DO_CALLBACK(events, "check_pool_rejects_underpaid");

  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store, 3), false, "store tx");
  events.push_back(std::vector<transaction>{store});
  cryptonote::block next;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, blk, chain.miner, fork_version, {store}, next), false, "store block");
  events.push_back(next);
  DO_CALLBACK(events, "check_stored_cd");
  return true;
}

bool gen_contracts_base::check_mint_supply(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_base::check_mint_supply");
  CHECK_TEST_CONDITION(check_vm_mirror(c, ev_index, events));
  const BlockchainDB& db = c.get_blockchain_storage().get_db();
  const uint64_t height = db.height() - 1;
  const cryptonote::block tip = db.get_block_from_height(height);
  CHECK_EQ(1u, tip.tx_hashes.size());
  const transaction unlock = db.get_tx(tip.tx_hashes.front());
  tx_extra_vm_call call{};
  CHECK_EQ(1u, get_tx_vm_call(unlock.extra, call));
  CHECK_EQ(1u, call.mint.size());

  const uint64_t minted = vm_test::locked_per_call;
  const tx_out& mint_out = tip.miner_tx.vout.back();
  CHECK_EQ(minted, mint_out.amount);
  crypto::public_key mint_key;
  CHECK_TEST_CONDITION(get_output_public_key(mint_out, mint_key));
  CHECK_EQ(call.mint.front().key, mint_key);
  CHECK_TEST_CONDITION(*get_output_view_tag(mint_out) == call.mint.front().view_tag);
  const std::vector<crypto::public_key> output_keys = get_additional_tx_pub_keys_from_extra(tip.miner_tx);
  CHECK_EQ(tip.miner_tx.vout.size(), output_keys.size());
  CHECK_EQ(call.mint.front().tx_key, output_keys.back());
  CHECK_EQ(height + QWC_MINT_UNLOCK_BLOCKS, tip.miner_tx.unlock_time);

  vm_block_commitment commitment;
  CHECK_TEST_CONDITION(db.get_vm_commitment(height, commitment));
  CHECK_EQ(minted, commitment.deposited);
  CHECK_EQ(minted, commitment.withdrawn);

  const uint64_t miner_fee = contract_miner_fee(unlock, get_tx_fee(unlock), tip.major_version);
  const uint64_t generated = db.get_block_already_generated_coins(height) - db.get_block_already_generated_coins(height - 1);
  CHECK_EQ(get_outs_money_amount(tip.miner_tx), generated + miner_fee + minted);
  const auto sums = c.get_coinbase_tx_sum(height, 1);
  CHECK_TEST_CONDITION(sums.first == generated);
  CHECK_TEST_CONDITION(sums.second == miner_fee);
  return true;
}

bool gen_contracts_mint_supply_invariant::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  transaction deploy, lock, unlock;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  tx_extra_vm_call lock_call = invoke_call(vm_test::m_funds_lock, {});
  lock_call.deposit = vm_test::locked_per_call;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, lock_call, lock, 1), false, "lock tx");
  push_kept_txs(events, {deploy, lock});
  cryptonote::block locked;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy, lock}, locked), false, "lock block");
  events.push_back(locked);
  DO_CALLBACK(events, "check_vm_mirror");

  tx_extra_vm_call unlock_call = invoke_call(vm_test::m_funds_unlock, {});
  tx_extra_vm_mint_destination destination{};
  destination.key = keypair::generate(hw::get_device("default")).pub;
  destination.view_tag.data = 0x2a;
  destination.tx_key = keypair::generate(hw::get_device("default")).pub;
  unlock_call.mint.push_back(destination);
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, unlock_call, unlock, 2), false, "unlock tx");
  push_kept_txs(events, {unlock});

  const auto push_tampered = [&](const std::function<void(transaction&)>& tamper) {
    ring_chain scratch = chain;
    cryptonote::block valid;
    CHECK_AND_ASSERT_MES(construct_next_block(scratch, locked, chain.miner, fork_version, {unlock}, valid), false, "template block");
    transaction miner_tx = valid.miner_tx;
    tamper(miner_tx);
    cryptonote::block rejected;
    CHECK_AND_ASSERT_MES(scratch.generator.construct_block_manually(rejected, locked, chain.miner,
        test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_timestamp | test_generator::bf_hf_version | test_generator::bf_miner_tx | test_generator::bf_tx_hashes,
        fork_version, fork_version, valid.timestamp, crypto::hash(), 0, miner_tx, valid.tx_hashes, 0, 0, fork_version),
        false, "rejected block");
    DO_CALLBACK(events, "mark_invalid_block");
    events.push_back(rejected);
    DO_CALLBACK(events, "check_vm_mirror");
    return true;
  };
  CHECK_AND_ASSERT_MES(push_tampered([](transaction& tx) {
    set_tx_out(tx.vout.back().amount, keypair::generate(hw::get_device("default")).pub, true, *get_output_view_tag(tx.vout.back()), tx.vout.back());
  }), false, "wrong mint key");
  CHECK_AND_ASSERT_MES(push_tampered([](transaction& tx) {
    tx.vout.pop_back();
    remove_field_from_tx_extra(tx.extra, typeid(tx_extra_additional_pub_keys));
  }), false, "missing mint");

  cryptonote::block minted;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, locked, chain.miner, fork_version, {unlock}, minted), false, "mint block");
  events.push_back(minted);
  DO_CALLBACK(events, "check_mint_supply");
  return true;
}
