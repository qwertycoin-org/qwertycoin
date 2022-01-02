// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2021, The Qwertycoin Group.
// Copyright (c) 2016, The Forknote developers
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

#include <algorithm>
#include <ctime>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/filesystem.hpp>

#include <crypto/hash.h>

#include <Common/int-util.h>
#include <Common/Util.h>

#include <CryptoNoteCore/Blockchain.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/TransactionExtra.h>
#include <CryptoNoteCore/TransactionPool.h>

#include <Global/Constants.h>
#include <Global/CryptoNoteConfig.h>

#include <Lmdb/Structures.h>

#include <Serialization/SerializationTools.h>
#include <Serialization/BinarySerializationTools.h>


#define CURRENT_MEMPOOL_ARCHIVE_VER 1

#undef ERROR

using namespace Logging;
using namespace Qwertycoin;

namespace CryptoNote {

class BlockTemplate
{
public:
    bool addTransaction(const Crypto::Hash &txid, const Transaction &tx)
    {
        if (!canAdd(tx)) {
            return false;
        }

        for (const auto &in : tx.inputs) {
            if (in.type() == typeid(KeyInput)) {
                auto r = m_keyImages.insert(boost::get<KeyInput>(in).keyImage);
                (void)r; // just to make compiler to shut up
                assert(r.second);
            } else if (in.type() == typeid(MultisignatureInput)) {
                const auto &msig = boost::get<MultisignatureInput>(in);
                auto r = m_usedOutputs.insert(std::make_pair(msig.amount, msig.outputIndex));
                (void)r; // just to make compiler to shut up
                assert(r.second);
            }
        }

        m_txHashes.push_back(txid);

        return true;
    }

    const std::vector<Crypto::Hash> &getTransactions() const
    {
        return m_txHashes;
    }

private:
    bool canAdd(const Transaction &tx)
    {
        for (const auto &in : tx.inputs) {
            if (in.type() == typeid(KeyInput)) {
                if (m_keyImages.count(boost::get<KeyInput>(in).keyImage)) {
                    return false;
                }
            } else if (in.type() == typeid(MultisignatureInput)) {
                const auto &msig = boost::get<MultisignatureInput>(in);
                if (m_usedOutputs.count(std::make_pair(msig.amount, msig.outputIndex))) {
                    return false;
                }
            }
        }

        return true;
    }

private:
    std::unordered_set<Crypto::KeyImage> m_keyImages;
    std::set<std::pair<uint64_t, uint64_t>> m_usedOutputs;
    std::vector<Crypto::Hash> m_txHashes;
};

using CryptoNote::BlockInfo;

std::unordered_set<Crypto::Hash> m_validated_transactions;
std::unordered_set<Crypto::Hash> mValidatedTransactions;

    TxMemoryPool::TxMemoryPool(const CryptoNote::Currency &sCurrency,
                               CryptoNote::Blockchain &sChain,
                               CryptoNote::ICore &sCore,
                               CryptoNote::ITimeProvider &sTimeProvider,
                               Logging::ILogger &sLog,
                               bool bBlockchainIndexesEnabled)
            : mCurrency(sCurrency),
              mBlockchain(sChain),
              mCore(sCore),
              mTimeProvider(sTimeProvider),
              mTxCheckInterval(60, sTimeProvider),
              mFeeIndex(boost::get<1>(mTransactions)),
              mLogger(sLog, "TxMemoryPool"),
              mPaymentIndex(bBlockchainIndexesEnabled),
              mTimestampIndex(bBlockchainIndexesEnabled)
    {
    }

