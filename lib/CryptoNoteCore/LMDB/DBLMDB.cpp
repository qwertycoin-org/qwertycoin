// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Karbowanec developers
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

#include <algorithm>
#include <cstring>
#include <memory>
#include <random>

#include <boost/filesystem.hpp>

#include <Common/Util.h>
#include <Common/TimeUtils.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/LMDB/DBLMDB.h>
#include <CryptoNoteCore/LMDB/LMDBBlob.h>

#if defined(__i386) || defined(__x86_64)
#define MISALIGNED_OK    1
#endif

using namespace Common;
using namespace Crypto;
using namespace CryptoNote;
using namespace Logging;

#define VERSION 1

namespace {
#define MDBValSet(var, val) MDB_val var = {sizeof(val), (void *)&(val)}

    template<typename T>
    struct FMdbValCopy : MDB_val
    {
        FMdbValCopy(const T &t) : tCopy(t)
        {
            mv_size = sizeof(T);
            mv_data = &tCopy;
        }

    private:
        T tCopy;
    };

    template<>
    struct FMdbValCopy<CryptoNote::blobData> : public MDB_val
    {
        FMdbValCopy(const CryptoNote::blobData &bD) : sData(new char[bD.size()])
        {
            memcpy(sData.get(), bD.data(), bD.size());
            mv_size = bD.size();
            mv_data = sData.get();
        }

    private:
        std::unique_ptr<char[]> sData;
    };

    template<>
    struct FMdbValCopy<const char *> : public MDB_val
    {
        FMdbValCopy(const char *cS) :
                uSize(strlen(cS) + 1),
                sData(new char[uSize])
        {
            mv_size = uSize;
            mv_data = sData.get();
            memcpy(mv_data, cS, uSize);
        }

    private:
        size_t uSize;
        std::unique_ptr<char[]> sData;
    };

    /* DB schema:
     *
     * Table                 Key            Data
     * -----                 ---            ----
     * Blocks                block ID       block blob
     * BlockHeights          block hash     block height
     * BlockInfo             block ID       {block metadata}
     *
     * Transactions          Tx ID          Tx Blob
     * TransactionIndices    Tx Hash        {Tx Hash, metadata}
     * TransactionOutputs    Tx ID          {Tx Amount output indices}
     *
     * OutputTransactions    output ID      {txn hash, local index}
     * OutputAmounts         amount         [{amount output index, metadata}...]
     *
     * SpentKeys             input hash     -
     *
     * TransactionPoolMeta   Tx hash        Tx metadata
     * TransactionPoolBlob   Tx hash        Tx blob
     *
     * Note: where the data items are of uniform size, DUPFIXED tables have
     * been used to save space. In most of these cases, a dummy "zerokval"
     * key is used when accessing the table; the Key listed above will be
     * attached as a prefix on the Data to serve as the DUPSORT key.
     * (DUPFIXED saves 8 bytes per record.)
     *
     * The output_amounts table doesn't use a dummy key, but uses DUPSORT.
     */
    const char *const LMDB_BLOCKS = "Blocks";
    const char *const LMDB_BLOCK_HEIGHTS = "BlockHeights";
    const char *const LMDB_BLOCK_INFO = "BlockInfo";

    const char *const LMDB_TRANSACTIONS = "Transactions";
    const char *const LMDB_TRANSACTION_INDICES = "TransactionIndices";
    const char *const LMDB_TRANSACTION_OUTPUTS = "TransactionOutputs";

    const char *const LMDB_OUTPUT_TRANSACTIONS = "OutputTransactions";
    const char *const LMDB_OUTPUT_AMOUNTS = "OutputAmounts";
    const char *const LMDB_SPENT_KEYS = "SpentKeys";

    const char *const LMDB_TRANSACTIONPOOL_META = "TransactionPoolMeta";
    const char *const LMDB_TRANSACTIONPOOL_BLOB = "TransactionPoolBlob";

    const char *const LMDB_PROPERTIES = "Properties";

    const char cZeroKey[8] = {0};
    const MDB_val cZeroKVal = {sizeof(cZeroKey), (void *) cZeroKey};

    std::string lmdbError(const std::string &cErrorString, int iDBRes)
    {
        const std::string cFullString = cErrorString + mdb_strerror(iDBRes);

        return cFullString;
    }

    inline void lmdbOpen(MDB_txn *sTxn,
                         const char *cName,
                         int iFlags,
                         MDB_dbi &sDBi,
                         const std::string &cErrorString)
    {
        auto sDBRes = mdb_dbi_open(sTxn, cName, iFlags, &sDBi);
        if (sDBRes) {
            throw (DB_OPEN_FAILURE((lmdbError(cErrorString + " : ", sDBRes) +
                                    std::string(" - you may want to start with --db-salvage")).c_str()));
        } else {
            // std::cout << "lmdbOpen: " << cName << " opened. sDBRes: " << sDBRes << std::endl;
        }
    }
} // namespace

