// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
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

#include <Logging/LoggerRef.h>
#include <Transfers/TransfersContainer.h>
#include <Transfers/IObservableImpl.h>
#include <ITransfersSynchronizer.h>

namespace CryptoNote {

class TransfersSubscription : public IObservableImpl <ITransfersObserver, ITransfersSubscription>
{
public:
    TransfersSubscription(const CryptoNote::Currency &currency,
                          Logging::ILogger &logger,
                          const AccountSubscription &sub);

    SynchronizationStart getSyncStart();
    void onBlockchainDetach(uint32_t height);
    void onError(const std::error_code &ec, uint32_t height);
    bool advanceHeight(uint32_t height);
    const AccountKeys &getKeys() const;
    bool addTransaction(const TransactionBlockInfo &blockInfo,
                        const ITransactionReader &tx,
                        const std::vector<TransactionOutputInformationIn> &transfers);

    void deleteUnconfirmedTransaction(const Crypto::Hash& transactionHash);
    void markTransactionConfirmed(const TransactionBlockInfo &block,
                                  const Crypto::Hash &transactionHash,
                                  const std::vector<uint32_t> &globalIndices);

    // ITransfersSubscription
    AccountPublicAddress getAddress() override;
    ITransfersContainer& getContainer() override;

private:
    Logging::LoggerRef logger;
    TransfersContainer transfers;
    AccountSubscription subscription;
};

} // namespace CryptoNote
