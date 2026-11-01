#include "contracts_template_cache.h"
#include "../unit_tests/vm_test_utils.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/blockchain.h"
#include "device/device.hpp"

using namespace cryptonote;

gen_contracts_cached_template::gen_contracts_cached_template()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_cached_template, check_cached_template);
}

bool gen_contracts_cached_template::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  transaction deploy, store;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy tx");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cache"), store, 1), false, "store tx");
  CHECK_AND_ASSERT_MES(push_kept_txs(events, {deploy}), false, "pool deploy");
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy}, deployed), false, "deploy block");
  events.push_back(deployed);
  CHECK_AND_ASSERT_MES(push_kept_txs(events, {store}), false, "pool store");
  DO_CALLBACK(events, "check_cached_template");
  return true;
}

bool gen_contracts_cached_template::check_cached_template(core& core, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_cached_template::check_cached_template");
  Blockchain& chain = core.get_blockchain_storage();
  std::vector<transaction> pool;
  CHECK_TEST_CONDITION(core.get_pool_transactions(pool, true));
  CHECK_EQ(1, pool.size());
  const crypto::hash expected = get_transaction_hash(pool.front());

  account_base miner;
  miner.generate();
  for (unsigned attempt = 0; attempt < 2; ++attempt)
  {
    block candidate;
    difficulty_type difficulty;
    uint64_t height = 0, reward = 0, seed_height = 0;
    crypto::hash seed_hash;
    CHECK_TEST_CONDITION(chain.create_block_template(candidate, miner.get_keys().m_account_address,
        difficulty, height, reward, blobdata(), seed_height, seed_hash));
    CHECK_EQ(1, candidate.tx_hashes.size());
    CHECK_EQ(expected, candidate.tx_hashes.front());
  }
  return true;
}