    bool TxMemoryPool::addTransaction(const Transaction &sTransaction,
                                      const Crypto::Hash &sTxHash,
                                      uint64_t uBlobSize,
                                      tx_verification_context &tvc,
                                      bool bKeepedByBlock)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << "L137";
        if (!check_inputs_types_supported(sTransaction)) {
            tvc.m_verification_failed = true;
            return false;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " check_inputs_types_supported successfully";

        uint64_t uInputsAmount = 0;
        uint64_t uOutputsAmount = get_outs_money_amount(sTransaction);

        if (!get_inputs_money_amount(sTransaction, uInputsAmount)) {
            mLogger(ERROR, BRIGHT_RED)
                    << "Failed to get inputs amount of transaction with hash: "
                    << getObjectHash(sTransaction);
            tvc.m_verification_failed = true;
            return false;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " get_inputs_money_amount successfully";

        if (uOutputsAmount > uInputsAmount) {
            mLogger(ERROR, BRIGHT_RED) << "transaction use more money then it has: use "
                                       << mCurrency.formatAmount(uOutputsAmount) << ", have "
                                       << mCurrency.formatAmount(uInputsAmount);
            tvc.m_verification_failed = true;
            return false;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " !uOutputsAmount > uInputsAmount";

        std::vector<TransactionExtraField> vTxExtraFields;
        parseTransactionExtra(sTransaction.extra, vTxExtraFields);
        TransactionExtraTTL sTtl;
        if (!findTransactionExtraFieldByType(vTxExtraFields, sTtl)) {
            sTtl.ttl = 0;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " findTransactionExtraFieldByType successfully";

        const uint64_t uFee = uInputsAmount - uOutputsAmount;
        bool bIsFusionTransaction = uFee == 0 && mCurrency.isFusionTransaction(sTransaction,
                                                                               uBlobSize,
                                                                               mCore.get_current_blockchain_height());

        if (sTtl.ttl != 0 && bKeepedByBlock) {
            uint64_t uNow = static_cast<uint64_t>(time(nullptr));
            if (sTtl.ttl <= uNow) {
                mLogger(WARNING, BRIGHT_YELLOW)
                        << "Transaction TTL has already expired: tx = "
                        << sTxHash << ", ttl = " << sTtl.ttl;
                tvc.m_verification_failed = true;
                return false;
            } else if (sTtl.ttl - uNow > mCurrency.mempoolTxLiveTime() +
                                         mCurrency.blockFutureTimeLimit()) {
                mLogger(WARNING, BRIGHT_YELLOW)
                        << "Transaction TTL is out of range: tx = " << sTxHash
                        << ",ttl = " << sTtl.ttl;
                tvc.m_verification_failed = true;
                return false;
            }

            if (uFee != 0) {
                mLogger(WARNING, BRIGHT_YELLOW)
                        << "Transaction with TTL has non-zero fee: tx = " << sTxHash
                        << ", fee = " << mCurrency.formatAmount(uFee);
                tvc.m_verification_failed = true;
                return false;
            }
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " sTtl.ttl != 0 && bKeepedByBlock";

        TransactionExtraMergeMiningTag sMmTag;
        if (getMergeMiningTagFromExtra(sTransaction.extra, sMmTag)) {
            mLogger(ERROR, BRIGHT_RED) << "Merge mining tag was found in extra of transaction";
            tvc.m_verification_failed = true;
            return false;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " !getMergeMiningTagFromExtra";

        if (!bKeepedByBlock) {
            std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);
            if (haveSpentInputs(sTransaction)) {
                mLogger(ERROR, BRIGHT_RED)
                        << "Transaction with id= " << sTxHash
                        << " used already spent inputs";
                tvc.m_verification_failed = true;
                return false;
            }
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " !bKeepedByBlock 0";

        BlockInfo sMaxUsedBlock;

        // Check inputs
        bool bInputsValid = mBlockchain.checkTransactionInputs(sTransaction, sMaxUsedBlock);
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        FTxPoolMeta sMeta;

        if (!bInputsValid) {
            if (!bKeepedByBlock) {
                mLogger(INFO) << "tx used wrong inputs, rejected";
                tvc.m_verification_failed = true;

                return false;
            } else if (bIsLMDB && bKeepedByBlock) {
                sMeta.uBlobSize = uBlobSize;
                sMeta.uFee = uFee;
                sMeta.uMaxUsedBlockID = NULL_HASH;
                sMeta.uMaxUsedBlockHeight = 0;
                sMeta.uLastFailedHeight = 0;
                sMeta.uLastFailedID = NULL_HASH;
                sMeta.uKeptByBlock = bKeepedByBlock;
                sMeta.uReceiveTime = mTimeProvider.now();
                sMeta.uLastRelayedTime = mTimeProvider.now();
                sMeta.uDoubleSpendSeen = haveSpentInputs(sTransaction);
                memcpy(sMeta.uPadding, 0, sizeof(sMeta.uPadding));
                try {
                    CryptoNote::Transaction sTx = sTransaction;
                    mBlockchain.pDB->blockTxnStart(false);
                    mBlockchain.pDB->addTxPoolTransaction(sTx, sMeta);
                    mBlockchain.pDB->blockTxnStop();
                    mBlockchain.pDB->blockTxnStart(false);
                    mBlockchain.pDB->addTimeToLifeIndex(sTxHash, sTtl.ttl);
                    mBlockchain.pDB->blockTxnStop();
                } catch (const std::exception &e) {
                    mLogger(ERROR, BRIGHT_RED) << "transaction already exists at inserting in memory pool: "
                                               << e.what();
                    return false;
                }

                tvc.m_added_to_pool = true;
                tvc.m_verification_failed = false;
            }

            sMaxUsedBlock.clear();
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " !bInputsValid";

        if (!bKeepedByBlock) {
            bool bSizeValid = mBlockchain.checkTransactionSize(uBlobSize);
            if (!bSizeValid) {
                mLogger(ERROR, BRIGHT_RED) << "tx too big, rejected";
                tvc.m_verification_failed = true;
                return false;
            }
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " !bKeepedByBlock 1";

        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        if (!bKeepedByBlock) {
            mLogger(ERROR, BRIGHT_RED) << "Trying to add recently deleted transaction. Ignore: " << sTxHash;
            tvc.m_verification_failed = true;
            tvc.m_should_be_relayed = false;
            tvc.m_added_to_pool = false;
            return true;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << " !bKeepedByBlock 2";

        if (!bIsLMDB) {
            FTransactionDetails sTxDetails;

            sTxDetails.sTransactionHash = sTxHash;
            sTxDetails.uBlobSize = uBlobSize;
            sTxDetails.sTransaction = sTransaction;
            sTxDetails.uFee = uFee;
            sTxDetails.bKeepedByBlock = bKeepedByBlock;
            sTxDetails.sReceiveTime = mTimeProvider.now();
            sTxDetails.maxUsedBlock = sMaxUsedBlock;
            sTxDetails.lastFailedBlock.clear();

            auto sTxDetailsP = mTransactions.insert(sTxDetails);
            if (!(sTxDetailsP.second)) {
                mLogger(ERROR, BRIGHT_RED) << "transaction already exists at inserting in memory pool";
                return false;
            }

            mPaymentIndex.add(sTransaction);
            mTimestampIndex.add(sTxDetails.sReceiveTime, sTxDetails.sTransactionHash);

            if (sTtl.ttl != 0) {
                mTimeToLifeIndex.emplace(std::make_pair(sTxHash, sTtl.ttl));
            }
        }
        else {
            sMeta.uBlobSize = uBlobSize;
            sMeta.uKeptByBlock = bKeepedByBlock;
            sMeta.uFee = uFee;
            sMeta.uMaxUsedBlockID = sMaxUsedBlock.id;
            sMeta.uMaxUsedBlockHeight = sMaxUsedBlock.height;
            sMeta.uLastFailedID = NULL_HASH;
            sMeta.uLastFailedHeight = 0;
            sMeta.uReceiveTime = mTimeProvider.now();
            sMeta.uLastRelayedTime = mTimeProvider.now();
            sMeta.uDoubleSpendSeen = false;
            memset(sMeta.uPadding, 0, sizeof(sMeta.uPadding));

            try {
                Crypto::Hash sHash = getObjectHash(sTransaction);
                mBlockchain.pDB->blockTxnStart(false);
                mBlockchain.pDB->removeTxPoolTransaction(sHash);
                mBlockchain.pDB->blockTxnStop();
                const CryptoNote::Transaction& sTx = sTransaction;
                mBlockchain.pDB->blockTxnStart(false);
                mBlockchain.pDB->addTxPoolTransaction(sTx, sMeta);
                mBlockchain.pDB->blockTxnStop();
                mBlockchain.pDB->blockTxnStart(false);
                mBlockchain.pDB->addPaymentIndex(sTx);
                mBlockchain.pDB->blockTxnStop();
                mBlockchain.pDB->blockTxnStart(false);
                mBlockchain.pDB->addTimestampIndex(sMeta.uReceiveTime, sHash);
                mBlockchain.pDB->blockTxnStop();

                if (sTtl.ttl != 0) {
                    mBlockchain.pDB->blockTxnStart(false);
                    mBlockchain.pDB->addTimeToLifeIndex(sHash, sTtl.ttl);
                    mBlockchain.pDB->blockTxnStop();
                }
            } catch (std::exception &e) {
                mLogger(ERROR, BRIGHT_RED) << "internal error: transaction already exists at inserting in memory pool: "
                                           << e.what();
                return false;
            }
        }

        tvc.m_added_to_pool = true;
        tvc.m_should_be_relayed = bInputsValid && (uFee > 0 || bIsFusionTransaction || sTtl.ttl != 0);
        tvc.m_verification_failed = false;

        if (!bIsLMDB) {
            if (!addTransactionInputs(sTxHash, sTransaction, bKeepedByBlock)) {
                return false;
            }
        }

        tvc.m_verification_failed = false;

        return true;
    }

    bool TxMemoryPool::addTransaction(const Transaction &sTransaction,
                                      tx_verification_context &tvc,
                                      bool bKeepedByBlock)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << "L370";
        Crypto::Hash sHash = NULL_HASH;
        uint64_t uBlobSize = 0;
        getObjectHash(sTransaction, sHash, uBlobSize);

        return addTransaction(sTransaction, sHash, uBlobSize, tvc, bKeepedByBlock);
    }

    bool TxMemoryPool::takeTransaction(const Crypto::Hash &sTxHash,
                                       Transaction &sTransaction,
                                       uint64_t &uBlobSize,
                                       uint64_t &uFee)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        if (!bIsLMDB) {
            auto sIt = mTransactions.find(sTxHash);
            if (sIt == mTransactions.end()) {
                return false;
            }

            auto &sTxD = *sIt;

            sTransaction = sTxD.sTransaction;
            uBlobSize = sTxD.uBlobSize;
            uFee = sTxD.uFee;

            removeTransaction(sIt);
        } else if (bIsLMDB) {
            FTxPoolMeta sMeta;
            CryptoNote::blobData sTxBlob;
            mBlockchain.pDB->blockTxnStart(false);
            if (!mBlockchain.pDB->getTxPoolTransactionBlob(sTxHash, sTxBlob)) {
                // Some log
                return false;
            }
            mBlockchain.pDB->blockTxnStop();
            Transaction sTx;
            if (!parseAndValidateTransactionFromBlob(sTxBlob, sTx)) {
                // Some log
                return false;
            }

            mBlockchain.pDB->blockTxnStart(false);
            if (!mBlockchain.pDB->getTxPoolTransactionMeta(sTxHash, sMeta)) {
                // Some log
                return false;
            }
            mBlockchain.pDB->blockTxnStop();

            sTransaction = sTx;
            uBlobSize = sMeta.uBlobSize;
            uFee = sMeta.uFee;

            mBlockchain.pDB->blockTxnStart(false);
            mBlockchain.pDB->removeTxPoolTransaction(sTxHash);
            mBlockchain.pDB->blockTxnStop();
        }

        return true;
    }

    uint64_t TxMemoryPool::getTransactionsCount() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);
        uint64_t uSize;
        if (!bIsLMDB) {
            uSize = mTransactions.size();
        } else {
            uSize = mBlockchain.pDB->getTxPoolTransactionCount();
        }

        return uSize;
    }

    void TxMemoryPool::getTransactions(std::list<Transaction> &lTransactions) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "txList: " << lTransactions.size() << ENDL;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        if (!bIsLMDB) {
            for (const auto &sTx : mTransactions) {
                lTransactions.push_back(sTx.sTransaction);
            }
        }
        else if (bIsLMDB) {
            mBlockchain.pDB->forAllTxPoolTransactions([&lTransactions](const Crypto::Hash &sTxhash,
                                                                       const FTxPoolMeta &sMeta,
                                                                       const CryptoNote::blobData *sBlobData) {
                Transaction sTransaction;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                    return false;
                }

                lTransactions.push_back(sTransaction);
                return true;
            }, true);
        }
    }

    void TxMemoryPool::getTransactions(std::list<Transaction> &lTransactions,
                                       bool bIncludeUnrelayedTransactions) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "txList: " << lTransactions.size() << ENDL;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        mBlockchain.pDB->forAllTxPoolTransactions([&lTransactions](const Crypto::Hash &sTxhash,
                                                      const FTxPoolMeta &sMeta,
                                                      const CryptoNote::blobData *sBlobData)
        {
            Transaction sTransaction;
            if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                return false;
            }

            lTransactions.push_back(sTransaction);
            return true;
        }, true, bIncludeUnrelayedTransactions);
    }

