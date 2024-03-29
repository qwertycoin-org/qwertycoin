// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2023, The Qwertycoin Group.
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

#include <thread>
#include <boost/asio.hpp>
#include <BlockchainExplorer/BlockchainExplorerDataBuilder.h>
#include <Common/ObserverManager.h>
#include <CryptoNoteCore/ICore.h>
#include <CryptoNoteCore/ICoreObserver.h>
#include <CryptoNoteCore/ITransaction.h>
#include <CryptoNoteProtocol/ICryptoNoteProtocolQuery.h>
#include <CryptoNoteProtocol/ICryptoNoteProtocolObserver.h>
#include <INode.h>

namespace CryptoNote {

class core;

class InProcessNode : public INode,
                      public CryptoNote::ICryptoNoteProtocolObserver,
                      public CryptoNote::ICoreObserver
{
public:
    InProcessNode(CryptoNote::ICore &core, CryptoNote::ICryptoNoteProtocolQuery &protocol);
    InProcessNode(const InProcessNode &) = delete;
    InProcessNode(InProcessNode &&) = delete;
    ~InProcessNode() override;

    void init(const Callback &callback) override;
    bool shutdown() override;

    bool addObserver(INodeObserver *observer) override;
    bool removeObserver(INodeObserver *observer) override;

    size_t getPeerCount() const override;
    uint32_t getLastLocalBlockHeight() const override;
    uint32_t getLastKnownBlockHeight() const override;
    uint32_t getLocalBlockCount() const override;
    uint32_t getKnownBlockCount() const override;
    uint32_t getNodeHeight() const override;
    uint64_t getLastLocalBlockTimestamp() const override;
    uint64_t getMinimalFee() const override;
    BlockHeaderInfo getLastLocalBlockHeaderInfo() const override;
    uint32_t getGRBHeight() const override;

    void getNewBlocks(std::vector<Crypto::Hash> &&knownBlockIds,
                      std::vector<CryptoNote::block_complete_entry> &newBlocks,
                      uint32_t &startHeight,
                      const Callback &callback) override;
    void getTransactionOutsGlobalIndices(const Crypto::Hash &transactionHash,
                                         std::vector<uint32_t> &outsGlobalIndices,
                                         const Callback &callback) override;
    void getRandomOutsByAmounts(
        std::vector<uint64_t> &&amounts,
        uint64_t outsCount,
        std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> &result,
        const Callback &callback) override;
    void relayTransaction(const CryptoNote::Transaction &transaction,
                          const Callback &callback) override;
    void queryBlocks(std::vector<Crypto::Hash> &&knownBlockIds,
                     uint64_t timestamp,
                     std::vector<BlockShortEntry> &newBlocks,
                     uint32_t &startHeight,
                     const Callback &callback) override;
    void getPoolSymmetricDifference(std::vector<Crypto::Hash> &&knownPoolTxIds,
                                    Crypto::Hash knownBlockId,
                                    bool &isBcActual,
                                    std::vector<std::unique_ptr<ITransactionReader>> &newTxs,
                                    std::vector<Crypto::Hash> &deletedTxIds,
                                    const Callback &callback) override;
    void getMultisignatureOutputByGlobalIndex(uint64_t amount,
                                              uint32_t gindex,
                                              MultisignatureOutput &out,
                                              const Callback &callback) override;

    void getBlocks(const std::vector<uint32_t> &blockHeights,
                   std::vector<std::vector<BlockDetails>> &blocks,
                   const Callback &callback) override;
    void getBlocks(const std::vector<Crypto::Hash> &blockHashes,
                   std::vector<BlockDetails> &blocks,
                   const Callback &callback) override;
    void getBlocks(uint64_t timestampBegin,
                   uint64_t timestampEnd,
                   uint32_t blocksNumberLimit,
                   std::vector<BlockDetails> &blocks,
                   uint32_t &blocksNumberWithinTimestamps,
                   const Callback &callback) override;
    void getTransactions(const std::vector<Crypto::Hash> &transactionHashes,
                         std::vector<TransactionDetails> &transactions,
                         const Callback &callback) override;
    void getTransactionsByPaymentId(const Crypto::Hash &paymentId,
                                    std::vector<TransactionDetails> &transactions,
                                    const Callback &callback) override;
    void getPoolTransactions(uint64_t timestampBegin,
                             uint64_t timestampEnd,
                             uint32_t transactionsNumberLimit,
                             std::vector<TransactionDetails> &transactions,
                             uint64_t &transactionsNumberWithinTimestamps,
                             const Callback &callback) override;
    void isSynchronized(bool &syncStatus, const Callback &callback) override;

    InProcessNode &operator=(const InProcessNode &) = delete;
    InProcessNode &operator=(InProcessNode &&) = delete;

private:
    void peerCountUpdated(size_t count) override;
    void lastKnownBlockHeightUpdated(uint32_t height) override;
    void blockchainSynchronized(uint32_t topHeight) override;
    void blockchainUpdated() override;
    void poolUpdated() override;

    void updateLastLocalBlockHeaderInfo();
    void resetLastLocalBlockHeaderInfo();
    void getNewBlocksAsync(std::vector<Crypto::Hash> &knownBlockIds,
                           std::vector<CryptoNote::block_complete_entry> &newBlocks,
                           uint32_t &startHeight,
                           const Callback &callback);
    std::error_code doGetNewBlocks(std::vector<Crypto::Hash> &&knownBlockIds,
                                   std::vector<CryptoNote::block_complete_entry> &newBlocks,
                                   uint32_t &startHeight);

    void getTransactionOutsGlobalIndicesAsync(const Crypto::Hash &transactionHash,
                                              std::vector<uint32_t> &outsGlobalIndices,
                                              const Callback &callback);
    std::error_code doGetTransactionOutsGlobalIndices(const Crypto::Hash &transactionHash,
                                                      std::vector<uint32_t> &outsGlobalIndices);

    void getRandomOutsByAmountsAsync(
        std::vector<uint64_t> &amounts,
        uint64_t outsCount,
        std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> &result,
        const Callback &callback);
    std::error_code doGetRandomOutsByAmounts(
        std::vector<uint64_t> &&amounts,
        uint64_t outsCount,
        std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> &result);

    void relayTransactionAsync(const CryptoNote::Transaction &transaction, const Callback &callback);
    std::error_code doRelayTransaction(const CryptoNote::Transaction &transaction);

    void queryBlocksLiteAsync(std::vector<Crypto::Hash> &knownBlockIds,
                              uint64_t timestamp,
                              std::vector<BlockShortEntry> &newBlocks,
                              uint32_t &startHeight,
                              const Callback &callback);
    std::error_code doQueryBlocksLite(std::vector<Crypto::Hash> &&knownBlockIds,
                                      uint64_t timestamp,
                                      std::vector<BlockShortEntry> &newBlocks,
                                      uint32_t &startHeight);

    void getPoolSymmetricDifferenceAsync(std::vector<Crypto::Hash> &&knownPoolTxIds,
                                         Crypto::Hash knownBlockId,
                                         bool &isBcActual,
                                         std::vector<std::unique_ptr<ITransactionReader>> &newTxs,
                                         std::vector<Crypto::Hash> &deletedTxIds,
                                         const Callback &callback);

    void getOutByMSigGIndexAsync(uint64_t amount,
                                 uint32_t gindex,
                                 MultisignatureOutput &out,
                                 const Callback &callback);

    void getBlocksAsync(const std::vector<uint32_t> &blockHeights,
                        std::vector<std::vector<BlockDetails>> &blocks,
                        const Callback &callback);
    std::error_code doGetBlocks(const std::vector<uint32_t> &blockHeights,
                                std::vector<std::vector<BlockDetails>> &blocks);

    void getBlocksAsync(const std::vector<Crypto::Hash> &blockHashes,
                        std::vector<BlockDetails> &blocks,
                        const Callback &callback);
    std::error_code doGetBlocks(const std::vector<Crypto::Hash> &blockHashes,
                                std::vector<BlockDetails> &blocks);

    void getBlocksAsync(uint64_t timestampBegin,
                        uint64_t timestampEnd,
                        uint32_t blocksNumberLimit,
                        std::vector<BlockDetails> &blocks,
                        uint32_t &blocksNumberWithinTimestamps,
                        const Callback &callback);
    std::error_code doGetBlocks(uint64_t timestampBegin,
                                uint64_t timestampEnd,
                                uint32_t blocksNumberLimit,
                                std::vector<BlockDetails> &blocks,
                                uint32_t &blocksNumberWithinTimestamps);

    void getTransactionsAsync(const std::vector<Crypto::Hash> &transactionHashes,
                              std::vector<TransactionDetails> &transactions,
                              const Callback &callback);
    std::error_code doGetTransactions(const std::vector<Crypto::Hash> &transactionHashes,
                                      std::vector<TransactionDetails> &transactions);

    void getPoolTransactionsAsync(uint64_t timestampBegin,
                                  uint64_t timestampEnd,
                                  uint32_t transactionsNumberLimit,
                                  std::vector<TransactionDetails> &transactions,
                                  uint64_t &transactionsNumberWithinTimestamps,
                                  const Callback &callback);
    std::error_code doGetPoolTransactions(uint64_t timestampBegin,
                                          uint64_t timestampEnd,
                                          uint32_t transactionsNumberLimit,
                                          std::vector<TransactionDetails> &transactions,
                                          uint64_t &transactionsNumberWithinTimestamps);

    void getTransactionsByPaymentIdAsync(const Crypto::Hash &paymentId,
                                         std::vector<TransactionDetails> &transactions,
                                         const Callback &callback);
    std::error_code doGetTransactionsByPaymentId(const Crypto::Hash &paymentId,
                                                 std::vector<TransactionDetails> &transactions);

    void isSynchronizedAsync(bool& syncStatus, const Callback& callback);

    void workerFunc();
    bool doShutdown();

    enum State
    {
        NOT_INITIALIZED,
        INITIALIZED
    };

    State state;
    CryptoNote::ICore &core;
    CryptoNote::ICryptoNoteProtocolQuery &protocol;
    Tools::ObserverManager<INodeObserver> observerManager;
    BlockHeaderInfo lastLocalBlockHeaderInfo;

    boost::asio::io_service ioService;
    std::unique_ptr<std::thread> workerThread;
    std::unique_ptr<boost::asio::io_service::work> work;

    BlockchainExplorerDataBuilder blockchainExplorerDataBuilder;

    mutable std::mutex mutex;
};

} // namespace CryptoNote
