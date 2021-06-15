// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2021, The Qwertycoin Group.
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

#include <set>
#include <unordered_map>
#include <unordered_set>

#include <boost/utility.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <Common/int-util.h>
#include <Common/ObserverManager.h>
#include <Common/Util.h>

#include <crypto/hash.h>

#include <CryptoNoteCore/Blockchain.h>
#include <CryptoNoteCore/BlockchainIndices.h>
#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/ICore.h>
#include <CryptoNoteCore/ITimeProvider.h>
#include <CryptoNoteCore/ITransactionValidator.h>
#include <CryptoNoteCore/ITxPoolObserver.h>
#include <CryptoNoteCore/VerificationContext.h>

#include <Logging/LoggerRef.h>

namespace CryptoNote {

class ISerializer;
class Blockchain;

class OnceInTimeInterval
{
public:
    OnceInTimeInterval(unsigned interval, CryptoNote::ITimeProvider &timeProvider)
        : m_interval(interval),
          m_timeProvider(timeProvider)
    {
        m_lastWorkedTime = 0;
    }

    template<class functor_t>
    bool call(functor_t functr)
    {
        time_t now = m_timeProvider.now();

        if (now - m_lastWorkedTime > m_interval) {
            bool res = functr();
            m_lastWorkedTime = m_timeProvider.now();
            return res;
        }

        return true;
    }

private:
    time_t m_lastWorkedTime;
    unsigned m_interval;
    CryptoNote::ITimeProvider &m_timeProvider;
};

using CryptoNote::BlockInfo;
using namespace boost::multi_index;

    class TxMemoryPool : boost::noncopyable {
    public:
        TxMemoryPool(
                const CryptoNote::Currency &sCurrency,
                CryptoNote::Blockchain &sBlockchain,
                CryptoNote::ICore &sCore,
                CryptoNote::ITimeProvider &sTimeProvider,
                Logging::ILogger &sLog,
                bool bBlockchainIndexesEnabled);

        bool addObserver(ITxPoolObserver *observer);

        bool removeObserver(ITxPoolObserver *observer);

        // load/store operations
        bool init(const std::string &config_folder);

        bool deinit();

        bool haveTransaction(const Crypto::Hash &sTxHash) const;
        bool addTransaction(const Transaction &sTransaction,
                            tx_verification_context &tvc,
                            bool bKeepedByBlock);
        bool addTransaction(const Transaction &sTransaction,
                             const Crypto::Hash &sTxHash,
                             uint64_t uBlobSize,
                             tx_verification_context &tvc,
                             bool bKeepedByBlock);
        bool takeTransaction(const Crypto::Hash &sTxHash,
                              Transaction &sTransaction,
                              uint64_t &uBlobSize,
                              uint64_t &uFee);

        bool onBlockchainIncrement(uint64_t uNewBlockHeight,
                                   const Crypto::Hash &sTopBlockId);
        bool onBlockchainDecrement(uint64_t uNewBlockHeight,
                                   const Crypto::Hash &sTopBlockId);

        void lock() const;
        void unlock() const;
        std::unique_lock<std::recursive_mutex> obtainGuard() const;

        bool fillBlockTemplate(Block &sBlock,
                               uint64_t uMedianSize,
                               uint64_t uMaxCumulativeSize,
                               uint64_t uAlreadyGeneratedCoins,
                               uint64_t &uTotalSize,
                               uint64_t &uFee);

        void getTransactions(std::list<Transaction> &lTransactions) const;
        void getDifference(const std::vector<Crypto::Hash> &vKnownTxHashes,
                           std::vector<Crypto::Hash> &vNewTxHashes,
                           std::vector<Crypto::Hash> &vDeletedTxHashes) const;
        uint64_t getTransactionsCount() const;
        std::string printTransactionPool(bool bShortFormat) const;

        void onIdle();

        bool getTransactionHashesByPaymentId(const Crypto::Hash &sPaymentId,
                                             std::vector<Crypto::Hash> &vTransactionHashes);
        bool getTransactionHashesByTimestamp(uint64_t uTimestampBegin,
                                             uint64_t uTimestampEnd,
                                             uint32_t uTransactionsLimit,
                                             std::vector<Crypto::Hash> &vHashes,
                                             uint64_t &uTransactionsNumberWithinTimestamps);

        template<class I, class T, class M>
        void getTransactions(const I &vTxHashes, T &vTransactions, M &vMissedTxs)
        {
            std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);
            bool bIsLMDB = Tools::getDefaultDBType("lmdb");

