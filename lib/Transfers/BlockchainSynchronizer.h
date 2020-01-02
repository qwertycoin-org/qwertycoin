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
#include <condition_variable>
#include <future>
#include <mutex>
#include <Logging/LoggerRef.h>
#include <Transfers/IBlockchainSynchronizer.h>
#include <Transfers/IObservableImpl.h>
#include <Transfers/SynchronizationState.h>
#include <INode.h>
#include <IStreamSerializable.h>

namespace CryptoNote {

class BlockchainSynchronizer : public IObservableImpl<
                                   IBlockchainSynchronizerObserver,
                                   IBlockchainSynchronizer>,
                               public INodeObserver
{
    struct GetBlocksResponse
    {
        uint32_t startHeight;
        std::vector<BlockShortEntry> newBlocks;
    };

    struct GetBlocksRequest
    {
        GetBlocksRequest()
        {
            syncStart.timestamp = 0;
            syncStart.height = 0;
        }

        SynchronizationStart syncStart;
        std::vector<Crypto::Hash> knownBlocks;
    };

    struct GetPoolResponse
    {
        bool isLastKnownBlockActual;
        std::vector<std::unique_ptr<ITransactionReader>> newTxs;
        std::vector<Crypto::Hash> deletedTxIds;
    };

    struct GetPoolRequest
    {
        std::vector<Crypto::Hash> knownTxIds;
        Crypto::Hash lastKnownBlock;
    };

    // prioritized finite states
    enum class State
    {
        // WARNING: DO NOT REORDER!!!
        idle = 0,
        poolSync = 1,
        blockchainSync = 2,
        deleteOldTxs = 3,
        stopped = 4
    };

    enum class UpdateConsumersResult
    {
        nothingChanged = 0,
        addedNewBlocks = 1,
        errorOccurred = 2
    };

    typedef std::map<IBlockchainConsumer *, std::shared_ptr<SynchronizationState>> ConsumersMap;

public:
    BlockchainSynchronizer(INode &node,
                           Logging::ILogger &logger,
                           const Crypto::Hash &genesisBlockHash);
    ~BlockchainSynchronizer() override;

    // IBlockchainSynchronizer
    void addConsumer(IBlockchainConsumer *consumer) override;
    bool removeConsumer(IBlockchainConsumer *consumer) override;
    IStreamSerializable* getConsumerState(IBlockchainConsumer *consumer) const override;
    std::vector<Crypto::Hash> getConsumerKnownBlocks(IBlockchainConsumer &consumer) const override;

    std::future<std::error_code> addUnconfirmedTransaction(const ITransactionReader &transaction) override;
    std::future<void> removeUnconfirmedTransaction(const Crypto::Hash &transactionHash) override;

    void start() override;
    void stop() override;

    // IStreamSerializable
    void save(std::ostream &os) override;
    void load(std::istream &in) override;

    // INodeObserver
    void localBlockchainUpdated(uint32_t height) override;
    void lastKnownBlockHeightUpdated(uint32_t height) override;
    void poolChanged() override;

private:
    void removeOutdatedTransactions();
    void startPoolSync();
    void startBlockchainSync();

    void processBlocks(GetBlocksResponse &response);
    UpdateConsumersResult updateConsumers(const BlockchainInterval &interval,
                                          const std::vector<CompleteBlock> &blocks);
    std::error_code processPoolTxs(GetPoolResponse &response);
    std::error_code getPoolSymmetricDifferenceSync(GetPoolRequest &&request,
                                                   GetPoolResponse &response);
    std::error_code doAddUnconfirmedTransaction(const ITransactionReader &transaction);
    void doRemoveUnconfirmedTransaction(const Crypto::Hash &transactionHash);

    // Second parameter is used only in case of errors returned into callback from INode,
    // such as aborted or connection lost.
    bool setFutureState(State s);
    bool setFutureStateIf(State s, std::function<bool(void)> &&pred);

    void actualizeFutureState();
    bool checkIfShouldStop() const;
    bool checkIfStopped() const;

    void workingProcedure();

    GetBlocksRequest getCommonHistory();
    void getPoolUnionAndIntersection(std::unordered_set<Crypto::Hash> &poolUnion,
                                     std::unordered_set<Crypto::Hash> &poolIntersection) const;
    SynchronizationState *getConsumerSynchronizationState(IBlockchainConsumer *consumer) const;

private:
    mutable Logging::LoggerRef m_logger;
    ConsumersMap m_consumers;
    INode &m_node;
    const Crypto::Hash m_genesisBlockHash;

    Crypto::Hash lastBlockId;

    State m_currentState;
    State m_futureState;
    std::unique_ptr<std::thread> workingThread;
    std::list<std::pair<const ITransactionReader *, std::promise<std::error_code>>> m_addTransactionTasks;
    std::list<std::pair<const Crypto::Hash *, std::promise<void>>> m_removeTransactionTasks;

    mutable std::mutex m_consumersMutex;
    mutable std::mutex m_stateMutex;
    std::condition_variable m_hasWork;
};

} // namespace CryptoNote
