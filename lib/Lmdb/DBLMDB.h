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

#include <atomic>

#include <boost/thread/tss.hpp>

#include <lmdb.h>

#include <Lmdb/BlockchainDB.h>
#include <Lmdb/Structures.h>

#include <Logging/LoggerManager.h>
#include <Logging/LoggerRef.h>

#define ENABLE_AUTO_RESIZE 1

namespace CryptoNote {
    typedef struct FMdbTxnCursors
    {
        // Blocks
        MDB_cursor *sTxcBlocks;
        MDB_cursor *sTxcBlockHeights;
        MDB_cursor *sTxcBlockInfo;

        MDB_cursor *sTxcTransactions;
        MDB_cursor *sTxcTransactionIndices;
        MDB_cursor *sTxcTransactionOutputs;

        MDB_cursor *sTxcOutputTransactions;
        MDB_cursor *sTxcOutputAmounts;

        MDB_cursor *sTxcSpentKeys;

        MDB_cursor *sTxcTransactionPoolMeta;
        MDB_cursor *sTxcTransactionPoolBlob;

        MDB_cursor *sTxcProperties;
    } FMdbTxnCursors;

    typedef struct FMdbReadFlags
    {
        bool bRfTxn;

        bool bRfBlocks;
        bool bRfBlockHeights;
        bool bRfBlockInfo;

        bool bRfTransactions;
        bool bRfTransactionIndices;
        bool bRfTransactionOutputs;

        bool bRfOutputTransactions;
        bool bRfOutputAmounts;

        bool bRfSpentKeys;

        bool bRfTransactionPoolMeta;
        bool bRfTransactionPoolBlob;

        bool bRfProperties;
    } FMdbReadFlags;

// Blocks
#define sCurBlocks sCursor->sTxcBlocks
#define sCurBlockHeights sCursor->sTxcBlockHeights
#define sCurBlockInfo sCursor->sTxcBlockInfo

#define sCurTransactions sCursor->sTxcTransactions
#define sCurTransactionIndices sCursor->sTxcTransactionIndices
#define sCurTransactionOutputs sCursor->sTxcTransactionOutputs

#define sCurOutputTransactions sCursor->sTxcOutputTransactions
#define sCurOutputAmounts sCursor->sTxcOutputAmounts

#define sCurSpentKeys sCursor->sTxcSpentKeys

#define sCurTransactionPoolMeta sCursor->sTxcTransactionPoolMeta
#define sCurTransactionPoolBlob sCursor->sTxcTransactionPoolBlob

#define sCurProperties sCursor->sTxcProperties

    typedef struct FMdbThreadInfo
    {
        MDB_txn *sTReadTxn;
        FMdbTxnCursors sTReadCursors;
        FMdbReadFlags sTReadFlags;

        ~FMdbThreadInfo();
    } FMdbThreadInfo;

    struct FMdbTxnSafe
    {
        FMdbTxnSafe(const bool bCheck = true);

        ~FMdbTxnSafe();

        void commit(std::string cMessage = "");

        void abort();

        void uncheck();

        operator MDB_txn *() { return pTxn; }

        operator MDB_txn **() { return &pTxn; }

        static uint64_t numActiveTx();

        static void preventNewTxns();

        static void waitNoActiveTxns();

        static void allowNewTxns();

        FMdbThreadInfo *pThreadInfo;
        MDB_txn *pTxn;
        bool pBatchTxn = false;
        bool pCheck;
        static std::atomic<uint64_t> pNumActiveTxns;
        static std::atomic_flag pCreationGate;
    };

    class BlockchainLMDB : public BlockchainDB
    {
    public:
        BlockchainLMDB(bool bBatchTransactions, Logging::ILogger &sLogger);

        ~BlockchainLMDB();

        virtual void open(const std::string &cFileName, const int iDBFlags);

        virtual void close();

        virtual void sync();

        virtual void safeSyncMode(const bool bOnOff);

        virtual void reset();

        virtual std::vector<std::string> getFileNames() const;

        virtual std::string getDBName() const;

        virtual bool lock();

        virtual void unlock();

        virtual bool blockExists(const Crypto::Hash &sHash, uint64_t *height = NULL) const;

        virtual uint64_t getBlockHeight(const Crypto::Hash &sHash) const;

        virtual CryptoNote::BlockHeader getBlockHeader(const Crypto::Hash &sHash) const;

        virtual CryptoNote::blobData getBlockBlob(const Crypto::Hash &sHash) const;

