// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The BBSCoin Developers
// Copyright (c) 2018, The Karbo Developers
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

#include <unordered_set>
#include <crypto/crypto.h>
#include <Logging/LoggerRef.h>
#include <Transfers/IBlockchainSynchronizer.h>
#include <Transfers/IObservableImpl.h>
#include <Transfers/TransfersSubscription.h>
#include <Transfers/TypeHelpers.h>
#include <ITransfersSynchronizer.h>

namespace CryptoNote {

class INode;

class TransfersConsumer: public IObservableImpl<IBlockchainConsumerObserver, IBlockchainConsumer>
{
    struct PreprocessInfo
    {
        std::unordered_map<Crypto::PublicKey, std::vector<TransactionOutputInformationIn>> outputs;
        std::vector<uint32_t> globalIdxs;
    };

public:
    TransfersConsumer(const CryptoNote::Currency &currency,
                      INode &node,
                      Logging::ILogger &logger,
                      const Crypto::SecretKey &viewSecret);

    ITransfersSubscription& addSubscription(const AccountSubscription &subscription);
    bool removeSubscription(const AccountPublicAddress &address); // true if no subscribers left
    ITransfersSubscription *getSubscription(const AccountPublicAddress &acc);
    void getSubscriptions(std::vector<AccountPublicAddress> &subscriptions);

    void initTransactionPool(const std::unordered_set<Crypto::Hash> &uncommitedTransactions);
    void addPublicKeysSeen(const Crypto::Hash &transactionHash, const Crypto::PublicKey &outputKey);

    // IBlockchainConsumer
    SynchronizationStart getSyncStart() override;
    void onBlockchainDetach(uint32_t height) override;
    bool onNewBlocks(const CompleteBlock *blocks, uint32_t startHeight, uint32_t count) override;
    std::error_code onPoolUpdated(
        const std::vector<std::unique_ptr<ITransactionReader>> &addedTransactions,
        const std::vector<Crypto::Hash> &deletedTransactions) override;
    const std::unordered_set<Crypto::Hash> &getKnownPoolTxIds() const override;

    std::error_code addUnconfirmedTransaction(const ITransactionReader &transaction) override;
    void removeUnconfirmedTransaction(const Crypto::Hash &transactionHash) override;

private:
    template <typename F>
    void forEachSubscription(F action)
    {
        for (const auto &kv : m_subscriptions) {
            action(*kv.second);
        }
    }

    std::error_code preprocessOutputs(
        const TransactionBlockInfo &blockInfo,
        const ITransactionReader &tx,
        PreprocessInfo &info);
    std::error_code processTransaction(
        const TransactionBlockInfo &blockInfo,
        const ITransactionReader &tx);
    void processTransaction(
        const TransactionBlockInfo &blockInfo,
        const ITransactionReader &tx,
        const PreprocessInfo &info);
    void processOutputs(
        const TransactionBlockInfo &blockInfo,
        TransfersSubscription &sub,
        const ITransactionReader &tx,
        const std::vector<TransactionOutputInformationIn> &outputs,
        const std::vector<uint32_t> &globalIdxs,
        bool &contains,
        bool &updated);
    std::error_code createTransfers(
        const AccountKeys &account,
        const TransactionBlockInfo &blockInfo,
        const ITransactionReader &tx,
        const std::vector<uint32_t> &outputs,
        const std::vector<uint32_t> &globalIdxs,
        std::vector<TransactionOutputInformationIn> &transfers);
    std::error_code getGlobalIndices(
        const Crypto::Hash &transactionHash,
        std::vector<uint32_t> &outsGlobalIndices);

    void updateSyncStart();

private:
    SynchronizationStart m_syncStart;
    const Crypto::SecretKey m_viewSecret;
    // map { spend public key -> subscription }
    std::unordered_map<Crypto::PublicKey, std::unique_ptr<TransfersSubscription>> m_subscriptions;
    std::unordered_set<Crypto::PublicKey> m_spendKeys;
    std::unordered_set<Crypto::Hash> m_poolTxs;

    INode &m_node;
    const CryptoNote::Currency &m_currency;
    Logging::LoggerRef m_logger;
};

} // namespace CryptoNote
