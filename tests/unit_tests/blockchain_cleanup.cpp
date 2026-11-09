#include <algorithm>
#include <thread>

#include <gtest/gtest.h>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/hardfork.h"
#include "cryptonote_basic/height_window.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/tx_pool.h"
#include "blockchain_test_db.h"
#include "../common/blockchain_and_pool.h"

namespace cryptonote
{
  class BlockchainCleanupHarness
  {
  public:
    static void configure(Blockchain &chain)
    {
      auto *db = new BlockchainTestDB();
      db->chain_height = 2;
      chain.m_db = db;
      chain.m_nettype = FAKECHAIN;
      chain.m_hardfork = new HardFork(*chain.m_db, HF_VERSION_QWC_CONTRACTS, 0);
      chain.m_hardfork->init();
    }

    static crypto::hash queue(Blockchain &chain, const transaction &tx)
    {
      const blobdata blob = tx_to_blob(tx);
      const crypto::hash id = get_transaction_hash(tx);
      chain.m_returned_pool_txs.push_back({tx, blob, id, blob.size(), HF_VERSION_QWC_CONTRACTS});
      return id;
    }

    static bool cleanup_failed_batch(Blockchain &chain)
    {
      chain.prepare_handle_incoming_block_no_preprocess(0);
      chain.m_batch_success = false;
      return chain.cleanup_handle_incoming_blocks();
    }

    static bool returned_pool_txs_empty(const Blockchain &chain)
    {
      return chain.m_returned_pool_txs.empty();
    }

    static bool returned_pool_tx_queued(const Blockchain &chain, const crypto::hash &id)
    {
      return std::any_of(chain.m_returned_pool_txs.begin(), chain.m_returned_pool_txs.end(),
          [&id](const Blockchain::returned_pool_tx &tx) { return tx.id == id; });
    }

    static void fail_sync(Blockchain &chain)
    {
      static_cast<BlockchainTestDB &>(*chain.m_db).throw_on_sync = true;
      chain.m_sync_counter = 1;
      chain.m_db_sync_mode = db_sync;
    }

    static void cleanup_with_sync(Blockchain &chain)
    {
      chain.prepare_handle_incoming_block_no_preprocess(0);
      chain.cleanup_handle_incoming_blocks(true);
    }

  };
}

namespace
{
  bool pool_lock_free(cryptonote::tx_memory_pool &pool)
  {
    bool acquired = false;
    std::thread worker([&]() {
      acquired = pool.try_lock();
      if (acquired)
        pool.unlock();
    });
    worker.join();
    return acquired;
  }

  cryptonote::transaction returned_transaction()
  {
    cryptonote::transaction tx{};
    tx.version = 2;
    tx.rct_signatures.txnFee = 1;
    cryptonote::txin_to_key input{};
    input.amount = 100;
    input.key_offsets.push_back(0);
    tx.vin.push_back(input);
    cryptonote::tx_out output{};
    output.target = cryptonote::txout_to_key{crypto::null_pkey};
    tx.vout.push_back(output);
    return tx;
  }
}

TEST(blockchain_cleanup, failed_batch_returns_taken_transaction_to_pool)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainCleanupHarness::configure(chain.bc);
  const crypto::hash id = cryptonote::BlockchainCleanupHarness::queue(
      chain.bc, returned_transaction());
  ASSERT_FALSE(chain.txpool.have_tx(id, cryptonote::relay_category::all));
  ASSERT_FALSE(cryptonote::BlockchainCleanupHarness::cleanup_failed_batch(chain.bc));
  EXPECT_TRUE(chain.txpool.have_tx(id, cryptonote::relay_category::all));
  EXPECT_TRUE(cryptonote::BlockchainCleanupHarness::returned_pool_txs_empty(chain.bc));
  EXPECT_TRUE(pool_lock_free(chain.txpool));
}

TEST(blockchain_cleanup, expired_returned_transaction_is_dropped)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainCleanupHarness::configure(chain.bc);
  cryptonote::transaction tx = returned_transaction();
  ASSERT_TRUE(cryptonote::add_height_window_to_tx_extra(tx.extra, {1, 1}));
  tx.invalidate_hashes();
  const crypto::hash id = cryptonote::BlockchainCleanupHarness::queue(chain.bc, tx);
  ASSERT_FALSE(cryptonote::BlockchainCleanupHarness::cleanup_failed_batch(chain.bc));
  EXPECT_FALSE(chain.txpool.have_tx(id, cryptonote::relay_category::all));
  EXPECT_TRUE(chain.txpool.have_expired_window_tx(id));
  EXPECT_TRUE(cryptonote::BlockchainCleanupHarness::returned_pool_txs_empty(chain.bc));
  EXPECT_TRUE(pool_lock_free(chain.txpool));
}

TEST(blockchain_cleanup, sync_failure_keeps_returned_transaction_queued)
{
  cryptonote_test::BlockchainAndPool chain;
  cryptonote::BlockchainCleanupHarness::configure(chain.bc);
  const crypto::hash id = cryptonote::BlockchainCleanupHarness::queue(
      chain.bc, returned_transaction());
  cryptonote::BlockchainCleanupHarness::fail_sync(chain.bc);
  EXPECT_THROW(cryptonote::BlockchainCleanupHarness::cleanup_with_sync(chain.bc), std::runtime_error);
  EXPECT_TRUE(cryptonote::BlockchainCleanupHarness::returned_pool_tx_queued(chain.bc, id));
  EXPECT_TRUE(pool_lock_free(chain.txpool));
}
