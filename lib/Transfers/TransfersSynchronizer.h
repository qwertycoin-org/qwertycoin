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

#include <cstring>
#include <memory>
#include <unordered_map>
#include <Common/ObserverManager.h>
#include <Logging/LoggerRef.h>
#include <Transfers/IBlockchainSynchronizer.h>
#include <Transfers/TypeHelpers.h>
#include <ITransfersSynchronizer.h>

namespace CryptoNote {

class Currency;
class TransfersConsumer;
class INode;

class TransfersSynchronizer : public ITransfersSynchronizer, public IBlockchainConsumerObserver
{
    typedef std::unordered_map<
        Crypto::PublicKey,
        std::unique_ptr<TransfersConsumer>
    > ConsumersContainer;

    typedef Tools::ObserverManager<ITransfersSynchronizerObserver> SubscribersNotifier;

    typedef std::unordered_map<
        Crypto::PublicKey,
        std::unique_ptr<SubscribersNotifier>
    > SubscribersContainer;

public:
    TransfersSynchronizer(const CryptoNote::Currency &currency,
                         Logging::ILogger &logger,
                         IBlockchainSynchronizer &sync,
                         INode &node);
    ~TransfersSynchronizer() override;

    void initTransactionPool(const std::unordered_set<Crypto::Hash> &uncommitedTransactions);

    // ITransfersSynchronizer
    ITransfersSubscription &addSubscription(const AccountSubscription &acc) override;
    bool removeSubscription(const AccountPublicAddress &acc) override;
    void getSubscriptions(std::vector<AccountPublicAddress> &subscriptions) override;
    ITransfersSubscription *getSubscription(const AccountPublicAddress &acc) override;
    std::vector<Crypto::Hash> getViewKeyKnownBlocks(const Crypto::PublicKey &publicViewKey)override;

    void subscribeConsumerNotifications(const Crypto::PublicKey &viewPublicKey,
                                        ITransfersSynchronizerObserver *observer);
    void unsubscribeConsumerNotifications(const Crypto::PublicKey &viewPublicKey,
                                          ITransfersSynchronizerObserver *observer);

    void addPublicKeysSeen(const AccountPublicAddress &acc,
                           const Crypto::Hash &transactionHash,
                           const Crypto::PublicKey &outputKey);
    void markTransactionSafe(const Crypto::Hash &transactionHash);


    // IStreamSerializable
    void save(std::ostream &os) override;
    void load(std::istream &in) override;

private:
    void onBlocksAdded(
        IBlockchainConsumer *consumer,
        const std::vector<Crypto::Hash> &blockHashes) override;
    void onBlockchainDetach(
        IBlockchainConsumer *consumer,
        uint32_t blockIndex) override;
    void onTransactionDeleteBegin(
        IBlockchainConsumer *consumer,
        Crypto::Hash transactionHash) override;
    void onTransactionDeleteEnd(
        IBlockchainConsumer *consumer,
        Crypto::Hash transactionHash) override;
    void onTransactionUpdated(
        IBlockchainConsumer *consumer,
        const Crypto::Hash &transactionHash,
        const std::vector<ITransfersContainer *> &containers) override;

    bool findViewKeyForConsumer(IBlockchainConsumer *consumer, Crypto::PublicKey &viewKey) const;
    SubscribersContainer::const_iterator findSubscriberForConsumer(IBlockchainConsumer *c) const;

private:
    Logging::LoggerRef m_logger;
    ConsumersContainer m_consumers; // map { view public key -> consumer }
    SubscribersContainer m_subscribers;
    IBlockchainSynchronizer &m_sync;
    INode &m_node;
    const CryptoNote::Currency &m_currency;
};

} // namespace CryptoNote
