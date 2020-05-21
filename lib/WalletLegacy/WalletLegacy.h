// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016, The Monero Project
// Copyright (c) 2017-2018, Karbo developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <Common/ObserverManager.h>
#include <CryptoNoteCore/TransactionExtra.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/Currency.h>
#include <Transfers/BlockchainSynchronizer.h>
#include <Transfers/TransfersSynchronizer.h>
#include <Wallet/WalletAsyncContextCounter.h>
#include <Wallet/WalletErrors.h>
#include <WalletLegacy/IWalletLegacy.h>
#include <WalletLegacy/WalletRequest.h>
#include <WalletLegacy/WalletTransactionSender.h>
#include <WalletLegacy/WalletUserTransactionsCache.h>
#include <WalletLegacy/WalletUnconfirmedTransactions.h>
#include <CryptoNote.h>
#include <INode.h>

namespace CryptoNote {

class SyncStarter;

class WalletLegacy : public IWalletLegacy, IBlockchainSynchronizerObserver, ITransfersObserver
{
    enum WalletState
    {
        NOT_INITIALIZED = 0,
        INITIALIZED,
        LOADING,
        SAVING
    };

public:
    WalletLegacy(const CryptoNote::Currency &currency, INode &node, Logging::ILogger &loggerGroup);
    ~WalletLegacy() override;

    void addObserver(IWalletLegacyObserver *observer) override;
    void removeObserver(IWalletLegacyObserver *observer) override;

    void initAndGenerate(const std::string &password) override;
    void initAndGenerateDeterministic(const std::string &password) override;
    void initAndLoad(std::istream &source, const std::string &password) override;
    void initWithKeys(const AccountKeys &accountKeys, const std::string &password) override;
    void shutdown() override;
    void reset() override;
    void purge() override;

    Crypto::SecretKey generateKey(
        const std::string &password,
        const Crypto::SecretKey &recovery_param = Crypto::SecretKey(),
        bool recover = false,
        bool two_random = false) override;

    void save(std::ostream &destination, bool saveDetailed = true, bool saveCache = true) override;

    std::error_code changePassword(
        const std::string &oldPassword,
        const std::string &newPassword) override;

    std::string getAddress() override;

    uint64_t actualBalance() override;
    uint64_t pendingBalance() override;
    uint64_t dustBalance() override;

    size_t getTransactionCount() override;
    size_t getTransferCount() override;
    size_t getUnlockedOutputsCount() override;

    TransactionId findTransactionByTransferId(TransferId transferId) override;

    bool getTransaction(TransactionId transactionId, WalletLegacyTransaction &transaction) override;
    bool getTransfer(TransferId transferId, WalletLegacyTransfer &transfer) override;
    std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId> &paymentIds) const override;
    bool getTxProof(
        Crypto::Hash &txid,
        CryptoNote::AccountPublicAddress &address,
        Crypto::SecretKey &tx_key,
        std::string &sig_str) override;
    std::string getReserveProof(const uint64_t &reserve, const std::string &message) override;
    Crypto::SecretKey getTxKey(Crypto::Hash &txid) override;
    bool get_tx_key(Crypto::Hash &txid, Crypto::SecretKey &txSecretKey) override;
    void getAccountKeys(AccountKeys &keys) override;
    bool getSeed(std::string &electrum_words) override;

    TransactionId sendTransaction(
        const WalletLegacyTransfer &transfer,
        uint64_t fee,
        const std::string &extra = "",
        uint64_t mixIn = 0,
        uint64_t unlockTimestamp = 0,
        const std::vector<TransactionMessage> &messages = std::vector<TransactionMessage>(),
        uint64_t ttl = 0,
        const std::string &sender = "") override;
    TransactionId sendTransaction(
        const std::vector<WalletLegacyTransfer> &transfers,
        uint64_t fee,
        const std::string &extra = "",
        uint64_t mixIn = 0,
        uint64_t unlockTimestamp = 0,
        const std::vector<TransactionMessage> &messages = std::vector<TransactionMessage>(),
        uint64_t ttl = 0,
        const std::string &sender = "") override;

    TransactionId sendDustTransaction(
        const std::vector<WalletLegacyTransfer> &transfers,
        uint64_t fee,
        const std::string &extra = "",
        uint64_t mixIn = 0,
        uint64_t unlockTimestamp = 0) override;
    TransactionId sendFusionTransaction(
        const std::list<TransactionOutputInformation> &fusionInputs,
        uint64_t fee,
        const std::string &extra = "",
        uint64_t mixIn = 0,
        uint64_t unlockTimestamp = 0) override;
    std::error_code cancelTransaction(size_t transactionId) override;

    size_t estimateFusion(const uint64_t &threshold) override;
    std::list<TransactionOutputInformation> selectFusionTransfersToSend(
        uint64_t threshold,
        size_t minInputCount,
        size_t maxInputCount) override;

    std::string sign_message(const std::string &data) override;
    bool verify_message(const std::string &data,
                        const CryptoNote::AccountPublicAddress &address,
                        const std::string &signature) override;

    bool isTrackingWallet() override;

private:
    // IBlockchainSynchronizerObserver
    void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;
    void synchronizationCompleted(std::error_code result) override;

    // ITransfersObserver
    void onTransactionUpdated(
        ITransfersSubscription *object,
        const Crypto::Hash &transactionHash) override;
    void onTransactionDeleted(
        ITransfersSubscription *object,
        const Crypto::Hash &transactionHash) override;

    void initSync();
    void throwIfNotInitialised();

    void doSave(std::ostream &destination, bool saveDetailed, bool saveCache);
    void doLoad(std::istream &source);

    void synchronizationCallback(WalletRequest::Callback callback, std::error_code ec);
    void sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec);
    void notifyClients(std::deque<std::shared_ptr<WalletLegacyEvent>> &events);
    void notifyIfBalanceChanged();

    std::vector<TransactionId> deleteOutdatedUnconfirmedTransactions();

private:
    WalletState m_state;
    std::mutex m_cacheMutex;
    CryptoNote::AccountBase m_account;
    std::string m_password;
    const CryptoNote::Currency &m_currency;
    INode &m_node;
    Logging::ILogger &m_loggerGroup;
    bool m_isStopping;

    std::atomic<uint64_t> m_lastNotifiedActualBalance;
    std::atomic<uint64_t> m_lastNotifiedPendingBalance;
    std::atomic<uint64_t> m_lastNotifiedUnmixableBalance;

    BlockchainSynchronizer m_blockchainSync;
    TransfersSyncronizer m_transfersSync;
    ITransfersContainer *m_transferDetails;

    WalletUserTransactionsCache m_transactionsCache;
    std::unique_ptr<WalletTransactionSender> m_sender;

    WalletAsyncContextCounter m_asyncContextCounter;
    Tools::ObserverManager<CryptoNote::IWalletLegacyObserver> m_observerManager;

    std::unique_ptr<SyncStarter> m_onInitSyncStarter;
};

} // namespace CryptoNote