        virtual CryptoNote::blobData getBlockBlobFromHeight(const uint64_t &uHeight) const;

        virtual uint64_t getBlockTimestamp(const uint64_t &uHeight) const;

        virtual uint64_t getTopBlockTimestamp() const;

        virtual size_t getBlockSize(const uint64_t &uHeight) const;

        virtual CryptoNote::difficulty_type getBlockCumulativeDifficulty(const uint64_t &uHeight) const;

        virtual CryptoNote::difficulty_type getBlockDifficulty(const uint64_t &uHeight) const;

        virtual uint64_t getBlockAlreadyGeneratedCoins(const uint64_t &uHeight) const;

        virtual Crypto::Hash getBlockHashFromHeight(const uint64_t &uHeight) const;

        virtual std::vector<CryptoNote::Block> getBlocksRange(const uint64_t &uStartHeight,
                                                              const uint64_t &uEndHeight);

        virtual std::vector<Crypto::Hash> getHashesRange(const uint64_t &uStartHeight,
                                                         const uint64_t &uEndHeight);

        virtual void setBatchTransactions(bool bBatchTransactions);

        virtual bool batchStart(uint64_t uBatchNumBlocks = 0, uint64_t uBatchBytes = 0);

        virtual void batchCommit();

        virtual void batchCleanup();

        virtual void batchStop();

        virtual void batchAbort();

        virtual void blockTxnStart(bool bReadOnly);

        virtual void blockTxnStop();

        virtual void blockTxnAbort();

        virtual bool blockReadTxnStart(MDB_txn **sMdbTxn, FMdbTxnCursors **sCurs) const;

        virtual void blockReadTxnStop() const;

        virtual void popBlock(CryptoNote::Block &sBlock, std::vector<CryptoNote::Transaction> &vTransactions);

        virtual Crypto::Hash getTopBlockHash() const;

        virtual CryptoNote::Block getTopBlock() const;

        virtual uint64_t height() const;

        virtual bool transactionExists(const Crypto::Hash &sHash) const;

        virtual bool transactionExists(const Crypto::Hash &sHash, uint64_t &uTransactionId) const;

        virtual uint64_t getTransactionUnlockTime(const Crypto::Hash &sHash) const;

        virtual bool getTransactionBlob(const Crypto::Hash &sHash,
                                        CryptoNote::blobData &sTransactionBlob) const;

        virtual uint64_t getTransactionCount() const;

        virtual std::vector<CryptoNote::Transaction>
        getTransactionList(const std::vector<Crypto::Hash> &vHashList) const;

        virtual uint64_t getTransactionBlockHeight(const Crypto::Hash &sHash) const;

        virtual uint64_t getNumOutputs(const uint64_t &uAmount) const;

        virtual FOutputData getOutputKey(const uint64_t &uAmount, const uint32_t &uIndex);

        virtual FOutputData getOutputKey(const uint32_t &uGlobalIndex) const;

        virtual void getOutputKey(const uint64_t &uAmount,
                                  const std::vector<uint32_t> &vOffsets,
                                  std::vector<FOutputData> &vOutputs,
                                  bool bAllowPartial = false);

        virtual std::vector<uint64_t> getTransactionAmountOutputIndices(const uint64_t uTxId) const;

        virtual txOutIndex getOutputTransactionAndIndexFromGlobal(const uint64_t &uIndex) const;
        virtual void getOutputTransactionAndIndexFromGlobal(const std::vector<uint64_t> &vGlobalIndices,
                                                            std::vector<txOutIndex> &vTxOutIndices) const;

        virtual txOutIndex getOutputTransactionAndIndex(const uint64_t &uAmount, const uint32_t &uIndex) const;

        virtual void getOutputTransactionAndIndex(const uint64_t &uAmount,
                                                  const std::vector<uint32_t> &vOffsets,
                                                  std::vector<txOutIndex> &vIndices) const;

        virtual bool hasKeyImage(const Crypto::KeyImage &sImg) const;

        virtual void addTxPoolTransaction(const CryptoNote::Transaction &sTransaction,
                                          const FTxPoolMeta &sDetails);

        virtual void updateTxPoolTransaction(const Crypto::Hash &sHash,
                                             const FTxPoolMeta &sDetails);

        virtual uint64_t getTxPoolTransactionCount() const;

        virtual bool txPoolHasTransaction(const Crypto::Hash &sHash) const;

        virtual void removeTxPoolTransaction(const Crypto::Hash &sHash);

        virtual bool getTxPoolTransactionMeta(const Crypto::Hash &sHash, FTxPoolMeta &sDetails) const;

