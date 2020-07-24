// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <ctime>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <BlockchainExplorer/BlockchainExplorerData.h>
#include <Common/ObserverManager.h>
#include <CryptoNoteCore/Blockchain.h>
#include <CryptoNoteCore/BlockchainMessages.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/ICore.h>
#include <CryptoNoteCore/ICoreObserver.h>
#include <CryptoNoteCore/IMinerHandler.h>
#include <CryptoNoteCore/MessageQueue.h>
#include <CryptoNoteCore/MinerConfig.h>
#include <CryptoNoteCore/TransactionPool.h>
#include <CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h>
#include <Logging/LoggerMessage.h>
#include <P2p/NetNodeCommon.h>
#include <System/Dispatcher.h>

namespace CryptoNote {

  struct core_stat_info;
  class miner;
  class CoreConfig;

class core : public ICore,
             public IMinerHandler,
             public IBlockchainStorageObserver,
             public ITxPoolObserver
{
public:
    core(
        const Currency &currency,
        i_cryptonote_protocol *pprotocol,
        Logging::ILogger &logger,
        bool blockchainIndexesEnabled);
    ~core() override;

    bool on_idle() override;
    bool handle_incoming_tx( // TODO: Deprecated. Should be removed with CryptoNoteProtocolHandler.
        const BinaryArray &tx_blob,
        tx_verification_context &tvc,
        bool keeped_by_block,
        bool loose_check) override;
    bool handle_incoming_block_blob(
        const BinaryArray &block_blob,
        block_verification_context &bvc,
        bool control_miner,
        bool relay_block) override;

    i_cryptonote_protocol *get_protocol() override { return m_pprotocol; }
    const Currency &currency() const { return m_currency; }

    // IMinerHandler
    bool handle_block_found(Block &b) override;
    bool get_block_template(
        Block &b,
        const AccountPublicAddress &adr,
        difficulty_type &diffic,
        uint32_t &height,
        const BinaryArray &ex_nonce) override;
    bool get_difficulty_stat(
        uint32_t height,
        stat_period period,
        uint32_t& block_num,
        uint64_t& avg_solve_time,
        uint64_t& stddev_solve_time,
        uint32_t& outliers_num,
        difficulty_type &avg_diff,
        difficulty_type &min_diff,
        difficulty_type &max_diff) override;

    bool addObserver(ICoreObserver *observer) override;
    bool removeObserver(ICoreObserver *observer) override;

    miner &get_miner() { return *m_miner; }

    static void init_options(boost::program_options::options_description &desc);
    bool init(const CoreConfig &config, const MinerConfig &minerConfig, bool load_existing);
    bool set_genesis_block(const Block &b);
    bool deinit();

    // ICore
    size_t addChain(const std::vector<const IBlock *> &chain) override;
    bool handle_get_objects( // TODO: Deprecated. Should be removed with CryptoNoteProtocolHandler.
        NOTIFY_REQUEST_GET_OBJECTS_request &arg,
        NOTIFY_RESPONSE_GET_OBJECTS_request &rsp) override;
    bool getBackwardBlocksSizes(
        uint32_t fromHeight,
        std::vector<size_t> &sizes,
        size_t count) override;
    bool getBlockSize(const Crypto::Hash &hash, size_t &size) override;
    bool getAlreadyGeneratedCoins(const Crypto::Hash &hash, uint64_t &generatedCoins) override;
    bool getBlockReward(
        uint8_t blockMajorVersion,
        size_t medianSize,
        size_t currentBlockSize,
        uint64_t alreadyGeneratedCoins,
        uint64_t fee,
        uint64_t &reward,
        int64_t &emissionChange,
        uint32_t height,
        uint64_t blockTarget = CryptoNote::parameters::DIFFICULTY_TARGET) override;
    bool scanOutputkeysForIndices(
        const KeyInput &txInToKey,
        std::list<std::pair<Crypto::Hash, size_t>> &outputReferences) override;
    bool getBlockDifficulty(uint32_t height, difficulty_type &difficulty) override;
    bool getBlockCumulativeDifficulty(uint32_t height, difficulty_type &difficulty) override;
    bool getBlockContainingTx(
        const Crypto::Hash &txId,
        Crypto::Hash &blockId,
        uint32_t &blockHeight) override;
    bool getMultisigOutputReference(
        const MultisignatureInput &txInMultisig,
        std::pair<Crypto::Hash, size_t> &output_reference) override;
    bool getGeneratedTransactionsNumber(uint32_t height, uint64_t &generatedTransactions) override;
    bool getOrphanBlocksByHeight(uint32_t height, std::vector<Block> &blocks) override;
    bool getBlocksByTimestamp(
        uint64_t timestampBegin,
        uint64_t timestampEnd,
        uint32_t blocksNumberLimit,
        std::vector<Block> &blocks,
        uint32_t &blocksNumberWithinTimestamps) override;
    bool getPoolTransactionsByTimestamp(
        uint64_t timestampBegin,
        uint64_t timestampEnd,
        uint32_t transactionsNumberLimit,
        std::vector<Transaction> &transactions,
        uint64_t &transactionsNumberWithinTimestamps) override;
    bool getTransactionsByPaymentId(
        const Crypto::Hash &paymentId,
        std::vector<Transaction> &transactions) override;
    std::vector<Crypto::Hash> getTransactionHashesByPaymentId(
        const Crypto::Hash &paymentId) override;
    bool getOutByMSigGIndex(uint64_t amount, uint64_t gindex, MultisignatureOutput &out) override;
    std::unique_ptr<IBlock> getBlock(const Crypto::Hash &blocksId) override;
    bool handleIncomingTransaction(
        const Transaction &tx,
        const Crypto::Hash &txHash,
        size_t blobSize,
        tx_verification_context &tvc,
        bool keptByBlock,
        uint32_t height,
        bool loose_check) override;
    std::error_code executeLocked(const std::function<std::error_code()> &func) override;
    uint64_t getMinimalFeeForHeight(uint32_t height) override;
    uint64_t getMinimalFee() override;
    uint64_t getBlockTimestamp(uint32_t height) override;

    bool addMessageQueue(MessageQueue<BlockchainMessage> &messageQueue) override;
    bool removeMessageQueue(MessageQueue<BlockchainMessage> &messageQueue) override;

    virtual std::time_t getStartTime() const;

    uint32_t get_current_blockchain_height() override;
    uint8_t getCurrentBlockMajorVersion() override;
    uint8_t getBlockMajorVersionForHeight(uint32_t height) override;

    bool fillBlockDetails(const CryptoNote::Block &block, BlockDetails2 &blockDetails);
    bool fillTransactionDetails(
        const Transaction &tx,
        TransactionDetails2 &txRpcInfo,
        uint64_t timestamp = 0);

    static bool getPaymentId(const Transaction &transaction, Crypto::Hash &paymentId);

    bool have_block(const Crypto::Hash &id) override;
    std::vector<Crypto::Hash> buildSparseChain() override;
    std::vector<Crypto::Hash> buildSparseChain(const Crypto::Hash &startBlockId) override;
    void on_synchronized() override;

    void get_blockchain_top(uint32_t &height, Crypto::Hash &top_id) override;
    bool get_blocks(
        uint32_t start_offset,
        uint32_t count,
        std::list<Block> &blocks,
        std::list<Transaction> &txs);
    bool get_blocks(uint32_t start_offset, uint32_t count, std::list<Block> &blocks);

    template<class T, class D, class S>
    bool get_blocks(const T &block_ids, D &blocks, S &missed_bs)
    {
        return m_blockchain.getBlocks(block_ids, blocks, missed_bs);
    }

    bool queryBlocks(
        const std::vector<Crypto::Hash> &block_ids,
        uint64_t timestamp,
        uint32_t &start_height,
        uint32_t &current_height,
        uint32_t &full_offset,
        std::vector<BlockFullInfo> &entries) override;

    bool queryBlocksLite(
        const std::vector<Crypto::Hash> &knownBlockIds,
        uint64_t timestamp,
        uint32_t &resStartHeight,
        uint32_t &resCurrentHeight,
        uint32_t &resFullOffset,
        std::vector<BlockShortInfo> &entries) override;

    bool queryBlocksDetailed(
        const std::vector<Crypto::Hash> &knownBlockHashes,
        uint64_t timestamp,
        uint32_t &startIndex,
        uint32_t &currentIndex,
        uint32_t &fullOffset,
        std::vector<BlockFullInfo> &entries) override;

    Crypto::Hash getBlockIdByHeight(uint32_t height) override;
    void getTransactions(
        const std::vector<Crypto::Hash> &txs_ids,
        std::list<Transaction> &txs,
        std::list<Crypto::Hash> &missed_txs,
        bool checkTxPool = false) override;
    bool getBlockByHash(const Crypto::Hash &h, Block &blk) override;
    bool getBlockHeight(const Crypto::Hash &blockId, uint32_t &blockHeight) override;

    bool get_alternative_blocks(std::list<Block> &blocks);
    size_t get_alternative_blocks_count();

    void set_cryptonote_protocol(i_cryptonote_protocol *pprotocol);
    void set_checkpoints(Checkpoints &&chk_pts);

    std::vector<Transaction> getPoolTransactions() override;
    size_t get_pool_transactions_count();
    size_t get_blockchain_total_transactions();

    std::vector<Crypto::Hash> findBlockchainSupplement(
        const std::vector<Crypto::Hash> &remoteBlockIds,
        size_t maxCount,
        uint32_t &totalBlockCount,
        uint32_t &startBlockIndex) override;

    bool get_stat_info(core_stat_info &st_inf) override;

    bool get_tx_outputs_gindexs(const Crypto::Hash &tx_id, std::vector<uint32_t> &indexs) override;
    Crypto::Hash get_tail_id();
    bool get_random_outs_for_amounts(
        const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request &req,
        COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response &res) override;
    void pause_mining() override;
    void update_block_template_and_resume_mining() override;

    Blockchain &get_blockchain_storage() { return m_blockchain; }

    // debug functions
    void print_blockchain(uint32_t start_index, uint32_t end_index);
    void print_blockchain_index();
    std::string print_pool(bool short_format);
    std::list<CryptoNote::tx_memory_pool::TransactionDetails> getMemoryPool() const;
    void print_blockchain_outs(const std::string &file);
    bool getPoolChanges(
        const Crypto::Hash &tailBlockId,
        const std::vector<Crypto::Hash> &knownTxsIds,
        std::vector<Transaction> &addedTxs,
        std::vector<Crypto::Hash> &deletedTxsIds) override;
    bool getPoolChangesLite(
        const Crypto::Hash &tailBlockId,
        const std::vector<Crypto::Hash> &knownTxsIds,
        std::vector<TransactionPrefixInfo> &addedTxs,
        std::vector<Crypto::Hash> &deletedTxsIds) override;
    void getPoolChanges(
        const std::vector<Crypto::Hash> &knownTxsIds,
        std::vector<Transaction> &addedTxs,
        std::vector<Crypto::Hash> &deletedTxsIds) override;

    void rollbackBlockchain(uint32_t height) override;

    uint64_t getNextBlockDifficulty();
    uint64_t getTotalGeneratedAmount();
    uint8_t getBlockMajorVersionForHeight(uint32_t height) const;
    bool f_getMixin(const Transaction &transaction, uint64_t &mixin);

    bool is_key_image_spent(const Crypto::KeyImage &key_im);

    bool fillTxExtra(const std::vector<uint8_t> &rawExtra, TransactionExtraDetails2 &extraDetails);

    void setBlocksToFind(uint64_t blocksToFind);

private:
    bool add_new_tx(
        const Transaction &tx,
        const Crypto::Hash &tx_hash,
        size_t blob_size,
        tx_verification_context &tvc,
        bool keeped_by_block);
    bool load_state_data();
    bool parse_tx_from_blob(
        Transaction &tx,
        Crypto::Hash &tx_hash,
        Crypto::Hash &tx_prefix_hash,
        const BinaryArray &blob);
    bool handle_incoming_block(
        const Block &b,
        block_verification_context &bvc,
        bool control_miner,
        bool relay_block);

    bool check_tx_syntax(const Transaction &tx);
    // check correct values, amounts and all lightweight checks not related with database
    bool check_tx_semantic(const Transaction &tx, bool keeped_by_block);
    // check if tx already in memory pool or in main blockchain
    bool check_tx_mixin(const Transaction &tx, uint32_t height);
    // check if the mixin is not too large
    bool check_tx_fee(
        const Transaction &tx,
        size_t blobSize,
        tx_verification_context &tvc,
        uint32_t height,
        bool loose_check);
    // check if tx is not sending unmixable outputs
    bool check_tx_unmixable(const Transaction &tx, uint32_t height);

    bool update_miner_block_template();
    bool handle_command_line(const boost::program_options::variables_map &vm);
    bool check_tx_inputs_keyimages_diff(const Transaction &tx);
    void blockchainUpdated() override;
    void txDeletedFromPool() override;
    void poolUpdated();

    bool findStartAndFullOffsets(
        const std::vector<Crypto::Hash> &knownBlockIds,
        uint64_t timestamp,
        uint32_t &startOffset,
        uint32_t &startFullOffset);

    std::vector<Crypto::Hash> findIdsForShortBlocks(uint32_t startOffset, uint32_t startFullOffset);

    size_t median(std::vector<size_t> &v);

private:
    const Currency &m_currency;
    Logging::LoggerRef logger;
    CryptoNote::RealTimeProvider m_timeProvider;
    tx_memory_pool m_mempool;
    Blockchain m_blockchain;
    i_cryptonote_protocol *m_pprotocol;
    std::unique_ptr<miner> m_miner;
    std::string m_config_folder;
    cryptonote_protocol_stub m_protocol_stub;
    std::atomic<bool> m_starter_message_showed;
    Tools::ObserverManager<ICoreObserver> m_observerManager;
    time_t start_time;

    std::atomic<uint64_t> m_blocksFound;
    std::atomic<uint64_t> m_blocksToFind;

    friend class tx_validate_inputs;
};

} // namespace CryptoNote
