// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, Karbo developers
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

#include <crypto/hash.h>
#include <WalletLegacy/IWalletLegacy.h>
#include <WalletLegacy/WalletLegacyEvent.h>
#include <WalletLegacy/WalletUnconfirmedTransactions.h>
#include <ITransfersContainer.h>

namespace CryptoNote {

class ISerializer;

class WalletUserTransactionsCache
{
    typedef std::vector<WalletLegacyTransfer> UserTransfers;
    typedef std::vector<WalletLegacyTransaction> UserTransactions;

public:
    explicit WalletUserTransactionsCache(uint64_t mempoolTxLiveTime = 60 * 60 * 24);

    bool serialize(CryptoNote::ISerializer &serializer);

    uint64_t unconfirmedTransactionsAmount() const;
    uint64_t unconfrimedOutsAmount() const;
    size_t getTransactionCount() const;
    size_t getTransferCount() const;

    TransactionId addNewTransaction(
        uint64_t amount,
        uint64_t fee,
        const std::string &extra,
        const std::vector<WalletLegacyTransfer> &transfers,
        uint64_t unlockTime,
        const std::vector<TransactionMessage> &messages);

    TransactionId addNewTransaction(
        uint64_t amount,
        uint64_t fee,
        const std::string &extra,
        const std::vector<WalletLegacyTransfer> &transfers,
        uint64_t unlockTime);

    void updateTransaction(
        TransactionId transactionId,
        const CryptoNote::Transaction &tx,
        uint64_t amount,
        const std::list<TransactionOutputInformation> &usedOutputs,
        Crypto::SecretKey &tx_key);
    void updateTransactionSendingState(TransactionId transactionId, std::error_code ec);

    std::shared_ptr<WalletLegacyEvent> onTransactionUpdated(
        const TransactionInformation &txInfo,
        int64_t txBalance);
    std::shared_ptr<WalletLegacyEvent> onTransactionDeleted(const Crypto::Hash &transactionHash);

    TransactionId findTransactionByTransferId(TransferId transferId) const;

    bool getTransaction(TransactionId transactionId, WalletLegacyTransaction &transaction) const;
    WalletLegacyTransaction &getTransaction(TransactionId transactionId);
    TransactionId findTransactionByHash(const Crypto::Hash &hash);
    bool getTransfer(TransferId transferId, WalletLegacyTransfer &transfer) const;
    WalletLegacyTransfer& getTransfer(TransferId transferId);

    bool isUsed(const TransactionOutputInformation &out) const;
    void reset();

    std::vector<TransactionId> deleteOutdatedTransactions();

    std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>&paymentIds)const;

    void setConsolidateHeight(uint32_t height, const Crypto::Hash &consolidateTx) {
        m_prevConsolidateHeight = m_consolidateHeight;
        m_prevConsolidateTx = m_consolidateTx;
        m_consolidateHeight = height;
        m_consolidateTx = consolidateTx;
    }
    // used in external serialization
    void setPrevConsolidateHeight(uint32_t height, const Crypto::Hash &consolidateTx) {
        m_prevConsolidateHeight = height;
        m_prevConsolidateTx = consolidateTx;
    }
    uint32_t getConsolidateHeight() const { return m_consolidateHeight; }
    Crypto::Hash getConsolidateTx() const { return m_consolidateTx; }
    // used in external serialization
    uint32_t getPrevConsolidateHeight() const { return m_prevConsolidateHeight; }
    // used in external serialization
    Crypto::Hash getPrevConsolidateTx() const { return m_prevConsolidateTx; }
    void resetConsolidateHeight()
    {
        m_consolidateHeight = m_prevConsolidateHeight;
        m_consolidateTx = m_prevConsolidateTx;
        m_prevConsolidateHeight = 0;
        m_prevConsolidateTx = boost::value_initialized<Crypto::Hash>();
    }
private:
    TransactionId insertTransaction(WalletLegacyTransaction &&Transaction);
    TransferId insertTransfers(const std::vector<WalletLegacyTransfer> &transfers);
    void updateUnconfirmedTransactions();

    using Offset = UserTransactions::size_type;
    using UserPaymentIndex=std::unordered_map<PaymentId,std::vector<Offset>,boost::hash<PaymentId>>;

    void getGoodItems(UserTransactions &transactions, UserTransfers &transfers);
    void getGoodTransaction(TransactionId txId,
        UserTransactions &transactions,
        UserTransfers &transfers);

    void getTransfersByTx(TransactionId id, UserTransfers &transfers);

    void rebuildPaymentsIndex();
    void pushToPaymentsIndex(const PaymentId &paymentId, Offset distance);
    void pushToPaymentsIndexInternal(
        Offset distance,
        const WalletLegacyTransaction &info,
        std::vector<uint8_t> &extra);
    void popFromPaymentsIndex(const PaymentId &paymentId, Offset distance);

private:
    UserTransactions m_transactions;
    UserTransfers m_transfers;
    WalletUnconfirmedTransactions m_unconfirmedTransactions;
    UserPaymentIndex m_paymentsIndex;
    uint32_t m_consolidateHeight;
    Crypto::Hash m_consolidateTx;
    uint32_t m_prevConsolidateHeight;
    Crypto::Hash m_prevConsolidateTx;
};

} // namespace CryptoNote