        virtual bool getTxPoolTransactionBlob(const Crypto::Hash &sHash, CryptoNote::blobData &sBlobData) const;

        virtual CryptoNote::blobData getTxPoolTransactionBlob(const Crypto::Hash &sHash) const;

        virtual bool forAllTxPoolTransactions(std::function<bool(const Crypto::Hash &,
                                                                 const FTxPoolMeta &,
                                                                 const CryptoNote::blobData *)>,
                                              bool bIncludeBlob = false,
                                              bool bIncludeUnrelayedTransactions = true) const;

        virtual bool forAllKeyImages(std::function<bool(const Crypto::KeyImage &)>) const;

        virtual uint64_t addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                                  const CryptoNote::difficulty_type &uCumulativeDifficulty,
                                  const uint64_t &uCoinsGenerated,
                                  const std::vector<CryptoNote::Transaction> &transactions);

        virtual void doResize(uint64_t uIncreaseSize = 0);

        // helper functions
        static int compareUInt64(const MDB_val *a, const MDB_val *b);

        static int compareHash32(const MDB_val *a, const MDB_val *b);

        static int compareString(const MDB_val *a, const MDB_val *b);

    private:
        bool needResize(uint64_t uIncreaseSize = 0) const;

        void checkAndResizeForBatch(uint64_t uBatchNumBlocks, uint64_t uBatchBytes);

        uint64_t getEstimatedBatchSize(uint64_t uBatchNumBlocks, uint64_t uBatchBytes) const;

        virtual void addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                              const CryptoNote::difficulty_type &uCumulativeDifficulty,
                              const uint64_t &uCoinsGenerated, const Crypto::Hash &sBlockHash);

        virtual void removeBlock();

        virtual uint64_t addTransactionData(const Crypto::Hash &sBlockHash,
                                            const CryptoNote::Transaction &sTransactions,
                                            const Crypto::Hash &sTxHash);

        virtual void removeTransactionData(const Crypto::Hash &sTxHash,
                                           const CryptoNote::Transaction &sTransaction);

        virtual uint64_t addOutput(const Crypto::Hash &sTxHash,
                                   const CryptoNote::TransactionOutput &sTxOutput,
                                   const uint64_t &uIndex, const uint64_t &uUnlockTime);

        virtual void addTransactionAmountOutputIndices(const uint64_t uTxId,
                                                       const std::vector<uint64_t> &vAmountOutputIndices);

        virtual void removeTransactionOutputs(const uint64_t uTxId,
                                              const CryptoNote::Transaction &sTransaction);

        virtual void addSpentKey(const Crypto::KeyImage &sSpentKeyImage);

        virtual void removeSpentKey(const Crypto::KeyImage &sSpentKeyImage);

        uint64_t numOutputs() const;

        void checkOpen() const;

        virtual bool isReadOnly() const;

        virtual void fixUp();

        virtual void resetStats();

        virtual void showStats();

    private:
        MDB_env *mDbEnv;

        MDB_dbi mBlocks;
        MDB_dbi mBlockHeights;
        MDB_dbi mBlockInfo;

        MDB_dbi mTransactions;
        MDB_dbi mTransactionIndices;
        MDB_dbi mTransactionOutputs;

        MDB_dbi mOutputTransactions;
        MDB_dbi mOutputAmounts;

        MDB_dbi mSpentKeys;

        MDB_dbi mTransactionPoolMeta;
        MDB_dbi mTransactionPoolBlob;

        MDB_dbi mProperties;

        mutable uint64_t mCumSize;
        mutable uint32_t mCumCount;

        std::string mFolder;

        FMdbTxnSafe *mWriteDBTxnSafe;
        FMdbTxnSafe *mWriteBatchDBTxnSafe;
        boost::thread::id mWriter;

        bool mBatchActive;
        bool mBatchTransactions;

        FMdbTxnCursors mWriteCursors;
        mutable boost::thread_specific_ptr<FMdbThreadInfo> mTInfo;

        Logging::LoggerRef mLogger;

        // TODO: Remove below to Constants

#if defined(__arm__)
        constexpr static uint64_t DEFAULT_MAPSIZE = 1LL << 31;
#else
#if defined(ENABLE_AUTO_RESIZE)
        constexpr static uint64_t DEFAULT_MAPSIZE = 1LL << 29;
#else
        constexpr static uint64_t DEFAULT_MAPSIZE = 1LL << 33;
#endif
#endif
        constexpr static float RESIZE_PERCENT = 0.8f;
    };
}