#define CURSOR(name)                                                                        \
    if (sCur ## name) {                                                                     \
        std::cout << "Print current cursor: " << sCur##name << std::endl;                   \
        std::cout << "Print current cursor as bool: " << !!sCur##name << std::endl;          \
    } else {                                                                                \
        int iRes = mdb_cursor_open(*mWriteDBTxnSafe, m ## name, &sCur ## name);             \
        if (iRes) {                                                                         \
            throw(DB_ERROR(lmdbError("Failed to open cursor: ", iRes).c_str()));            \
        }                                                                                   \
    }

#define READ_CURSOR(cName)                                                                  \
    if (!sCur ## cName) {                                                                   \
        int iResult = mdb_cursor_open(pTxn, m ## cName, (MDB_cursor **) &sCur ## cName);    \
        if (iResult) {                                                                      \
            throw(DB_ERROR(lmdbError("Failed to open cursor: ", iResult).c_str()));         \
        }                                                                                   \
        if (sCursor != &mWriteCursors) {                                                    \
            mTInfo->sTReadFlags.bRf ## cName = true;                                        \
        }                                                                                   \
    } else if (sCursor != &mWriteCursors && !mTInfo->sTReadFlags.bRf ## cName) {            \
        int iResult = mdb_cursor_renew(pTxn, sCur ## cName);                                \
        if (iResult) {                                                                      \
            throw(DB_ERROR(lmdbError("Failed to renew cursor: ", iResult).c_str()));        \
        }                                                                                   \
                                                                                            \
        mTInfo->sTReadFlags.bRf ## cName = true;                                            \
    }

namespace CryptoNote {
#define TXN_PREFIX(iFlags);                                                                 \
    FMdbTxnSafe sAutoTxn;                                                                   \
    FMdbTxnSafe *sTxnPtr = &sAutoTxn;                                                       \
    if (mBatchActive) {                                                                     \
        sTxnPtr = mWriteDBTxnSafe;                                                          \
    } else {                                                                                \
        if (auto sMdbResult = lmdbTxnBegin(mDbEnv, NULL, iFlags, sAutoTxn)) {               \
            throw(DB_ERROR(lmdbError(std::string(                                           \
            "Failed to create a transaction for the db in ")                                \
            +__FUNCTION__+": ", sMdbResult).c_str()));                                      \
        }                                                                                   \
    }

#define TXN_PREFIX_READONLY()                                                               \
    MDB_txn *pTxn;                                                                          \
    FMdbTxnCursors *sCursor;                                                               \
    FMdbTxnSafe sAutoTxn;                                                                   \
    bool bMyReadTxn = blockReadTxnStart(&pTxn, &sCursor);                                  \
    if (bMyReadTxn) {                                                                       \
        sAutoTxn.pThreadInfo = mTInfo.get();                                                \
    } else {                                                                                \
        sAutoTxn.uncheck();                                                                 \
    }

#define TXN_POSTFIX_READONLY()

#define TXN_POSTFIX_SUCCESS()                                                               \
    do {                                                                                    \
        if (!mBatchActive) {                                                                \
            sAutoTxn.commit();                                                              \
        }                                                                                   \
    } while(0);

#define TXN_BLOCK_PREFIX()


    int BlockchainLMDB::compareUInt64(const MDB_val *a, const MDB_val *b)
    {
        uint64_t va, vb;
        memcpy(&va, a->mv_data, sizeof(va));
        memcpy(&vb, b->mv_data, sizeof(vb));
        return (va < vb) ? -1 : va > vb;
    }

    int BlockchainLMDB::compareHash32(const MDB_val *a, const MDB_val *b)
    {
        uint32_t *va = (uint32_t *) a->mv_data;
        uint32_t *vb = (uint32_t *) b->mv_data;
        for (int n = 7; n >= 0; n--) {
            if (va[n] == vb[n]) {
                continue;
            }
            return va[n] < vb[n] ? -1 : 1;
        }

        return 0;
    }

    int BlockchainLMDB::compareString(const MDB_val *a, const MDB_val *b)
    {
        const char *va = (const char *) a->mv_data;
        const char *vb = (const char *) b->mv_data;
        return strcmp(va, vb);
    }

    typedef struct FBlockInfo
    {
        uint64_t uBIHeight;
        uint64_t uBITimestamp;
        uint64_t uBIGeneratedCoins;
        uint64_t uBISize;
        CryptoNote::difficulty_type uBIDifficulty;
        Crypto::Hash sBIHash;
    } FBlockInfo;

    typedef struct FBlockHeight
    {
        Crypto::Hash sHash;
        uint64_t uHeight;
    } FBlockHeight;

    typedef struct FTransactionIndex
    {
        Crypto::Hash key;
        FTxData data;
    } FTransactionIndex;

    typedef struct FOutputKey
    {
        uint64_t uAmountIndex;
        uint64_t uOutputId;
        FOutputData sData;
    } FOutputKey;

    typedef struct FOutTx
    {
        uint64_t output_id;
        Crypto::Hash tx_hash;
        uint64_t local_index;
    } FOutTx;

    std::atomic<uint64_t> FMdbTxnSafe::pNumActiveTxns{0};
    std::atomic_flag FMdbTxnSafe::pCreationGate = ATOMIC_FLAG_INIT;

    FMdbThreadInfo::~FMdbThreadInfo()
    {
        MDB_cursor **sCur = &sTReadCursors.sTxcBlocks;
        unsigned uI;

        for (uI = 0; uI < sizeof(FMdbTxnCursors) / sizeof(MDB_cursor *); uI++) {
            if (sCur[uI]) {
                mdb_cursor_close(sCur[uI]);
            }
        }

        if (sTReadTxn) {
            mdb_txn_abort(sTReadTxn);
        }
    }

    FMdbTxnSafe::FMdbTxnSafe(const bool bCheck) : pTxn(NULL), pThreadInfo(NULL), pCheck(bCheck)
    {
        if (bCheck) {
            while (pCreationGate.test_and_set()) {
            }
            pNumActiveTxns++;
            pCreationGate.clear();
        }

    }

    FMdbTxnSafe::~FMdbTxnSafe()
    {
        if (!pCheck) {
            return;
        }

        if (pThreadInfo != nullptr) {
            mdb_txn_reset(pThreadInfo->sTReadTxn);
            memset(&pThreadInfo->sTReadFlags, 0, sizeof(pThreadInfo->sTReadFlags));
        } else if (pTxn != nullptr) {
            if (pBatchTxn) {
            } else {
            }
            mdb_txn_abort(pTxn);
        }

        pNumActiveTxns--;
    }

    void FMdbTxnSafe::uncheck()
    {
        pNumActiveTxns--;
        pCheck = false;
    }

    void FMdbTxnSafe::commit(std::string cMessage)
    {
        if (cMessage.empty()) {
            cMessage = "Failed to commit a transaction to the db";
        }

        if (auto sResult = mdb_txn_commit(pTxn)) {
            pTxn = nullptr;
            throw (DB_ERROR(lmdbError(cMessage + ": ", sResult).c_str()));
        }

        pTxn = nullptr;
    }

    void FMdbTxnSafe::abort()
    {
        LoggerManager logManager;
        LoggerRef mLogger(logManager, "FMdbTxnSafe");

        std::cout << "FMdbTxnSafe: abort()" << std::endl;
        if (pTxn != nullptr) {
            mdb_txn_abort(pTxn);
            pTxn = nullptr;
        } else {
            std::cout << "WARNING: FMdbTxnSafe: abort() called, but pTxn is NULL" << std::endl;
        }
    }

    uint64_t FMdbTxnSafe::numActiveTx()
    {
        return pNumActiveTxns;
    }

    void FMdbTxnSafe::preventNewTxns()
    {
        while (pCreationGate.test_and_set()) {
        }
    }

    void FMdbTxnSafe::waitNoActiveTxns()
    {
        while (pNumActiveTxns > 0) {
        }
    }

    void FMdbTxnSafe::allowNewTxns()
    {
        pCreationGate.clear();
    }

    void lmdbResized(MDB_env *sEnv)
    {
        LoggerManager logManager;
        LoggerRef mLogger(logManager, "FMdbTxnSafe");
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        FMdbTxnSafe::preventNewTxns();

        mLogger(INFO, BRIGHT_CYAN) << "LMDB map resize detected.";

        MDB_envinfo sLMDBEnvInfo;
        mdb_env_info(sEnv, &sLMDBEnvInfo);
        uint64_t uOldSize = sLMDBEnvInfo.me_mapsize;

        FMdbTxnSafe::waitNoActiveTxns();
        mLogger(INFO, BRIGHT_CYAN) << "No active DB Txs active.";

        int iResult = mdb_env_set_mapsize(sEnv, 0);
        if (iResult) {
            throw (DB_ERROR(lmdbError("Failed to set new mapsize: ", iResult).c_str()));
        }

        mdb_env_info(sEnv, &sLMDBEnvInfo);
        uint64_t uNewSize = sLMDBEnvInfo.me_mapsize;
        mLogger(INFO, BRIGHT_CYAN) << "LMDB Mapsize increased." << "  Old: " << uOldSize / (1024 * 1024) << "MiB"
                                   << ", New: " << uNewSize / (1024 * 1024) << "MiB";

        FMdbTxnSafe::allowNewTxns();
    }

    inline int lmdbTxnBegin(MDB_env *sEnv,
                            MDB_txn *sParent,
                            unsigned int iFlags,
                            MDB_txn **sTxn)
    {
        int iResult = mdb_txn_begin(sEnv, sParent, iFlags, sTxn);
        if (iResult == MDB_MAP_RESIZED) {
            lmdbResized(sEnv);

            iResult = mdb_txn_begin(sEnv, sParent, iFlags, sTxn);
        }

        return iResult;
    }

    inline int lmdbTxnRenew(MDB_txn *sTxn)
    {
        int iResult = mdb_txn_renew(sTxn);
        if (iResult == MDB_MAP_RESIZED) {
            lmdbResized(mdb_txn_env(sTxn));

            iResult = mdb_txn_renew(sTxn);
        }

        return iResult;
    }

    void BlockchainLMDB::doResize(uint64_t uIncreaseSize)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        std::lock_guard<std::recursive_mutex> lockGuard(pSyncronizationLock);

        const uint64_t uAddSize = (size_t) 1 << 30;

        try {
            boost::filesystem::path sPath(mFolder);
            boost::filesystem::space_info sSpaceInfo = boost::filesystem::space(sPath);


            if (sSpaceInfo.available < uAddSize) {
                return;
            } else {
                MDB_envinfo sLMDBEnvInfo;
                mdb_env_info(mDbEnv, &sLMDBEnvInfo);
                MDB_stat sLMDBStat;
                mdb_env_stat(mDbEnv, &sLMDBStat);
                uint64_t uOldMapSize = sLMDBEnvInfo.me_mapsize;
                uint64_t uNewMapSize = (double) uOldMapSize + uAddSize;

                if (uIncreaseSize > 0) {
                    uNewMapSize = sLMDBEnvInfo.me_mapsize + uIncreaseSize;
                }

                uNewMapSize += (uNewMapSize % sLMDBStat.ms_psize);

                FMdbTxnSafe::preventNewTxns();

                if (mWriteDBTxnSafe != nullptr) {
                    if (mBatchActive) {
                        throw (DB_ERROR("LMDB resizing not yet supported when batch transactions enabled!"));
                    } else {
                        throw (DB_ERROR(
                                "attempting resize with write transaction in progress, this should not happen!"));
                    }
                }

                FMdbTxnSafe::waitNoActiveTxns();

                int iResult = mdb_env_set_mapsize(mDbEnv, uNewMapSize);
                if (iResult) {
                    throw (DB_ERROR(lmdbError("Failed to set new mapsize: ", iResult).c_str()));
                }

                mLogger(INFO, BRIGHT_CYAN) << "LMDB Mapsize increased." << "  Old: " << uOldMapSize / (1024 * 1024)
                                           << "MiB" << ", New: " << uNewMapSize / (1024 * 1024) << "MiB";

                FMdbTxnSafe::allowNewTxns();
            }
        } catch (std::exception &e) {
            mLogger(ERROR, BRIGHT_RED) << "Error during resizing: " << e.what();
        }

    }

    Crypto::Hash BlockchainLMDB::getTopBlockHash() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        uint64_t uHeight = height() - 1;

        if (uHeight > 0) {
            return getBlockHashFromHeight(uHeight);
        }

        return NULL_HASH;
    }

    CryptoNote::Block BlockchainLMDB::getTopBlock() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();
        uint64_t uHeight = height();

        CryptoNote::Block sBlock;
        if (uHeight > 0) {
            sBlock = getBlockFromHeight(uHeight -1);
        }

        return sBlock;
    }

    uint64_t BlockchainLMDB::height() const
    {
        // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        checkOpen();
        TXN_PREFIX_READONLY();
        int iResult;

        // Get current height
        MDB_stat sDBStats;
        iResult = mdb_stat(pTxn, mBlocks, &sDBStats);
        if (iResult) {
            throw (DB_ERROR(lmdbError("Failed to query m_blocks: ", iResult).c_str()));
        }

        return sDBStats.ms_entries;
    }

    uint64_t BlockchainLMDB::numOutputs() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        int iResult;

        MDB_stat sDBStats;
        if ((iResult = mdb_stat(pTxn, mOutputTransactions, &sDBStats))) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". bad iResult: " << iResult;
            throw (DB_ERROR(lmdbError("Failed to query mOutputTransactions: ", iResult).c_str()));
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". good iResult: " << iResult;

        uint64_t uRetVal = sDBStats.ms_entries;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uRetVal: " << uRetVal;

        return uRetVal;
    }

    bool BlockchainLMDB::transactionExists(const Crypto::Hash &sHash) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(TransactionIndices);

        MDBValSet(sValHash, sHash);
        bool bTransactionFound = false;

        TIME_MEASURE_START(txTime1);
        auto getResult = mdb_cursor_get(sCurTransactionIndices, (MDB_val *) &cZeroKVal, &sValHash, MDB_GET_BOTH);
        if (getResult == 0) {
            bTransactionFound = true;
        } else if (getResult != MDB_NOTFOUND) {
            throw (DB_ERROR(lmdbError(
                    std::string("DB error attempting to fetch transaction index from hash ") + Common::podToHex(sHash) +
                    ": ", getResult).c_str()));
        }

        TIME_MEASURE_FINISH(txTime1);
        gTimeTxExists += txTime1;

        TXN_POSTFIX_READONLY();

        if (!bTransactionFound) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << "Transaction with hash " << sHash << "not found in database";
            return false;
        }

        return true;
    }

    bool BlockchainLMDB::transactionExists(const Crypto::Hash &sHash, uint64_t &uTransactionId) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(TransactionIndices);

        MDBValSet(sValHash, sHash);

        TIME_MEASURE_START(txTime1);
        auto getResult = mdb_cursor_get(sCurTransactionIndices, (MDB_val *) &cZeroKVal, &sValHash, MDB_GET_BOTH);
        TIME_MEASURE_FINISH(txTime1);
        gTimeTxExists += txTime1;

        if (!getResult) {
            FTransactionIndex *sTIx = (FTransactionIndex *) sValHash.mv_data;
            uTransactionId = sTIx->data.uTxID;
        }

        TXN_POSTFIX_READONLY();

        bool bRet = false;
        if (getResult == MDB_NOTFOUND) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << "Transaction with hash " << sHash << "not found in database";
        } else if (getResult) {
            throw (DB_ERROR(lmdbError("DB error attempting to fetch transaction from hash", getResult).c_str()));
        } else {
            bRet = true;
        }

        return bRet;
    }

    bool BlockchainLMDB::getTransactionBlob(const Crypto::Hash &sHash,
                                            CryptoNote::blobData &sTransactionBlob) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(TransactionIndices);
        READ_CURSOR(Transactions);

        MDBValSet(sValHash, sHash);
        MDB_val sResult;

        auto getResult = mdb_cursor_get(sCurTransactionIndices, (MDB_val *) &cZeroKVal, &sValHash, MDB_GET_BOTH);
        if (getResult == 0) {
            FTransactionIndex *sTIx = (FTransactionIndex *) sValHash.mv_data;
            MDBValSet(sValTxId, sTIx->data.uTxID);
            getResult = mdb_cursor_get(sCurTransactions, &sValTxId, &sResult, MDB_SET);
        }

        if (getResult == MDB_NOTFOUND) {
            return false;
        } else if (getResult) {
            throw (DB_ERROR(lmdbError("DB error attempting to fetch tx from hash", getResult).c_str()));
        }

        sTransactionBlob.assign(reinterpret_cast<char *>(sResult.mv_data), sResult.mv_size);

        TXN_POSTFIX_READONLY();

        return true;
    }

    uint64_t BlockchainLMDB::getTransactionCount() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        checkOpen();
        TXN_PREFIX_READONLY();
        int iResult;

        MDB_stat sDBStats;
        if (iResult = mdb_stat(pTxn, mTransactions, &sDBStats)) {
            throw (DB_ERROR(lmdbError("Failed to query m_txs: ", iResult).c_str()));
        }

        TXN_POSTFIX_READONLY();

        return sDBStats.ms_entries;
    }

    uint64_t BlockchainLMDB::addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                                      const CryptoNote::difficulty_type &uCumulativeDifficulty,
                                      const uint64_t &uCoinsGenerated,
                                      const std::vector<CryptoNote::Transaction> &transactions)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "0";

        checkOpen();
        uint64_t uHeight = height();

        try {
            BlockchainDB::addBlock(block, uBlockSize, uCumulativeDifficulty, uCoinsGenerated, transactions);
        } catch (const DB_ERROR_TXN_START) {
            throw;
        }
        catch (...) {
            blockTxnAbort();
            throw;
        }
        return ++uHeight;
    }

    void BlockchainLMDB::addBlock(const CryptoNote::Block &block,
                                  const size_t &uBlockSize,
                                  const CryptoNote::difficulty_type &uCumulativeDifficulty,
                                  const uint64_t &uCoinsGenerated,
                                  const Crypto::Hash &sBlockHash)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1";

        checkOpen();
        {
            FMdbTxnCursors *sCursor = &mWriteCursors;
            uint64_t uHeight = height();

            CURSOR(BlockHeights)

            FBlockHeight sBlockHeight = {sBlockHash, uHeight};
            MDBValSet(sValHeight, sBlockHeight);
            if (mdb_cursor_get(sCurBlockHeights, (MDB_val *) &cZeroKVal, &sValHeight, MDB_GET_BOTH) == 0) {
                throw (BLOCK_EXISTS("Attempting to add block that's already in the db"));
            }

            if (uHeight > 0) {
                MDBValSet(sParentKey, block.previousBlockHash);
                int iResult = mdb_cursor_get(sCurBlockHeights, (MDB_val *) &cZeroKVal, &sParentKey, MDB_GET_BOTH);
                if (iResult) {
                    mLogger(DEBUGGING, BRIGHT_CYAN) << "uHeight: " << uHeight;
                    mLogger(DEBUGGING, BRIGHT_CYAN) << "sParentKey: " << block.previousBlockHash;
                    throw (DB_ERROR(lmdbError("Failed to get top block hash to check for new block's parent: ",
                                              iResult).c_str()));
                }

                FBlockHeight *sPrevHeight = (FBlockHeight *) sParentKey.mv_data;
                if (sPrevHeight->uHeight != uHeight - 1) {
                    throw (BLOCK_PARENT_DNE("Top block is not new block's parent"));
                }
            }

            int iResult = 0;

            MDBValSet(sValKey, uHeight);

            CURSOR(Blocks)
            CURSOR(BlockInfo)

            CryptoNote::blobData sBlockBlob;
            if (!blockToBlob(block, sBlockBlob)) {
                mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. !blockToBlob";
            }
            /*
            CryptoNote::Block sTempBlock;
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. sBlockBlob: " << sBlockBlob << ENDL;
            if (!parseAndValidateBlockFromBlob(sBlockBlob, sTempBlock)) {
                mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. !parseAndValidateBlockFromBlob";
            }
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. sTempBlock timestamp: " << sTempBlock.timestamp;
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. sTempBlock transactionHashes: " << sTempBlock.transactionHashes.size();
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. block timestamp: " << block.timestamp;
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1. block transactionHashes: " << block.transactionHashes.size();
             */
            // This call to mdb_cursor_put will change height()
            FMdbValCopy<blobData> cBlob(sBlockBlob);
            iResult = mdb_cursor_put(sCurBlocks, &sValKey, &cBlob, MDB_APPEND);
            if (iResult) {
                throw (DB_ERROR(lmdbError("Failed to add block blob to db transaction: ", iResult).c_str()));
            }

            FBlockInfo sBlockInfo;
            sBlockInfo.uBIHeight = uHeight;
            sBlockInfo.uBITimestamp = block.timestamp;
            sBlockInfo.uBIGeneratedCoins = uCoinsGenerated;
            sBlockInfo.uBIDifficulty = uCumulativeDifficulty;
            sBlockInfo.sBIHash = sBlockHash;

            MDBValSet(sValBlockInfo, sBlockInfo);
            iResult = mdb_cursor_put(sCurBlockInfo, (MDB_val *) &cZeroKVal, &sValBlockInfo, MDB_APPENDDUP);
            if (iResult) {
                throw (DB_ERROR(lmdbError("Failed to add block info to db transaction: ", iResult).c_str()));
            }

            iResult = mdb_cursor_put(sCurBlockHeights, (MDB_val *) &cZeroKVal, &sValHeight, 0);
            if (iResult) {
                throw (DB_ERROR(lmdbError("Failed to add block height by hash to db transaction: ", iResult).c_str()));
            }

            mCumSize += uBlockSize;
            mCumCount++;
        }
    }

    bool BlockchainLMDB::blockExists(const Crypto::Hash &sHash, uint64_t *height) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(BlockHeights);

        bool bReturn = false;
        MDBValSet(sValHash, sHash);
        auto getResult = mdb_cursor_get(sCurBlockHeights, (MDB_val *) &cZeroKVal, &sValHash, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << " Block with hash " << sHash << " not found in DB";
        } else if (getResult) {
            throw (DB_ERROR(lmdbError("DB error attempting to fetch block index from hash", getResult).c_str()));
        } else {
            if (height) {
                const auto *sBlockHeight = (const FBlockHeight *) sValHash.mv_data;
                *height = sBlockHeight->uHeight;
            }

            bReturn = true;
        }

        TXN_POSTFIX_READONLY();

        return bReturn;
    }

    CryptoNote::blobData BlockchainLMDB::getBlockBlob(const Crypto::Hash &sHash) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        return getBlockBlobFromHeight(getBlockHeight(sHash));
    }

    uint64_t BlockchainLMDB::getBlockHeight(const Crypto::Hash &sHash) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(BlockHeights);

        MDBValSet(sValHash, sHash);
        auto getResult = mdb_cursor_get(sCurBlockHeights, (MDB_val *) &cZeroKVal, &sValHash, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw (BLOCK_DNE("Attempted to retrieve non-existent block height"));
        } else if (getResult) {
            throw (DB_ERROR("Error attempting to retrieve a block height from the db"));
        }

        FBlockHeight *sBHe = (FBlockHeight *) sValHash.mv_data;
        uint64_t uRet = sBHe->uHeight;

        TXN_POSTFIX_READONLY();

        return uRet;
    }

    CryptoNote::BlockHeader BlockchainLMDB::getBlockHeader(const Crypto::Hash &sHash) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        // BlockHeader object is automatically cast from block object
        return getBlock(sHash);
    }

    CryptoNote::blobData BlockchainLMDB::getBlockBlobFromHeight(const uint64_t &uHeight) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uHeight: " << uHeight;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(Blocks);

        FMdbValCopy<uint64_t> sKeyHeight(uHeight);
        MDB_val sResult;
        auto getResult = mdb_cursor_get(sCurBlocks, &sKeyHeight, &sResult, MDB_SET);
        if (getResult == MDB_NOTFOUND) {
            throw (BLOCK_DNE(std::string("Attempt to get block from height ").append(
                    boost::lexical_cast<std::string>(uHeight)).append(" failed -- block not in db").c_str()));
        } else if (getResult) {
            throw (DB_ERROR("Error attempting to retrieve a block from the db"));
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". Before blob assign.";
        CryptoNote::blobData sBlobData;
        sBlobData.assign(reinterpret_cast<char *>(sResult.mv_data), sResult.mv_size);
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". After blob assign.";

        TXN_POSTFIX_READONLY();

        return sBlobData;
    }

    size_t BlockchainLMDB::getBlockSize(const uint64_t &uHeight) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(BlockInfo);

        MDBValSet(sValHeight, uHeight);
        auto getResult = mdb_cursor_get(sCurBlockInfo, (MDB_val *) &cZeroKVal, &sValHeight, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw (BLOCK_DNE(std::string("Attempt to get block size from height ").append(
                    boost::lexical_cast<std::string>(uHeight)).append(" failed -- block size not in db").c_str()));
        } else if (getResult) {
            throw (DB_ERROR("Error attempting to retrieve a block size from the db"));
        }

        FBlockInfo *sBIn = (FBlockInfo *) sValHeight.mv_data;
        size_t uRet = sBIn->uBISize;

        TXN_POSTFIX_READONLY();

        return uRet;
    }

    CryptoNote::difficulty_type BlockchainLMDB::getBlockCumulativeDifficulty(const uint64_t &uHeight) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(BlockInfo);

        MDBValSet(sValHeight, uHeight);
        auto getResult = mdb_cursor_get(sCurBlockInfo, (MDB_val *) &cZeroKVal, &sValHeight, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw (BLOCK_DNE(std::string("Attempt to get cumulative difficulty from height ").append(
                    boost::lexical_cast<std::string>(uHeight)).append(" failed -- block size not in db").c_str()));
        } else if (getResult) {
            throw (DB_ERROR("Error attempting to retrieve a cumulative difficulty from the db"));
        }

        FBlockInfo *sBI = (FBlockInfo *) sValHeight.mv_data;
        uint64_t uRet = sBI->uBIDifficulty;

        TXN_POSTFIX_READONLY();

        return uRet;
    }

    CryptoNote::difficulty_type BlockchainLMDB::getBlockDifficulty(const uint64_t &uHeight) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        uint64_t uDiff1 = 0;
        uint64_t uDiff2 = 0;

        uDiff1 = getBlockCumulativeDifficulty(uHeight);

        if (uHeight > 0) {
            uDiff2 = getBlockCumulativeDifficulty(uHeight - 1);
        }

        return (uDiff1 - uDiff2);
    }

    uint64_t BlockchainLMDB::getBlockAlreadyGeneratedCoins(const uint64_t &uHeight) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(BlockInfo);

        MDBValSet(sValHeight, uHeight);
        auto getResult = mdb_cursor_get(sCurBlockInfo, (MDB_val *) &cZeroKVal, &sValHeight, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw (BLOCK_DNE(std::string("Attempt to get generated coins from height ").append(
                    boost::lexical_cast<std::string>(uHeight)).append(" failed -- block size not in db").c_str()));
        } else if (getResult) {
            throw (DB_ERROR("Error attempting to retrieve a total generated coins from the db"));
        }

        FBlockInfo *sBI = (FBlockInfo *) sValHeight.mv_data;
        uint64_t uRet = sBI->uBIGeneratedCoins;

        TXN_POSTFIX_READONLY();

        return uRet;
    }

    Crypto::Hash BlockchainLMDB::getBlockHashFromHeight(const uint64_t &uHeight) const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        TXN_PREFIX_READONLY();
        READ_CURSOR(BlockInfo);

        MDBValSet(sValHeight, uHeight);
        auto getResult = mdb_cursor_get(sCurBlockInfo, (MDB_val *) &cZeroKVal, &sValHeight, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw (BLOCK_DNE(std::string("Attempt to get block hash from height ").append(
                    boost::lexical_cast<std::string>(uHeight)).append(" failed -- block size not in db").c_str()));
        } else if (getResult) {
            throw (DB_ERROR("Error attempting to retrieve a block hash from the db"));
        }

        FBlockInfo *sBI = (FBlockInfo *) sValHeight.mv_data;
        Crypto::Hash sRet = sBI->sBIHash;

        TXN_POSTFIX_READONLY();

        return sRet;

    }

    void BlockchainLMDB::removeBlock()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    }

    uint64_t BlockchainLMDB::addTransactionData(const Crypto::Hash &sBlockHash,
                                                const CryptoNote::Transaction &sTransaction,
                                                const Crypto::Hash &sTxHash)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        FMdbTxnCursors *sCursor = &mWriteCursors;
        uint64_t uHeight = height();

        int iResult;
        uint64_t uTxId = getTransactionCount();

        CURSOR(Transactions)
        CURSOR(TransactionIndices)

        MDBValSet(sValTxId, uTxId);
        MDBValSet(sValTxHash, sTxHash);

        iResult = mdb_cursor_get(sCurTransactionIndices, (MDB_val *) &cZeroKVal, &sValTxHash, MDB_GET_BOTH);

        if (iResult == 0) {
            FTransactionIndex *sTxIndexP = (FTransactionIndex *) sValTxHash.mv_data;
            {}
        } else if (iResult != MDB_NOTFOUND) {
            {}
        }

        FTransactionIndex sTxIndex;
        sTxIndex.key = sTxHash;
        sTxIndex.data.uTxID = uTxId;
        sTxIndex.data.uUnlockTime = sTransaction.unlockTime;
        sTxIndex.data.uBlockID = uHeight;

        sValTxHash.mv_size = sizeof(sTxIndex);
        sValTxHash.mv_data = (void *) &sTxIndex;

        iResult = mdb_cursor_put(sCurTransactionIndices, (MDB_val *) &cZeroKVal, &sValTxHash, 0);
        if (iResult) {
            throw (DB_ERROR(lmdbError("Failed to add tx data to db transaction: ", iResult).c_str()));
        }

        FMdbValCopy<blobData> cBlob(transactionToBlob(sTransaction));
        iResult = mdb_cursor_put(sCurTransactions, &sValTxId, &cBlob, MDB_APPEND);
        if (iResult) {
            throw (DB_ERROR(lmdbError("Failed to add tx blob to db transaction: ", iResult).c_str()));
        }

        return uTxId;
    }

    void BlockchainLMDB::removeTransactionData(const Crypto::Hash &sTxHash,
                                               const CryptoNote::Transaction &tx)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    }

    uint64_t BlockchainLMDB::addOutput(const Crypto::Hash &sTxHash,
                                       const CryptoNote::TransactionOutput &sTxOutput,
                                       const uint64_t &uIndex,
                                       const uint64_t &uUnlockTime)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();

        FMdbTxnCursors *sCursor = &mWriteCursors;

        uint64_t uHeight = height();
        uint64_t uNumOutputs = numOutputs();
        int result = 0;

        CURSOR(OutputTransactions)

        FOutTx sOutTx = {uNumOutputs, sTxHash, uIndex};
        MDBValSet(sValOutTx, sOutTx);
        result = mdb_cursor_put(sCurOutputTransactions, (MDB_val *) &cZeroKVal, &sValOutTx,
                                MDB_APPENDDUP);
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". result sCurOutputTransactions: "
                                        << result;
        if (result) {
            throw (DB_ERROR(lmdbError("Failed to add output tx hash to db transaction: ", result).c_str()));
        }

        CURSOR(OutputAmounts)

        FOutputKey sOutKey;
        MDB_val sData;
        FMdbValCopy<uint64_t> sValAmount(sTxOutput.amount);
        result = mdb_cursor_get(sCurOutputAmounts, &sValAmount, &sData, MDB_SET);
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". result sCurOutputAmounts: "
                                        << result;
        if (!result) {
            uint64_t uNumElements = 0;
            result = mdb_cursor_count(sCurOutputAmounts, &uNumElements);
            if (result) {
                throw (DB_ERROR(std::string("Failed to get number of outputs for amount: ").append(
                        mdb_strerror(result)).c_str()));
            }

            sOutKey.uAmountIndex = uNumElements;
        } else if (result != MDB_NOTFOUND) {
            throw (DB_ERROR(lmdbError("Failed to get output amount in db transaction: ", result).c_str()));
        } else {
            sOutKey.uAmountIndex = 0;
        }

        if (sTxOutput.target.type() == typeid(KeyOutput)) {
            sOutKey.sData.sPublicKey = boost::get<KeyOutput>(sTxOutput.target).key;
        } else if (sTxOutput.target.type() == typeid(KeyOutput)) {
            throw (DB_ERROR(std::string("Transaction output should be of type KeyOutput!").c_str()));
        }

        // sOutKey.sData.sPublicKey = boost::get<MultisignatureOutput>(sTxOutput.target).keys;

        sOutKey.uOutputId = uNumOutputs;
        sOutKey.sData.uUnlockTime = uUnlockTime;
        sOutKey.sData.uHeight = uHeight;
        sData.mv_size = sizeof(FOutputKey);
        sData.mv_data = &sOutKey;

        if ((result = mdb_cursor_put(sCurOutputAmounts, &sValAmount, &sData, MDB_APPENDDUP))) {
            throw (DB_ERROR(lmdbError("Failed to add output pubkey to db transaction: ", result).c_str()));
        }

        return sOutKey.uAmountIndex;

    }

    void BlockchainLMDB::addTransactionAmountOutputIndices(const uint64_t uTxId,
                                                           const std::vector<uint64_t> &vAmountOutputIndices)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();
        FMdbTxnCursors *sCursor = &mWriteCursors;

        CURSOR(TransactionOutputs)

        int result = 0;
        int iNumOutputs = vAmountOutputIndices.size();

        MDBValSet(sValTxId, uTxId);
        MDB_val sVal;
        sVal.mv_data = (void *) vAmountOutputIndices.data();
        sVal.mv_size = sizeof(uint64_t) * iNumOutputs;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                        << ". tx_outputs[tx_hash] size: "
                                        << sVal.mv_size;

        result = mdb_cursor_put(sCurTransactionOutputs, (MDB_val *) &cZeroKVal, &sVal, MDB_APPEND);
        if (result) {
            throw (DB_ERROR(
                    std::string("Failed to add <tx hash, amount output index array> to db transaction: ").append(
                            mdb_strerror(result)).c_str()));
        }
    }

    void BlockchainLMDB::addSpentKey(const KeyImage &sSpentKeyImage)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();
        FMdbTxnCursors *sCursor = &mWriteCursors;

        CURSOR(SpentKeys)

        MDB_val sKey = {sizeof(sSpentKeyImage), (void *) &sSpentKeyImage};
        if (auto result = mdb_cursor_put(sCurSpentKeys, (MDB_val *) &cZeroKVal, &sKey, MDB_NODUPDATA)) {
            if (result == MDB_KEYEXIST) {
                throw (KEY_IMAGE_EXISTS("Attempting to add spent key image that's already in the db"));
            } else {
                throw (DB_ERROR(lmdbError("Error adding spent key image to db transaction: ", result).c_str()));
            }
        }
    }

    void BlockchainLMDB::removeSpentKey(const Crypto::KeyImage &sSpentKeyImage)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        checkOpen();
        FMdbTxnCursors *sCursor = &mWriteCursors;

        CURSOR(SpentKeys)

        MDB_val sKey = {sizeof(sSpentKeyImage), (void *) &sSpentKeyImage};
        auto result = mdb_cursor_get(sCurSpentKeys, (MDB_val *) &cZeroKVal, &sKey, MDB_GET_BOTH);
        if (result != 0 && result != MDB_NOTFOUND) {
            throw (DB_ERROR(lmdbError("Error finding spent key to remove", result).c_str()));
        }

        if (!result) {
            result = mdb_cursor_del(sCurSpentKeys, 0);
            if (result) {
                throw (DB_ERROR(lmdbError("Error adding removal of key image to db transaction", result).c_str()));
            }
        }
    }

    void BlockchainLMDB::checkOpen() const
    {
        // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        if (!pOpen) {
            throw DB_ERROR("DB operation attempted on a not-open DB instance");
        }
    }

    BlockchainLMDB::~BlockchainLMDB()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        if (mBatchActive) {
            try {
                batchAbort();
            } catch (...) {

            }

            if (pOpen) {
                close();
            }
        }
    }

    BlockchainLMDB::BlockchainLMDB(bool bBatchTransactions, Logging::ILogger &sLogger)
            : BlockchainDB(),
              mLogger(sLogger, "LMDB")
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        mFolder = "thishsouldnotexistbecauseitisgibberish";

        mBatchTransactions = bBatchTransactions;
        mWriteDBTxnSafe = nullptr;
        mWriteBatchDBTxnSafe = nullptr;
        mBatchActive = false;
        mCumSize = 0;
        mCumCount = 0;
    }

    std::vector<std::string> BlockchainLMDB::getFileNames() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        std::vector<std::string> vFileNames;

        boost::filesystem::path sDataFile(mFolder);
        sDataFile /= CryptoNote::parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME;
        boost::filesystem::path sLockFile(mFolder);
        sLockFile /= CryptoNote::parameters::CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME;

        vFileNames.push_back(sDataFile.string());
        vFileNames.push_back(sLockFile.string());

        return vFileNames;
    }

    std::string BlockchainLMDB::getDBName() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        return Tools::getDefaultDBType();
    }

    void BlockchainLMDB::open(const std::string &cFileName, const int iDBFlags)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        int iResult;
        int iMdbFlags = MDB_NORDAHEAD;

        if (pOpen) {
            throw DB_OPEN_FAILURE("Attempted to open db, but it's already open");
        }

        boost::filesystem::path sDir(cFileName);
        if (boost::filesystem::exists(sDir)) {
            if (!boost::filesystem::is_directory(sDir)) {
                throw DB_OPEN_FAILURE("LMDB needs a directory path, but a file was passed");
            }
        } else {
            if (!boost::filesystem::create_directories(sDir)) {
                throw DB_OPEN_FAILURE(std::string("Failed to create directory ").append(cFileName).c_str());
            }
        }

        // Check for existing LMDB files in base directory
        boost::filesystem::path sOldFiles = sDir.parent_path();
        if (boost::filesystem::exists(sOldFiles / parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME) ||
            boost::filesystem::exists(sOldFiles / parameters::CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME)) {
            mLogger(INFO, BRIGHT_CYAN) << "Found existing LMDB files in " << sOldFiles.string();
            mLogger(INFO, BRIGHT_CYAN) << "Move " << parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME << " and/or "
                                       << parameters::CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME << " to " << cFileName
                                       << ", or delete them, and then restart";

            throw DB_ERROR("Database could not be opened");
        }

        mFolder = cFileName;