    void TxMemoryPool::getTransactions(const std::vector<Crypto::Hash> &vTxHashes,
                                       std::vector<CryptoNote::Transaction> &vTransactions,
                                       std::vector<Crypto::Hash> &vMissedTxs)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::"<<__func__;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "txList: " << vTransactions.size() << ENDL;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "missedList: " << vMissedTxs.size() << ENDL;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        for (const auto &sId : vTxHashes) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::"<<__func__<< ". sId: " << sId << ENDL;

            if (bIsLMDB) {
                if (mBlockchain.pDB->txPoolHasTransaction(sId)) {
                    mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::"<<__func__<< "found tx with hash " << sId << "in mempool" << ENDL;
                    CryptoNote::blobData sBlobData;
                    CryptoNote::Transaction sTx;
                    mBlockchain.pDB->blockTxnStart(false);
                    mBlockchain.pDB->getTxPoolTransactionBlob(sId, sBlobData);
                    mBlockchain.pDB->blockTxnStop();

                    if (!parseAndValidateTransactionFromBlob(std::move(sBlobData), sTx)) {
                        mLogger(ERROR, BRIGHT_RED) << "TxMemoryPool::"<<__func__ << "Invalid transaction: " << sId;
                    }

                    vTransactions.push_back(sTx);

                    mBlockchain.pDB->blockTxnStart(false);
                    mBlockchain.pDB->removeTxPoolTransaction(sId);
                    mBlockchain.pDB->blockTxnStop();
                } else {
                    mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::"<<__func__<< "couldn't find tx with hash " << sId << "in mempool" << ENDL;
                    vMissedTxs.push_back(sId);
                }
            }
            else {
                auto sIt = mTransactions.find(sId);
                if (sIt == mTransactions.end()) {
                    mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::"<<__func__<< "couldn't find tx with hash " << sId << "in mempool" << ENDL;
                    vMissedTxs.push_back(sId);
                } else {
                    mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::"<<__func__<< "found tx with hash " << sId << "in mempool" << ENDL;
                    vTransactions.push_back(sIt->sTransaction);
                }
            }

        }

        return;
    }

    void TxMemoryPool::getMemoryPool(std::list<CryptoNote::TxMemoryPool::FTransactionDetails> lTxD) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        if (!bIsLMDB) {
            for (const auto &sTxD : mFeeIndex) {
                lTxD.push_back(sTxD);
            }
        }
        else if (bIsLMDB) {
            mBlockchain.pDB->forAllTxPoolTransactions([&lTxD](const Crypto::Hash &sTxhash,
                                                                       const FTxPoolMeta &sMeta,
                                                                       const CryptoNote::blobData *sBlobData) {
                Transaction sTransaction;
                FTransactionDetails sTxD;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                    return false;
                }

                sTxD.sTransactionHash = sTxhash;
                sTxD.sTransaction = sTransaction;
                sTxD.uBlobSize = sMeta.uBlobSize;
                sTxD.uFee = sMeta.uFee;
                sTxD.bKeepedByBlock = sMeta.uKeptByBlock;
                sTxD.sReceiveTime = sMeta.uReceiveTime;
                sTxD.maxUsedBlock.id = sMeta.uMaxUsedBlockID;
                sTxD.maxUsedBlock.height = sMeta.uMaxUsedBlockHeight;
                sTxD.lastFailedBlock.id = sMeta.uLastFailedID;
                sTxD.lastFailedBlock.height = sMeta.uLastFailedHeight;

                lTxD.push_back(sTxD);

                return true;
            }, true);
        }
    }

    std::list<CryptoNote::TxMemoryPool::FTransactionDetails> TxMemoryPool::getMemoryPool() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        std::list<CryptoNote::TxMemoryPool::FTransactionDetails> lTxD;
        if (!bIsLMDB) {
            for (const auto &sTxD : mFeeIndex) {
                lTxD.push_back(sTxD);
            }
        }
        else if (bIsLMDB) {
            mBlockchain.pDB->forAllTxPoolTransactions([&lTxD](const Crypto::Hash &sTxhash,
                                                              const FTxPoolMeta &sMeta,
                                                              const CryptoNote::blobData *sBlobData) {
                Transaction sTransaction;
                FTransactionDetails sTxD;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                    return false;
                }

                sTxD.sTransactionHash = sTxhash;
                sTxD.sTransaction = sTransaction;
                sTxD.uBlobSize = sMeta.uBlobSize;
                sTxD.uFee = sMeta.uFee;
                sTxD.bKeepedByBlock = sMeta.uKeptByBlock;
                sTxD.sReceiveTime = sMeta.uReceiveTime;
                sTxD.maxUsedBlock.id = sMeta.uMaxUsedBlockID;
                sTxD.maxUsedBlock.height = sMeta.uMaxUsedBlockHeight;
                sTxD.lastFailedBlock.id = sMeta.uLastFailedID;
                sTxD.lastFailedBlock.height = sMeta.uLastFailedHeight;

                lTxD.push_back(sTxD);

                return true;
            }, true);
        }

        return lTxD;
    }

    void TxMemoryPool::getDifference(const std::vector<Crypto::Hash> &vKnownTxHashes,
                                     std::vector<Crypto::Hash> &vNewTxHashes,
                                     std::vector<Crypto::Hash> &vDeletedTxHashes) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        std::unordered_set<Crypto::Hash> sReadyTxHashes;

        if (!bIsLMDB) {
            for (const auto &sTx : mTransactions) {
                FTransactionCheckInfo sCheckInfo(sTx);
                if (mValidatedTransactions.find(sTx.sTransactionHash) != mValidatedTransactions.end()) {
                    sReadyTxHashes.insert(sTx.sTransactionHash);
                    mLogger(DEBUGGING) << "MemPool - tx " << sTx.sTransactionHash << " loaded from cache";
                } else if (isTransactionReadyToGo(sTx.sTransaction, sCheckInfo)) {
                    sReadyTxHashes.insert(sTx.sTransactionHash);
                    mValidatedTransactions.insert(sTx.sTransactionHash);
                    mLogger(DEBUGGING) << "MemPool - tx " << sTx.sTransactionHash << " added to cache";
                }
            }
        } else {
            mBlockchain.pDB->forAllTxPoolTransactions([&sReadyTxHashes](const Crypto::Hash &sTxHash,
                                                                       const FTxPoolMeta &sMeta,
                                                                       const CryptoNote::blobData *sBlobData)
            {
                Transaction sTx;
                FTransactionDetails sTxD;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTx)) {
                    // Some log
                    return false;
                }

                sReadyTxHashes.insert(sTxHash);

                return true;
            }, true);
        }


        std::unordered_set<Crypto::Hash> sKnownSet(vKnownTxHashes.begin(), vKnownTxHashes.end());
        for (auto sIt = sReadyTxHashes.begin(), e = sReadyTxHashes.end(); sIt != e;) {
            auto sKnowIt = sKnownSet.find(*sIt);
            if (sKnowIt != sKnownSet.end()) {
                sKnownSet.erase(sKnowIt);
                sIt = sReadyTxHashes.erase(sIt);
            } else {
                ++sIt;
            }
        }

        vNewTxHashes.assign(sReadyTxHashes.begin(), sReadyTxHashes.end());
        vDeletedTxHashes.assign(sKnownSet.begin(), sKnownSet.end());
    }

    bool TxMemoryPool::onBlockchainIncrement(uint64_t uNewBlockHeight, const Crypto::Hash &sTopBlockId)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        if (!bIsLMDB) {
            if (!mValidatedTransactions.empty()) {
                mLogger(DEBUGGING) << "MemPool - Block height incremented, cleared " << mValidatedTransactions.size()
                                   << " cached transaction hashes. New height: " << uNewBlockHeight
                                   << " Top block: " << sTopBlockId;
                mValidatedTransactions.clear();
            }
        }

        return true;
    }

    bool TxMemoryPool::onBlockchainDecrement(uint64_t uNewBlockHeight, const Crypto::Hash &sTopBlockId)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        if (!bIsLMDB) {
            if (!mValidatedTransactions.empty()) {
                mLogger(DEBUGGING) << "MemPool - Block height incremented, cleared " << mValidatedTransactions.size()
                                   << " cached transaction hashes. New height: " << uNewBlockHeight
                                   << " Top block: " << sTopBlockId;
                mValidatedTransactions.clear();
            }
        }

        return true;
    }

    bool TxMemoryPool::haveTransaction(const Crypto::Hash &sTxHash) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        if (!bIsLMDB) {
            if (mTransactions.count(sTxHash)) {
                return true;
            }
        } else {
            if (mBlockchain.pDB->txPoolHasTransaction(sTxHash)) {
                return true;
            }
        }

        return false;
    }

    void TxMemoryPool::lock() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        mTransactionsLock.lock();
    }

    void TxMemoryPool::unlock() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        mTransactionsLock.unlock();
    }

    std::unique_lock<std::recursive_mutex> TxMemoryPool::obtainGuard() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        return std::unique_lock<std::recursive_mutex>(mTransactionsLock);
    }

    bool TxMemoryPool::isTransactionReadyToGo(const Transaction &sTransaction,
                                              FTransactionCheckInfo &sTxCheckInfo) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        if (!mBlockchain.checkTransactionInputs(sTransaction,
                                               sTxCheckInfo.maxUsedBlock,
                                               sTxCheckInfo.lastFailedBlock)) {
            return false;
        }

        if (mBlockchain.haveSpentKeyImages(sTransaction)) {
            return false;
        }

        return true;
    }

    std::string TxMemoryPool::printTransactionPool(bool bShortFormat) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        std::stringstream ss;

        // TODO lmdbization
        if (!bIsLMDB) {
            for (const auto &sTxD : mFeeIndex) {
                ss << "id: " << sTxD.sTransactionHash << std::endl;

                if (!bShortFormat) {
                    ss << storeToJson(sTxD.sTransaction) << std::endl;
                }

                ss << "blobSize: " << sTxD.uBlobSize << std::endl
                   << "fee: " << mCurrency.formatAmount(sTxD.uFee) << std::endl
                   << "keptByBlock: " << (sTxD.bKeepedByBlock ? 'T' : 'F') << std::endl
                   << "max_used_block_height: " << sTxD.maxUsedBlock.height << std::endl
                   << "max_used_block_id: " << sTxD.maxUsedBlock.id << std::endl
                   << "last_failed_height: " << sTxD.lastFailedBlock.height << std::endl
                   << "last_failed_id: " << sTxD.lastFailedBlock.id << std::endl
                   << "amount_out: " << get_outs_money_amount(sTxD.sTransaction) << std::endl
                   << "fee_atomic_units: " << sTxD.uFee << std::endl
                   << "received_timestamp: " << sTxD.sReceiveTime << std::endl
                   << "received: " << std::ctime(&sTxD.sReceiveTime);

                auto ttlIt = mTimeToLifeIndex.find(sTxD.sTransactionHash);
                if (ttlIt != mTimeToLifeIndex.end()) {
                    ss << "TTL: " << std::ctime(reinterpret_cast<const time_t*>(&ttlIt->second));
                }

                ss << std::endl;
            }
        }
        else if (bIsLMDB) {
            std::list<CryptoNote::TxMemoryPool::FTransactionDetails> lTxD;
            mBlockchain.pDB->forAllTxPoolTransactions([&lTxD](const Crypto::Hash &sTxHash,
                                                              const FTxPoolMeta &sMeta,
                                                              const CryptoNote::blobData *sBlobData) {
                Transaction sTransaction;
                FTransactionDetails sTxD;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                    return false;
                }

                sTxD.sTransactionHash = sTxHash;
                sTxD.sTransaction = sTransaction;
                sTxD.uBlobSize = sMeta.uBlobSize;
                sTxD.uFee = sMeta.uFee;
                sTxD.bKeepedByBlock = sMeta.uKeptByBlock;
                sTxD.sReceiveTime = sMeta.uReceiveTime;
                sTxD.maxUsedBlock.id = sMeta.uMaxUsedBlockID;
                sTxD.maxUsedBlock.height = sMeta.uMaxUsedBlockHeight;
                sTxD.lastFailedBlock.id = sMeta.uLastFailedID;
                sTxD.lastFailedBlock.height = sMeta.uLastFailedHeight;

                lTxD.push_back(sTxD);

                return true;
            }, true);

            for (const auto &sTxD : lTxD) {
                ss << "id: " << sTxD.sTransactionHash << std::endl;

                if (!bShortFormat) {
                    ss << storeToJson(sTxD.sTransaction) << std::endl;
                }

                ss << "blobSize: " << sTxD.uBlobSize << std::endl
                   << "fee: " << mCurrency.formatAmount(sTxD.uFee) << std::endl
                   << "keptByBlock: " << (sTxD.bKeepedByBlock ? 'T' : 'F') << std::endl
                   << "max_used_block_height: " << sTxD.maxUsedBlock.height << std::endl
                   << "max_used_block_id: " << sTxD.maxUsedBlock.id << std::endl
                   << "last_failed_height: " << sTxD.lastFailedBlock.height << std::endl
                   << "last_failed_id: " << sTxD.lastFailedBlock.id << std::endl
                   << "amount_out: " << get_outs_money_amount(sTxD.sTransaction) << std::endl
                   << "fee_atomic_units: " << sTxD.uFee << std::endl
                   << "received_timestamp: " << sTxD.sReceiveTime << std::endl
                   << "received: " << std::ctime(&sTxD.sReceiveTime);

                uint64_t sTtl = mBlockchain.pDB->getTimeToLife(sTxD.sTransactionHash);
                ss << "TTL: " << std::ctime(reinterpret_cast<const time_t*>(sTtl));

                ss << std::endl;
            }
        }

        return ss.str();
    }

    bool TxMemoryPool::fillBlockTemplate(Block &sBlock,
                                         uint64_t uMedianSize,
                                         uint64_t uMaxCumulativeSize,
                                         uint64_t uAlreadyGeneratedCoins,
                                         uint64_t &uTotalSize,
                                         uint64_t &uFee)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        uTotalSize = 0;
        uFee = 0;

        uint64_t uMaxTotalSize = (125 * uMedianSize) / 100;
        uMaxTotalSize = std::min(uMaxTotalSize, uMaxCumulativeSize) - mCurrency.minerTxBlobReservedSize();

        BlockTemplate sBT;

        std::list<Transaction> lTxs;

        if (!bIsLMDB) {
            for (auto sIt = mFeeIndex.rbegin(); sIt != mFeeIndex.rend() && sIt->uFee == 0; ++sIt) {
                const auto &sTxD = *sIt;

                if (mTimeToLifeIndex.count(sTxD.sTransactionHash)) {
                    continue;
                }

                if (mCurrency.fusionTxMaxSize() < uTotalSize + sTxD.uBlobSize) {
                    continue;
                }

                FTransactionCheckInfo sCheckInfo(sTxD);
                if (isTransactionReadyToGo(sTxD.sTransaction, sCheckInfo) &&
                    sBT.addTransaction(sTxD.sTransactionHash, sTxD.sTransaction)) {
                    uTotalSize += sTxD.uBlobSize;
                    mLogger(DEBUGGING) << "Fusion transaction " << sTxD.sTransactionHash << " included to block template";
                }
            }

            for (auto i = mFeeIndex.begin(); i != mFeeIndex.end(); ++i) {
                const auto &sTxD = *i;

                if (mTimeToLifeIndex.count(sTxD.sTransactionHash)) {
                    continue;
                }

                uint64_t uBlockSizeLimit = (sTxD.uFee == 0) ? uMedianSize : uMaxTotalSize;
                if (uBlockSizeLimit < uTotalSize + sTxD.uBlobSize) {
                    continue;
                }

                FTransactionCheckInfo sCheckInfo(sTxD);
                bool bReady = false;
                if (mValidatedTransactions.find(sTxD.sTransactionHash) != mValidatedTransactions.end()) {
                    bReady = true;
                    mLogger(DEBUGGING) << "Fill block template - tx added from cache: " << sTxD.sTransactionHash;
                } else if(isTransactionReadyToGo(sTxD.sTransaction, sCheckInfo)) {
                    bReady = true;
                    mValidatedTransactions.insert(sTxD.sTransactionHash);
                    mLogger(DEBUGGING) << "Fill block template - tx added to cache: " << sTxD.sTransactionHash;
                }

                mFeeIndex.modify(i, [&sCheckInfo](FTransactionCheckInfo &sItem) {
                    sItem = sCheckInfo;
                });

                if (bReady && sBT.addTransaction(sTxD.sTransactionHash, sTxD.sTransaction)) {
                    uTotalSize += sTxD.uBlobSize;
                    uFee = sTxD.uFee;
                    mLogger(DEBUGGING) << "Transaction " << sTxD.sTransactionHash << " included to block template";
                } else {
                    mLogger(DEBUGGING) << "Transaction " << sTxD.sTransactionHash << " is failed to include to block template";
                }
            }
        }
        else {
            std::list<CryptoNote::TxMemoryPool::FTransactionDetails> lTxD;
            mBlockchain.pDB->forAllTxPoolTransactions([&lTxD](const Crypto::Hash &sTxhash,
                                                              const FTxPoolMeta &sMeta,
                                                              const CryptoNote::blobData *sBlobData) {
                Transaction sTransaction;
                FTransactionDetails sTxD;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                    return false;
                }

                sTxD.sTransactionHash = sTxhash;
                sTxD.sTransaction = sTransaction;
                sTxD.uBlobSize = sMeta.uBlobSize;
                sTxD.uFee = sMeta.uFee;
                sTxD.bKeepedByBlock = sMeta.uKeptByBlock;
                sTxD.sReceiveTime = sMeta.uReceiveTime;
                sTxD.maxUsedBlock.id = sMeta.uMaxUsedBlockID;
                sTxD.maxUsedBlock.height = sMeta.uMaxUsedBlockHeight;
                sTxD.lastFailedBlock.id = sMeta.uLastFailedID;
                sTxD.lastFailedBlock.height = sMeta.uLastFailedHeight;

                lTxD.push_back(sTxD);

                return true;
            }, true);

            for (const auto &sTxD : lTxD) {
                uint64_t uBlockSizeLimit = (sTxD.uFee == 0) ? uMedianSize : uMaxTotalSize;
                if (uBlockSizeLimit < uTotalSize + sTxD.uBlobSize) {
                    continue;
                }

                FTransactionCheckInfo sCheckInfo(sTxD);
                bool bReady = false;
                if (isTransactionReadyToGo(sTxD.sTransaction, sCheckInfo)) {
                    bReady = true;
                }

                if (bReady && sBT.addTransaction(sTxD.sTransactionHash, sTxD.sTransaction)) {
                    uTotalSize += sTxD.uBlobSize;
                    uFee = sTxD.uFee;
                    mLogger(DEBUGGING) << "Transaction " << sTxD.sTransactionHash << " included to block template";
                } else {
                    mLogger(DEBUGGING) << "Transaction " << sTxD.sTransactionHash << " is failed to include to block template";
                }
            }
        }

        sBlock.transactionHashes = sBT.getTransactions();

        return true;
    }

    bool TxMemoryPool::init(const std::string &config_folder)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        mConfigFolder = config_folder;

        if (!bIsLMDB) {
            std::string cStateFilePath = config_folder + "/" + mCurrency.txPoolFileName();
            boost::system::error_code sEc;
            if (!boost::filesystem::exists(cStateFilePath, sEc)) {
                return true;
            }

            if (!loadFromBinaryFile(*this, cStateFilePath)) {
                mLogger(ERROR) << "Failed to load memory pool from file " << cStateFilePath;

                mTransactions.clear();
                mSpentKeyImages.clear();
                mSpentOutputs.clear();
                mPaymentIndex.clear();
                mTimestampIndex.clear();
                mTimeToLifeIndex.clear();
            } else {
                buildIndices();
            }

            removeExpiredTransactions();
        } else if (bIsLMDB) {
            if (mBlockchain.pDB->pOpen) {
                removeExpiredTransactions();
            }
        }

        return true;
    }

    bool TxMemoryPool::deinit()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        if (!bIsLMDB) {
            if (!Tools::create_directories_if_necessary(mConfigFolder)) {
                mLogger(INFO) << "Failed to create data directory: " << mConfigFolder;
                return false;
            }

            std::string cStateFilePath = mConfigFolder + "/" + mCurrency.txPoolFileName();

            if (!storeToBinaryFile(*this, cStateFilePath)) {
                mLogger(INFO) << "Failed to serialize memory pool to file " << cStateFilePath;
            }

            mPaymentIndex.clear();
            mTimestampIndex.clear();
            mTimeToLifeIndex.clear();
        }

        return true;
    }

    void serialize(CryptoNote::TxMemoryPool::FTransactionDetails &sTxD, ISerializer &s)
    {
        s(sTxD.sTransactionHash, "id");
        s(sTxD.uBlobSize, "blobSize");
        s(sTxD.uFee, "fee");
        s(sTxD.sTransaction, "tx");
        s(sTxD.maxUsedBlock.height, "maxUsedBlock.height");
        s(sTxD.maxUsedBlock.id, "maxUsedBlock.id");
        s(sTxD.lastFailedBlock.height, "lastFailedBlock.height");
        s(sTxD.lastFailedBlock.id, "lastFailedBlock.id");
        s(sTxD.bKeepedByBlock, "keptByBlock");
        s(reinterpret_cast<uint64_t &>(sTxD.sReceiveTime), "receiveTime");
    }

    void TxMemoryPool::serialize(ISerializer &s)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        uint8_t uVersion = CURRENT_MEMPOOL_ARCHIVE_VER;

        s(uVersion, "version");

        if (uVersion != CURRENT_MEMPOOL_ARCHIVE_VER) {
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        if (s.type() == ISerializer::INPUT) {
            mTransactions.clear();
            readSequence<FTransactionDetails>(std::inserter(mTransactions, mTransactions.end()), "transactions", s);
        } else {
            writeSequence<FTransactionDetails>(mTransactions.begin(), mTransactions.end(), "transactions", s);
        }

        KV_MEMBER(mSpentKeyImages);
        KV_MEMBER(mSpentOutputs);
        KV_MEMBER(mRecentlyDeletedTransactions);
    }

    void TxMemoryPool::onIdle()
    {
        mLogger(TRACE, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        mTxCheckInterval.call([this](){ return removeExpiredTransactions(); });
    }

    bool TxMemoryPool::removeExpiredTransactions()
    {
        mLogger(TRACE, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        uint64_t uNow = mTimeProvider.now();
        bool bSomethingRemoved = false;
        if (!bIsLMDB) {
            for (auto sIt = mRecentlyDeletedTransactions.begin(); sIt != mRecentlyDeletedTransactions.end();) {
                uint64_t uElapsedTimeSinceDeletion = uNow - sIt->second;
                if (uElapsedTimeSinceDeletion > mCurrency.numberOfPeriodsToForgetTxDeletedFromPool() *
                                                mCurrency.mempoolTxLiveTime()) {
                    sIt = mRecentlyDeletedTransactions.erase(sIt);
                } else {
                    ++sIt;
                }
            }

            for (auto sIt = mTransactions.begin(); sIt != mTransactions.end();) {
                uint64_t uTxAge = uNow - sIt->sReceiveTime;
                bool bRemove = uTxAge > (sIt->bKeepedByBlock ? mCurrency.mempoolTxFromAltBlockLiveTime() :
                                         mCurrency.mempoolTxLiveTime());
                auto sTtlIt = mTimeToLifeIndex.find(sIt->sTransactionHash);
                bool bIsTtlExpired = (sTtlIt != mTimeToLifeIndex.end() && sTtlIt->second <= uNow);

                if (bRemove || bIsTtlExpired) {
                    if (bIsTtlExpired) {
                        mLogger(TRACE) << "Tx " << sIt->sTransactionHash
                                       << " removed from tx pool due to expired TTL, TTL : " << sTtlIt->second;
                    } else {
                        mLogger(TRACE) << "Tx " << sIt->sTransactionHash
                                       << " removed from tx pool due to outdated, age: " << uTxAge;
                    }

                    mRecentlyDeletedTransactions.emplace(sIt->sTransactionHash, uNow);
                    sIt = removeTransaction(sIt);
                    bSomethingRemoved = true;
                } else {
                    ++sIt;
                }
            }
        }
        else {
            std::list<CryptoNote::TxMemoryPool::FTransactionDetails> lTxD;
            mBlockchain.pDB->blockTxnStart(true);
            mBlockchain.pDB->forAllTxPoolTransactions([&lTxD](const Crypto::Hash &sTxhash,
                                                              const FTxPoolMeta &sMeta,
                                                              const CryptoNote::blobData *sBlobData) {
                Transaction sTransaction;
                FTransactionDetails sTxD;
                if (!parseAndValidateTransactionFromBlob(*sBlobData, sTransaction)) {
                    return false;
                }

                sTxD.sTransactionHash = sTxhash;
                sTxD.sTransaction = sTransaction;
                sTxD.uBlobSize = sMeta.uBlobSize;
                sTxD.uFee = sMeta.uFee;
                sTxD.bKeepedByBlock = sMeta.uKeptByBlock;
                sTxD.sReceiveTime = sMeta.uReceiveTime;
                sTxD.maxUsedBlock.id = sMeta.uMaxUsedBlockID;
                sTxD.maxUsedBlock.height = sMeta.uMaxUsedBlockHeight;
                sTxD.lastFailedBlock.id = sMeta.uLastFailedID;
                sTxD.lastFailedBlock.height = sMeta.uLastFailedHeight;

                lTxD.push_back(sTxD);

                return true;
            }, true);

            mBlockchain.pDB->blockTxnStop();
            for (const auto &sTxD : lTxD) {
                uint64_t uTxAge = uNow - sTxD.sReceiveTime;
                bool bRemove = uTxAge > (sTxD.bKeepedByBlock ? mCurrency.mempoolTxFromAltBlockLiveTime() :
                                         mCurrency.mempoolTxLiveTime());

                mBlockchain.pDB->blockTxnStart(true);
                uint64_t uTtl = mBlockchain.pDB->getTimeToLife(sTxD.sTransactionHash);
                mBlockchain.pDB->blockTxnStop();
                bool bIsTtlExpired = uTtl <= uNow;

                if (bRemove || bIsTtlExpired) {
                    if (bIsTtlExpired) {
                        mLogger(TRACE) << "Tx " << sTxD.sTransactionHash
                                       << " removed from tx pool due to expired TTL, TTL : " << uTtl;
                    } else {
                        mLogger(TRACE) << "Tx " << sTxD.sTransactionHash
                                       << " removed from tx pool due to outdated, age: " << uTxAge;
                    }

                    mBlockchain.pDB->removeTxPoolTransaction(sTxD.sTransactionHash);
                    bSomethingRemoved = true;
                }
            }
        }

        if (bSomethingRemoved) {
            mObserverManager.notify(&ITxPoolObserver::txDeletedFromPool);
        }

        return true;
    }

    TxMemoryPool::mTransactionContainerT::iterator
    TxMemoryPool::removeTransaction(mTransactionContainerT::iterator sIt)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        removeTransactionInputs(sIt->sTransactionHash, sIt->sTransaction, sIt->bKeepedByBlock);
        mPaymentIndex.remove(sIt->sTransaction);
        mTimestampIndex.remove(sIt->sReceiveTime, sIt->sTransactionHash);
        mTimeToLifeIndex.erase(sIt->sTransactionHash);

        if (mValidatedTransactions.find(sIt->sTransactionHash) != mValidatedTransactions.end()) {
            mValidatedTransactions.erase(sIt->sTransactionHash);
            mLogger(DEBUGGING) << "Removing transaction from MemPool cache " << sIt->sTransactionHash
                               << ". Cache size: " << mValidatedTransactions.size();
        }

        return mTransactions.erase(sIt);
    }

    bool TxMemoryPool::removeTransactionInputs(const Crypto::Hash &sTxHash,
                                               const Transaction &sTransaction,
                                               bool bKeptByBlock)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        for (const auto &sIn : sTransaction.inputs) {
            if (sIn.type() == typeid(KeyInput)) {
                const auto &sTxIn = boost::get<KeyInput>(sIn);
                auto sIt = mSpentKeyImages.find(sTxIn.keyImage);
                if (!(sIt != mSpentKeyImages.end())) {
                    mLogger(ERROR, BRIGHT_RED) << "failed to find transaction input in key images. img="
                                                    << sTxIn.keyImage << ENDL
                                                    << "transaction id = " << sTxHash;
                    return false;
                }

                std::unordered_set<Crypto::Hash> &sKeyImageSet = sIt->second;
                if (!(!sKeyImageSet.empty())) {
                    mLogger(ERROR, BRIGHT_RED) << "empty key_image set, img=" << sTxIn.keyImage << ENDL
                                               << "transaction id = " << sTxHash;
                    return false;
                }

                auto sItInSet = sKeyImageSet.find(sTxHash);
                if (!(sItInSet != sKeyImageSet.end())) {
                    mLogger(ERROR, BRIGHT_RED) << "transaction id not found in key_image set, img="
                                               << sTxIn.keyImage << ENDL
                                               << "transaction id = " << sTxHash;
                    return false;
                }

                sKeyImageSet.erase(sItInSet);
                if (sKeyImageSet.empty()) {
                    mSpentKeyImages.erase(sIt);
                }
            } else if (sIn.type() == typeid(MultisignatureInput)) {
                if (!bKeptByBlock) {
                    const auto &sMSig = boost::get<MultisignatureInput>(sIn);
                    auto sOut = mGlobalOutputT(sMSig.amount, sMSig.outputIndex);
                    assert(mSpentOutputs.count(sOut));
                    mSpentOutputs.erase(sOut);
                }
            }
        }

        return true;
    }

    bool TxMemoryPool::addTransactionInputs(const Crypto::Hash &sTxHash,
                                            const Transaction &sTransaction,
                                            bool bKeptByBlock)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__ << "L1285";
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        // TODO: lmdbization

        for (const auto &sIn : sTransaction.inputs) {
            if (sIn.type() == typeid(KeyInput)) {
                const auto &sTxIn = boost::get<KeyInput>(sIn);
                std::unordered_set<Crypto::Hash> &sKeyImageSet = mSpentKeyImages[sTxIn.keyImage];
                if (!(bKeptByBlock || sKeyImageSet.size() == 0)) {
                    mLogger(ERROR, BRIGHT_RED)
                            << "internal error: keptByBlock=" << bKeptByBlock
                            << ",  kei_image_set.size()=" << sKeyImageSet.size() << ENDL
                            << "txin.keyImage=" << sTxIn.keyImage << ENDL << "tx_id=" << sTxHash;
                    return false;
                }

                auto sInsRes = sKeyImageSet.insert(sTxHash);
            } else if (sIn.type() == typeid(MultisignatureInput)) {
                if (!bKeptByBlock) {
                    const auto &sMSig = boost::get<MultisignatureInput>(sIn);
                    auto sRes = mSpentOutputs.insert(mGlobalOutputT(sMSig.amount, sMSig.outputIndex));
                    (void)sRes;
                    assert(sRes.second);
                }
            }
        }

        return true;
    }

    bool TxMemoryPool::haveSpentInputs(const Transaction &tx) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        // TODO: lmdbization

        if (!bIsLMDB) {
            for (const auto &sIn : tx.inputs) {
                if (sIn.type() == typeid(KeyInput)) {
                    const auto& sKeyIn = boost::get<KeyInput>(sIn);
                    if (mSpentKeyImages.count(sKeyIn.keyImage)) {
                        return true;
                    }
                } else if (sIn.type() == typeid(MultisignatureInput)) {
                    const auto& sMSig = boost::get<MultisignatureInput>(sIn);
                    if (mSpentOutputs.count(mGlobalOutputT(sMSig.amount, sMSig.outputIndex))) {
                        return true;
                    }
                }
            }
        }
        else if (bIsLMDB) {
            for (const auto &sIn : tx.inputs) {
                if (sIn.type() == typeid(KeyInput)) {
                    const auto& sKeyIn = boost::get<KeyInput>(sIn);
                    if (mBlockchain.pDB->hasKeyImage(sKeyIn.keyImage)) {
                        return true;
                    }
                } else if(sIn.type() == typeid(MultisignatureInput)) {
                    const auto& sMSig = boost::get<MultisignatureInput>(sIn);
                }
            }
        }



        return false;
    }

    bool TxMemoryPool::addObserver(ITxPoolObserver *observer)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        return mObserverManager.add(observer);
    }

    bool TxMemoryPool::removeObserver(ITxPoolObserver *observer)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;

        return mObserverManager.remove(observer);
    }

    void TxMemoryPool::buildIndices()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        for (auto sIt = mTransactions.begin(); sIt != mTransactions.end(); sIt++) {
            mPaymentIndex.add(sIt->sTransaction);
            mTimestampIndex.add(sIt->sReceiveTime, sIt->sTransactionHash);

            std::vector<TransactionExtraField> vTxExtraFiels;
            parseTransactionExtra(sIt->sTransaction.extra, vTxExtraFiels);
            TransactionExtraTTL sTtl;
            if (findTransactionExtraFieldByType(vTxExtraFiels, sTtl)) {
                if (sTtl.ttl != 0) {
                    mTimeToLifeIndex.emplace(std::make_pair(sIt->sTransactionHash, sTtl.ttl));
                }
            }
        }
    }

    bool TxMemoryPool::getTransactionHashesByPaymentId(const Crypto::Hash &sPaymentId,
                                                       std::vector<Crypto::Hash> &vTransactionHashes)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        // TODO: lmdbization

        if (!bIsLMDB) {
            vTransactionHashes = mPaymentIndex.find(sPaymentId);
        } else if (bIsLMDB) {
            vTransactionHashes = mBlockchain.pDB->getPaymentIndices(sPaymentId);
        }

        return true;
    }

    bool TxMemoryPool::getTransactionHashesByTimestamp(uint64_t uTimestampBegin,
                                                       uint64_t uTimestampEnd,
                                                       uint32_t uTransactionsLimit,
                                                       std::vector<Crypto::Hash> &vHashes,
                                                       uint64_t &uTransactionsNumberWithinTimestamps)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "TxMemoryPool::" << __func__;
        std::lock_guard<std::recursive_mutex> lock(mTransactionsLock);

        bool bIsLMDB = Tools::getDefaultDBType("lmdb");

        // TODO: lmdbization

        if (!bIsLMDB) {
            return mTimestampIndex.find(uTimestampBegin,
                                        uTimestampEnd,
                                        uTransactionsLimit,
                                        vHashes,
                                        uTransactionsNumberWithinTimestamps);
        } else if (bIsLMDB) {
            return mBlockchain.pDB->getTimestampIndicesInRange(uTimestampBegin,
                                                               uTimestampEnd,
                                                               uTransactionsLimit,
                                                               vHashes,
                                                               uTransactionsNumberWithinTimestamps);
        }


    }
} // namespace CryptoNote
