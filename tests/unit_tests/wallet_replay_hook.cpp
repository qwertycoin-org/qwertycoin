#include <ctime>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "wallet/wallet2.h"

class wallet_accessor_test
{
public:
  static crypto::hash genesis_hash(const tools::wallet2 &wallet)
  {
    return wallet.m_blockchain.genesis();
  }

  static bool should_skip_block(const tools::wallet2 &wallet, const cryptonote::block &block, uint64_t height)
  {
    return wallet.should_skip_block(block, height);
  }

  static void process_blocks(tools::wallet2 &wallet,
                             const std::vector<cryptonote::block_complete_entry> &blocks,
                             const std::vector<tools::wallet2::parsed_block> &parsed_blocks)
  {
    uint64_t blocks_added = 0;
    wallet.process_parsed_blocks(0, blocks, parsed_blocks, blocks_added);
  }
};

namespace
{
  struct replay_callback : tools::i_wallet2_callback
  {
    std::vector<std::string> events;

    void on_replay_block(uint64_t height, const cryptonote::block &block,
                         const std::vector<cryptonote::transaction> &transactions) override
    {
      std::string payload;
      for (const auto &transaction : transactions)
      {
        if (!payload.empty())
          payload += ",";
        payload += transaction.extra.empty() ? "empty" : std::to_string(transaction.extra.front());
      }
      events.push_back("replay:" + std::to_string(height) + ":" + std::to_string(block.nonce) + ":"
          + (transactions.empty() ? "empty" : payload));
    }

    void on_new_block(uint64_t height, const cryptonote::block &) override
    {
      events.push_back("new:" + std::to_string(height));
    }

    void on_reorg(uint64_t height, uint64_t blocks_detached, size_t) override
    {
      events.push_back("reorg:" + std::to_string(height) + ":" + std::to_string(blocks_detached));
    }
  };

  struct replay_batch
  {
    std::vector<cryptonote::block_complete_entry> blocks;
    std::vector<tools::wallet2::parsed_block> parsed;

    replay_batch(const crypto::hash &genesis, uint8_t first_marker)
    {
      blocks.resize(3);
      parsed.resize(3);
      parsed[0].hash = genesis;
      parsed[0].o_indices.indices.resize(1);

      for (size_t i = 1; i < parsed.size(); ++i)
      {
        const uint8_t marker = first_marker + i - 1;
        parsed[i].hash = crypto::null_hash;
        parsed[i].hash.data[0] = marker;
        parsed[i].block.nonce = marker;
        parsed[i].block.timestamp = 0;
        parsed[i].block.tx_hashes.resize(2);
        parsed[i].block.tx_hashes[0] = crypto::null_hash;
        parsed[i].block.tx_hashes[0].data[0] = marker;
        parsed[i].block.tx_hashes[1] = crypto::null_hash;
        parsed[i].block.tx_hashes[1].data[0] = marker + 40;
        parsed[i].txes.resize(2);
        parsed[i].txes[0].extra = {marker};
        parsed[i].txes[1].extra = {static_cast<uint8_t>(marker + 40)};
        parsed[i].o_indices.indices.resize(3);
        blocks[i].txs.resize(2);
      }
    }
  };
}

TEST(wallet_replay_hook, ordered_once_across_duplicate_reorg_pool_and_rescan)
{
  tools::wallet2 wallet;
  wallet.generate("", epee::wipeable_string("replay-hook"));
  replay_callback callback;
  wallet.callback(&callback);

  const replay_batch initial(wallet_accessor_test::genesis_hash(wallet), 11);
  wallet_accessor_test::process_blocks(wallet, initial.blocks, initial.parsed);
  wallet_accessor_test::process_blocks(wallet, initial.blocks, initial.parsed);
  cryptonote::transaction pool_tx;
  pool_tx.version = 2;
  crypto::hash pool_tx_hash = crypto::null_hash;
  pool_tx_hash.data[0] = 7;
  wallet.process_pool_state({std::make_tuple(pool_tx, pool_tx_hash, false)});
  EXPECT_EQ((std::vector<std::string>{"replay:1:11:11,51", "new:1", "replay:2:12:12,52", "new:2"}), callback.events);

  const replay_batch replacement(wallet_accessor_test::genesis_hash(wallet), 21);
  wallet_accessor_test::process_blocks(wallet, replacement.blocks, replacement.parsed);
  EXPECT_EQ((std::vector<std::string>{"replay:1:11:11,51", "new:1", "replay:2:12:12,52", "new:2",
                                      "reorg:1:2", "replay:1:21:21,61", "new:1",
                                      "replay:2:22:22,62", "new:2"}), callback.events);

  wallet.rescan_blockchain(true, false);
  wallet_accessor_test::process_blocks(wallet, initial.blocks, initial.parsed);
  EXPECT_EQ((std::vector<std::string>{"replay:1:11:11,51", "new:1", "replay:2:12:12,52", "new:2",
                                      "reorg:1:2", "replay:1:21:21,61", "new:1", "replay:2:22:22,62", "new:2",
                                      "replay:1:11:11,51", "new:1", "replay:2:12:12,52", "new:2"}), callback.events);
}

TEST(wallet_replay_hook, scanned_blocks_without_transactions_are_delivered)
{
  tools::wallet2 wallet;
  wallet.generate("", epee::wipeable_string("replay-hook"));
  wallet.set_refresh_type(tools::wallet2::RefreshNoCoinbase);
  replay_callback callback;
  wallet.callback(&callback);

  replay_batch batch(wallet_accessor_test::genesis_hash(wallet), 31);
  for (size_t i = 1; i < batch.parsed.size(); ++i)
  {
    batch.parsed[i].block.timestamp = std::time(nullptr);
    batch.parsed[i].block.tx_hashes.clear();
    batch.parsed[i].txes.clear();
    batch.parsed[i].o_indices.indices.resize(1);
    batch.blocks[i].txs.clear();
  }

  wallet_accessor_test::process_blocks(wallet, batch.blocks, batch.parsed);
  EXPECT_EQ((std::vector<std::string>{"replay:1:31:empty", "new:1", "replay:2:32:empty", "new:2"}), callback.events);
}

TEST(wallet_replay_hook, future_timestamp_does_not_skip_block)
{
  tools::wallet2 wallet;
  wallet.generate("", epee::wipeable_string("replay-hook"));
  cryptonote::block block;
  block.timestamp = std::numeric_limits<uint64_t>::max();
  EXPECT_FALSE(wallet_accessor_test::should_skip_block(wallet, block, 1));
}