#ifdef __OpenBSD__
        if ((iMdbFlags & MDB_WRITEMAP) == 0) {
            iMdbFlags |= MDB_WRITEMAP;
        }
#endif

        // Set up LMDB environment
        if ((iResult = mdb_env_create(&mDbEnv))) {
            throw DB_ERROR(lmdbError("Failed to create lmdb environment: ", iResult).c_str());
        }

        if ((iResult = mdb_env_set_maxdbs(mDbEnv, 32))) {
            throw DB_ERROR(lmdbError("Failed to set max number of dbs: ", iResult).c_str());
        }

        int iThreads = 8;
        if (iThreads > 110 && (iResult = mdb_env_set_maxreaders(mDbEnv, iThreads + 16))) {
            throw DB_ERROR(lmdbError("Failed to set max number of readers: ", iResult).c_str());
        }

        size_t uMapSize = DEFAULT_MAPSIZE;

        if (iDBFlags & DBF_FAST) {
            iMdbFlags |= MDB_NOSYNC;
        }

        if (iDBFlags & DBF_FASTEST) {
            iMdbFlags |= MDB_NOSYNC | MDB_WRITEMAP | MDB_MAPASYNC;
        }

        if (iDBFlags & DBF_RDONLY) {
            iMdbFlags |= MDB_RDONLY;
        }

        if (iDBFlags & DBF_SALVAGE) {
            iMdbFlags |= MDB_PREVSNAPSHOT;
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". iMdbFlags: " << iMdbFlags;

        if (auto sResult = mdb_env_open(mDbEnv, cFileName.c_str(), iMdbFlags, 0644)) {
            throw DB_ERROR(lmdbError("Failed to open lmdb environment: ", sResult).c_str());
        }

        MDB_envinfo sMEnvInfo;
        mdb_env_info(mDbEnv, &sMEnvInfo);
        uint64_t uCurMapSize = sMEnvInfo.me_mapsize;

        if (uCurMapSize < uMapSize) {
            if (auto sResult = mdb_env_set_mapsize(mDbEnv, uMapSize)) {
                throw DB_ERROR(lmdbError("Failed to set max memory map size: ", sResult).c_str());
            }

            mdb_env_info(mDbEnv, &sMEnvInfo);
            uCurMapSize = sMEnvInfo.me_mapsize;
            mLogger(INFO, BRIGHT_CYAN) << "LMDB memory map size: " << uCurMapSize;
        }

        // TODO: Add resize here

        int iTxnFlags = 0;
        if (iMdbFlags & MDB_RDONLY) {
            iTxnFlags |= MDB_RDONLY;
        }

        // Get a read/write MDB_txn, depending on iMdbFlags
        FMdbTxnSafe sTxn;
        if (auto iMdbRes = mdb_txn_begin(mDbEnv, nullptr, iTxnFlags, sTxn)) {
            throw DB_ERROR(lmdbError("Failed to create a transaction for the db: ", iMdbRes).c_str());
        }

        // Open necessary databases, and set properties as needed
        // uses macros to avoid having to change things too many places
        lmdbOpen(sTxn, LMDB_BLOCKS, MDB_INTEGERKEY | MDB_CREATE, mBlocks, "Failed to open db handle for mBlocks");
        lmdbOpen(sTxn, LMDB_BLOCK_INFO, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mBlockInfo,
                 "Failed to open db handle for mBlockInfo");
        lmdbOpen(sTxn, LMDB_BLOCK_HEIGHTS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mBlockHeights,
                 "Failed to open db handle for mBlockHeights");

        lmdbOpen(sTxn, LMDB_TRANSACTIONS, MDB_INTEGERKEY | MDB_CREATE, mTransactions,
                 "Failed to open db handle for mTransactions");
        lmdbOpen(sTxn, LMDB_TRANSACTION_INDICES, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
                 mTransactionIndices, "Failed to open db handle for mTransactionIndices");
        lmdbOpen(sTxn, LMDB_TRANSACTION_OUTPUTS, MDB_INTEGERKEY | MDB_CREATE, mTransactionOutputs,
                 "Failed to open db handle for mTransactionOutputs");

        lmdbOpen(sTxn, LMDB_OUTPUT_TRANSACTIONS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
                 mOutputTransactions, "Failed to open db handle for mOutputTransactions");
        lmdbOpen(sTxn, LMDB_OUTPUT_AMOUNTS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mOutputAmounts,
                 "Failed to open db handle for mOutputTransactions");

        lmdbOpen(sTxn, LMDB_SPENT_KEYS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mSpentKeys,
                 "Failed to open db handle for mSpentKeys");

        lmdbOpen(sTxn, LMDB_TRANSACTIONPOOL_META, MDB_CREATE, mTransactionPoolMeta,
                 "Failed to open db handle for mTransactionPoolMeta");
        lmdbOpen(sTxn, LMDB_TRANSACTIONPOOL_BLOB, MDB_CREATE, mTransactionPoolBlob,
                 "Failed to open db handle for mTransactionPoolBlob");

        lmdbOpen(sTxn, LMDB_PROPERTIES, MDB_CREATE, mProperties, "Failed to open db handle for mProperties");

        mdb_set_dupsort(sTxn, mSpentKeys, compareHash32);
        mdb_set_dupsort(sTxn, mBlockHeights, compareHash32);
        mdb_set_dupsort(sTxn, mTransactionIndices, compareHash32);
        mdb_set_dupsort(sTxn, mOutputAmounts, compareUInt64);
        mdb_set_dupsort(sTxn, mOutputTransactions, compareUInt64);
        mdb_set_dupsort(sTxn, mBlockInfo, compareUInt64);

        mdb_set_compare(sTxn, mTransactionPoolMeta, compareHash32);
        mdb_set_compare(sTxn, mTransactionPoolBlob, compareHash32);
        mdb_set_compare(sTxn, mProperties, compareString);

        /* do we need the hardfork stuff?
        if (!(iMdbFlags & MDB_RDONLY)) {
            iResult = mdb_drop(sTxn, )
        }
         */

        MDB_stat iLMDBStats;
        if ((iResult = mdb_stat(sTxn, mBlocks, &iLMDBStats))) {
            throw DB_ERROR(lmdbError("Failed to query mBlocks: ", iResult).c_str());
        }

        mLogger(INFO, BRIGHT_CYAN) << "Setting uHeight to: " << iLMDBStats.ms_entries;

        uint64_t uHeight = iLMDBStats.ms_entries;
        bool bCompatible = true;

        FMdbValCopy<const char *> sValCopy("version");
        MDB_val sV;
        auto sGetResult = mdb_get(sTxn, mProperties, &sValCopy, &sV);

        if (sGetResult == MDB_SUCCESS) {
            if (*(const uint32_t *) sV.mv_data > VERSION) {
                bCompatible = false;
            }

#if VERSION > 0
            else if (*(const uint32_t *) sV.mv_data < VERSION) {
                sTxn.commit();
                pOpen = true;

                // TODO: add later
                // migrate(*(const uint32_t*)sV.mv_data);
                return;
            }
#endif
        } else {
            if (VERSION > 0 && uHeight > 0) {
                bCompatible = false;
            }
        }

        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". bCompatible: " << bCompatible;
        if (!bCompatible) {
            sTxn.abort();
            mdb_env_close(mDbEnv);
            pOpen = false;
            mLogger(ERROR, BRIGHT_RED) << "Existing lmdb database is incompatible with this version.";
            mLogger(ERROR, BRIGHT_RED) << "Please delete the existing database and resync.";

            return;
        }

        if (!(iMdbFlags & MDB_RDONLY)) {
            // Only write version on an empty DB
            if (uHeight == 0) {
                FMdbValCopy<const char *> sK("version");
                FMdbValCopy<uint32_t> sV(VERSION);
                auto sPutResult = mdb_put(sTxn, mProperties, &sK, &sV, 0);
                if (sPutResult != MDB_SUCCESS) {
                    sTxn.abort();
                    mdb_env_close(mDbEnv);
                    pOpen = false;
                    mLogger(ERROR, BRIGHT_RED) << "Failed to write version to database.";

                    return;
                } else {
                    mLogger(DEBUGGING, BRIGHT_GREEN) << "Successfully written Version " << VERSION << " to database";
                }
            }
        }

        // Commit the transaction
        sTxn.commit();

        pOpen = true;
        // Finished init
    }

    void BlockchainLMDB::close()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        if (mBatchActive) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << "close() first calling batch_abort() due to active batch transaction";
            batchAbort();
        }

        this->sync();
        mTInfo.reset();

        mdb_env_close(mDbEnv);
        pOpen = false;
    }

    void BlockchainLMDB::sync()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        checkOpen();

        // Does nothing unless LMDB environment was opened with MDB_NOSYNC or in part
        // MDB_NOMETASYNC. Force flush to be synchronous.
        if (auto sResult = mdb_env_sync(mDbEnv, true)) {
            throw (DB_ERROR(lmdbError("Failed to sync database: ", sResult).c_str()));
        }
    }

    void BlockchainLMDB::safeSyncMode(const bool bOnOff)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "switching safe mode " << (bOnOff ? "on" : "off");
        mdb_env_set_flags(mDbEnv, MDB_NOSYNC | MDB_MAPASYNC, !bOnOff);
    }

    void BlockchainLMDB::reset()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        checkOpen();

        FMdbTxnSafe sTxn;

        if (auto sResult = lmdbTxnBegin(mDbEnv, NULL, 0, sTxn)) {
            throw (DB_ERROR(lmdbError("Failed to create a transaction for the db: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mBlocks, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mBlocks: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mBlockInfo, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mBlockInfo: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mBlockHeights, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mBlockHeights: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mTransactions, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mTransactions: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mTransactionIndices, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mTransactionIndices: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mTransactionOutputs, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mTransactionOutputs: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mOutputAmounts, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mOutputAmounts: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mOutputTransactions, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mOutputTransactions: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mSpentKeys, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mSpentKeys: ", sResult).c_str()));
        }

        if (auto sResult = mdb_drop(sTxn, mProperties, 0)) {
            throw (DB_ERROR(lmdbError("Failed to drop mProperties: ", sResult).c_str()));
        }

        FMdbValCopy<const char *> k("version");
        FMdbValCopy<uint32_t> v(VERSION);

        if (auto sResult = mdb_put(sTxn, mProperties, &k, &v, 0)) {
            throw (DB_ERROR(lmdbError("Failed to write version to database: ", sResult).c_str()));
        }

        sTxn.commit();
        mCumSize = 0;
        mCumCount = 0;
    }

    bool BlockchainLMDB::lock()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        checkOpen();

        return false;
    }

    void BlockchainLMDB::unlock()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        checkOpen();
    }

    void BlockchainLMDB::blockTxnStart(bool bReadOnly)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        if (bReadOnly) {
            MDB_txn *sTxn;
            FMdbTxnCursors *sCurs;
            blockReadTxnStart(&sTxn, &sCurs);

            return;
        }

        /**
         * Distinguish the exceptions here from exceptions that would be thrown while
         * using the txn and committing it.
         *
         * If an exception is thrown in this setup, we don't want the caller to catch
         * it and proceed as if there were an existing write txn, such as trying to
         * call blockTxnAbort(). It also indicates a serious issue which will
         * probably be thrown up another layer.
         */
        if (!mBatchActive && mWriteDBTxnSafe) {
            throw (DB_ERROR_TXN_START(
                    (std::string("Attempted to start new write txn when write txn already exists in ") +
                     __func__).c_str()));
        }

        if (!mBatchActive) {
            mWriter = boost::this_thread::get_id();
            mWriteDBTxnSafe = new FMdbTxnSafe();

            if (auto sDBRes = lmdbTxnBegin(mDbEnv, NULL, 0, *mWriteDBTxnSafe)) {
                delete mWriteDBTxnSafe;
                mWriteDBTxnSafe = nullptr;
                throw (DB_ERROR_TXN_START(lmdbError("Failed to create a transaction for the db: ", sDBRes).c_str()));
            }
            memset(&mWriteCursors, 0, sizeof(mWriteCursors));
            if (mTInfo.get()) {
                if (mTInfo->sTReadFlags.bRfTxn) {
                    mdb_txn_reset(mTInfo->sTReadTxn);
                }
                memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
            }
        } else if (mWriter != boost::this_thread::get_id()) {
            throw (DB_ERROR_TXN_START(
                    (std::string("Attempted to start new write txn when batch txn already exists in ") +
                     __func__).c_str()));
        }
    }

    void BlockchainLMDB::blockTxnAbort()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        if (mWriteDBTxnSafe && mWriter == boost::this_thread::get_id()) {
            if (!mBatchActive) {
                delete mWriteDBTxnSafe;
                mWriteDBTxnSafe = nullptr;
                memset(&mWriteCursors, 0, sizeof(mWriteCursors));
            }
        } else if (mTInfo->sTReadTxn) {
            mdb_txn_reset(mTInfo->sTReadTxn);
            memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
        } else {
            // This would probably mean an earlier exception was caught, but then we
            // proceeded further than we should have.
            throw (DB_ERROR((std::string("BlockchainLMDB::") + __func__ +
                             std::string(": block-level DB transaction abort called when write txn doesn't exist")
                            ).c_str()));
        }
    }

    void BlockchainLMDB::blockTxnStop()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        if (mWriteDBTxnSafe && mWriter == boost::this_thread::get_id()) {
            if (!mBatchActive) {
                mWriteDBTxnSafe->commit();

                delete mWriteDBTxnSafe;
                mWriteDBTxnSafe = nullptr;
                memset(&mWriteCursors, 0, sizeof(mWriteCursors));
            }
        } else if (mTInfo->sTReadTxn) {
            mdb_txn_reset(mTInfo->sTReadTxn);
            memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
        }
    }

    bool BlockchainLMDB::blockReadTxnStart(MDB_txn **sMdbTxn, FMdbTxnCursors **sCurs) const
    {
        // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        bool bRet = false;
        FMdbThreadInfo *sTInfo;

        if (mWriteDBTxnSafe && mWriter == boost::this_thread::get_id()) {
            *sMdbTxn = mWriteDBTxnSafe->pTxn;
            *sCurs = (FMdbTxnCursors *) &mWriteCursors;

            return bRet;
        }

        if (!(sTInfo = mTInfo.get()) || mdb_txn_env(sTInfo->sTReadTxn) != mDbEnv) {
            sTInfo = new FMdbThreadInfo;
            mTInfo.reset(sTInfo);

            memset(&sTInfo->sTReadCursors, 0, sizeof(sTInfo->sTReadCursors));
            memset(&sTInfo->sTReadFlags, 0, sizeof(sTInfo->sTReadFlags));

            if (auto iMdbRes = lmdbTxnBegin(mDbEnv, nullptr, MDB_RDONLY, &sTInfo->sTReadTxn)) {
                throw (DB_ERROR_TXN_START(
                        lmdbError("Failed to create a read transaction for the db: ", iMdbRes).c_str()));
            }

            bRet = true;
        } else if (!sTInfo->sTReadFlags.bRfTxn) {
            if (auto iMdbRes = lmdbTxnRenew(sTInfo->sTReadTxn)) {
                throw (DB_ERROR_TXN_START(
                        lmdbError("Failed to renew a read transaction for the db: ", iMdbRes).c_str()));
            }

            bRet = true;
        }

        if (bRet) {
            sTInfo->sTReadFlags.bRfTxn = true;
        }

        *sMdbTxn = sTInfo->sTReadTxn;
        *sCurs = &sTInfo->sTReadCursors;

        return bRet;
    }

    void BlockchainLMDB::blockReadTxnStop() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        mdb_txn_reset(mTInfo->sTReadTxn);
        memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
    }

    void BlockchainLMDB::setBatchTransactions(bool bBatchTransactions)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        if ((bBatchTransactions) && mBatchTransactions) {

        }

        mBatchTransactions = bBatchTransactions;
    }

    bool BlockchainLMDB::batchStart(uint64_t uBatchNumBlocks, uint64_t uBatchBytes)
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        return true;
    }

    void BlockchainLMDB::batchCommit()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    }

    void BlockchainLMDB::batchAbort()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

        if (!mBatchTransactions) {
            throw (DB_ERROR("Batch transactions not enabled."));
        }

        if (!mBatchActive) {
            throw (DB_ERROR("Batch transaction not in progress."));
        }

        if (mWriteBatchDBTxnSafe == nullptr) {
            throw (DB_ERROR("Batch transaction not in progress."));
        }

        if (mWriter != boost::this_thread::get_id()) {
            throw (DB_ERROR("Batch transactions owned by another thread."));
        }

        checkOpen();
        mWriteDBTxnSafe = nullptr;
        // Explicitly call in case mdb_env_close() (BlockchainLMDB::close())
        // called before BlockchainLMDB destructor called.
        mWriteBatchDBTxnSafe->abort();
        delete mWriteBatchDBTxnSafe;
        mWriteBatchDBTxnSafe = nullptr;
        mBatchActive = false;
        memset(&mWriteCursors, 0, sizeof(mWriteCursors));
    }

    void BlockchainLMDB::batchStop()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    }

    bool BlockchainLMDB::isReadOnly() const
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        return true;
    }

    void BlockchainLMDB::fixUp()
    {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
        BlockchainDB::fixUp();
    }
}