            for (const auto &sId : vTxHashes) {
                std::cout << "TxMemoryPool::"<<__func__<< ". sId: " << sId << ENDL;

                if (bIsLMDB) {

                } else {
                    auto sIt = mTransactions.find(sId);
                    if (sIt == mTransactions.end()) {
                        std::cout << "TxMemoryPool::"<<__func__<< "couldn't find tx with hash " << sId << "in mempool" << ENDL;
                        vMissedTxs.push_back(sId);
                    } else {
                        std::cout << "TxMemoryPool::"<<__func__<< "found tx with hash " << sId << "in mempool" << ENDL;
                        vTransactions.push_back(sIt->sTransaction);
                    }
                }

            }
        }

        void serialize(ISerializer &s);
        void getTransactions(std::list<Transaction> &lTransactions,
                             bool bIncludeUnrelayedTransactions) const;
        void getTransactions(const std::vector<Crypto::Hash> &vTxHashes,
                             std::vector<CryptoNote::Transaction> &vTransactions,
                             std::vector<Crypto::Hash> &vMissedTxs);

        struct FTransactionCheckInfo
        {
            BlockInfo maxUsedBlock;
            BlockInfo lastFailedBlock;
        };

        struct FTransactionDetails : public FTransactionCheckInfo
        {
            Crypto::Hash sTransactionHash;
            Transaction sTransaction;
            uint64_t uBlobSize;
            uint64_t uFee;
            bool bKeepedByBlock;
            time_t sReceiveTime;
        };

        void getMemoryPool(std::list<CryptoNote::TxMemoryPool::FTransactionDetails> lTxD) const;
        std::list<CryptoNote::TxMemoryPool::FTransactionDetails> getMemoryPool() const;

    private:
        struct FTransactionPriorityComparator
        {
            bool operator()(const FTransactionDetails &sLhs, const FTransactionDetails &sRhs) const
            {
                uint64_t lhs_hi, lhs_lo = mul128(sLhs.uFee, sRhs.uBlobSize, &lhs_hi);
                uint64_t rhs_hi, rhs_lo = mul128(sRhs.uFee, sLhs.uBlobSize, &rhs_hi);

                return
                        // prefer more profitable transactions
                        (lhs_hi > rhs_hi) || (lhs_hi == rhs_hi && lhs_lo > rhs_lo) ||
                        // prefer smaller
                        (lhs_hi == rhs_hi && lhs_lo == rhs_lo && sLhs.uBlobSize < sRhs.uBlobSize) ||
                        // prefer older
                        (lhs_hi == rhs_hi && lhs_lo == rhs_lo && sLhs.uBlobSize == sRhs.uBlobSize
                         && sLhs.sReceiveTime < sRhs.sReceiveTime);
            }
        };

        typedef hashed_unique<BOOST_MULTI_INDEX_MEMBER(FTransactionDetails, Crypto::Hash,
                                                       sTransactionHash)> mMainIndexT;
        typedef ordered_non_unique<identity<FTransactionDetails>,
                FTransactionPriorityComparator> mFeeIndexT;
        typedef multi_index_container<FTransactionDetails,
                                     indexed_by<mMainIndexT, mFeeIndexT>> mTransactionContainerT;
        typedef std::pair<uint64_t, uint64_t> mGlobalOutputT;
        typedef std::set<mGlobalOutputT> mGlobalOutputsContainerT;
        typedef std::unordered_map<Crypto::KeyImage,
                                   std::unordered_set<Crypto::Hash>> mKeyImagesContainerT;

        bool addTransactionInputs(const Crypto::Hash &sTxHash,
                                  const Transaction &sTransaction,
                                  bool bKeptByBlock);
        bool haveSpentInputs(const Transaction &tx) const;
        bool removeTransactionInputs(const Crypto::Hash &sTxHash,
                                     const Transaction &sTransaction,
                                     bool bKeptByBlock);

        mTransactionContainerT::iterator removeTransaction(mTransactionContainerT::iterator sIt);
        bool removeExpiredTransactions();
        bool isTranscationReadyToGo(const Transaction &sTransaction,
                                    FTransactionCheckInfo &sTxCheckInfo) const;

        void buildIndices();

        Tools::ObserverManager<ITxPoolObserver> mObserverManager;
        const CryptoNote::Currency &mCurrency;
        CryptoNote::ICore &mCore;
        OnceInTimeInterval mTxCheckInterval;
        mutable std::recursive_mutex mTransactionsLock;
        mKeyImagesContainerT mSpentKeyImages;
        mGlobalOutputsContainerT mSpentOutputs;

        std::string mConfigFolder;
        Blockchain &mBlockchain;
        CryptoNote::ITimeProvider &mTimeProvider;

        mTransactionContainerT mTransactions;
        mTransactionContainerT::nth_index<1>::type &mFeeIndex;
        std::unordered_map<Crypto::Hash, uint64_t> mRecentlyDeletedTransactions;

        Logging::LoggerRef mLogger;
        PaymentIdIndex mPaymentIndex;
        TimestampTransactionsIndex mTimestampIndex;
        std::unordered_map<Crypto::Hash, uint64_t> mTimeToLifeIndex;
    };
} // namespace CryptoNote


