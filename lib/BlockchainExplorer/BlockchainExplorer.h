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

#include <atomic>
#include <mutex>
#include <unordered_set>
#include <BlockchainExplorer/BlockchainExplorerErrors.h>
#include <BlockchainExplorer/IBlockchainExplorer.h>
#include <Common/ObserverManager.h>
#include <Logging/LoggerRef.h>
#include <Wallet/WalletAsyncContextCounter.h>
#include "INode.h"

namespace CryptoNote {

class BlockchainExplorer : public IBlockchainExplorer, public INodeObserver
{
    class PoolUpdateGuard
    {
    public:
        PoolUpdateGuard();

        bool beginUpdate();
        bool endUpdate();

    private:
        enum class State
        {
            NONE,
            UPDATING,
            UPDATE_REQUIRED
        };

        std::atomic<State> m_state;
    };

    enum State
    {
        NOT_INITIALIZED,
        INITIALIZED
    };
public:
    BlockchainExplorer(INode &node, Logging::ILogger &logger);
    BlockchainExplorer(const BlockchainExplorer &) = delete;
    BlockchainExplorer(BlockchainExplorer &&) = delete;
    ~BlockchainExplorer() override = default;

    void init() override;
    void shutdown() override;

    bool addObserver(IBlockchainObserver *observer) override;
    bool removeObserver(IBlockchainObserver *observer) override;

    bool getBlocks(const std::vector<uint32_t> &blockHeights,
                   std::vector<std::vector<BlockDetails>> &blocks) override;
    bool getBlocks(const std::vector<Crypto::Hash> &blockHashes,
                   std::vector<BlockDetails> &blocks) override;
    bool getBlocks(uint64_t timestampBegin,
                   uint64_t timestampEnd,
                   uint32_t blocksNumberLimit,
                   std::vector<BlockDetails> &blocks,
                   uint32_t &blocksNumberWithinTimestamps) override;

    bool getBlockchainTop(BlockDetails &topBlock) override;

    bool getPoolState(const std::vector<Crypto::Hash> &knownPoolTransactionHashes,
                      Crypto::Hash knownBlockchainTop,
                      bool &isBlockchainActual,
                      std::vector<TransactionDetails> &newTransactions,
                      std::vector<Crypto::Hash> &removedTransactions) override;
    bool getPoolTransactions(uint64_t timestampBegin,
                             uint64_t timestampEnd,
                             uint32_t transactionsNumberLimit,
                             std::vector<TransactionDetails> &transactions,
                             uint64_t &transactionsNumberWithinTimestamps) override;
    bool getTransactions(const std::vector<Crypto::Hash> &transactionHashes,
                         std::vector<TransactionDetails> &transactions) override;
    bool getTransactionsByPaymentId(const Crypto::Hash &paymentId,
                                    std::vector<TransactionDetails> &transactions) override;

    uint64_t getRewardBlocksWindow() override;
    uint64_t getFullRewardMaxBlockSize(uint8_t majorVersion) override;

    bool isSynchronized() override;

    void poolChanged() override;
    void blockchainSynchronized(uint32_t topHeight) override;
    void localBlockchainUpdated(uint32_t height) override;

    BlockchainExplorer &operator=(const BlockchainExplorer &) = delete;
    BlockchainExplorer &operator=(BlockchainExplorer &&) = delete;

private:
    void poolUpdateEndHandler();

private:
    BlockDetails knownBlockchainTop;
    uint32_t knownBlockchainTopHeight;
    std::unordered_set<Crypto::Hash> knownPoolState;

    std::atomic<State> state;
    std::atomic<bool> synchronized;
    std::atomic<uint32_t> observersCounter;
    Tools::ObserverManager<IBlockchainObserver> observerManager;

    std::mutex mutex;

    INode &node;
    Logging::LoggerRef logger;

    WalletAsyncContextCounter asyncContextCounter;
    PoolUpdateGuard poolUpdateGuard;
};

} // namespace CryptoNote
