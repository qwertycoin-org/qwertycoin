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
#include <boost/format.hpp>

#include <Common/Util.h>
#include <Common/TimeUtils.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/TransactionExtra.h>

#include <Lmdb/DBLMDB.h>
#include <Lmdb/LMDBBlob.h>

#if defined(__i386) || defined(__x86_64)
#define MISALIGNED_OK 1
#endif

using namespace Common;
using namespace Crypto;
using namespace CryptoNote;
using namespace Logging;

#define VERSION 1

namespace {
#define MDBValSet(var, val) MDB_val var = { sizeof(val), (void *)&(val) }

template<typename T>
struct FMdbValCopy : MDB_val {
    FMdbValCopy(const T &t) : tCopy(t)
    {
        mv_size = sizeof(T);
        mv_data = &tCopy;
    }

private:
    T tCopy;
};

template<>
struct FMdbValCopy<CryptoNote::blobData> : public MDB_val {
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
struct FMdbValCopy<const char *> : public MDB_val {
    FMdbValCopy(const char *cS) : uSize(strlen(cS) + 1), sData(new char[uSize])
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
 * PaymentIndex          PaymentID      tx hash
 * TimestampIndex        Timestamp      tx hash/es
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

const char *const LMDB_PAYMENT_INDEX = "PaymentIndex";
const char *const LMDB_TIMESTAMP_INDEX = "TimestampIndex";
const char *const LMDB_TIME_TO_LIFE_INDEX = "TimeToLifeIndex";

const char *const LMDB_PROPERTIES = "Properties";

const char cZeroKey[8] = { 0 };
const MDB_val cZeroKVal = { sizeof(cZeroKey), (void *)cZeroKey };

std::string lmdbError(const std::string &cErrorString, int iDBRes)
{
    const std::string cFullString = cErrorString + mdb_strerror(iDBRes);

    return cFullString;
}

inline void lmdbOpen(MDB_txn *sTxn, const char *cName, int iFlags, MDB_dbi &sDBi,
                     const std::string &cErrorString)
{
    auto sDBRes = mdb_dbi_open(sTxn, cName, iFlags, &sDBi);
    if (sDBRes) {
        throw(DB_OPEN_FAILURE((lmdbError(cErrorString + " : ", sDBRes)
                               + std::string(" - you may want to start with --db-salvage"))
                                      .c_str()));
    } else {
        // std::cout << "lmdbOpen: " << cName << " opened. sDBRes: " << sDBRes << std::endl;
    }
}
} // namespace

#define CURSOR(name)                                                                               \
    if (sCur##name) {                                                                              \
    } else {                                                                                       \
        int iRes = mdb_cursor_open(*mWriteDBTxnSafe, m##name, &sCur##name);                        \
        if (iRes) {                                                                                \
            throw(DB_ERROR(lmdbError("Failed to open cursor: ", iRes).c_str()));                   \
        }                                                                                          \
    }

#define READ_CURSOR(cName)                                                                         \
    if (!sCur##cName) {                                                                            \
        int iResult = mdb_cursor_open(pTxn, m##cName, (MDB_cursor **)&sCur##cName);                \
        if (iResult) {                                                                             \
            throw(DB_ERROR(lmdbError("Failed to open cursor: ", iResult).c_str()));                \
        }                                                                                          \
        if (sCursor != &mWriteCursors) {                                                           \
            mTInfo->sTReadFlags.bRf##cName = true;                                                 \
        }                                                                                          \
    } else if (sCursor != &mWriteCursors && !mTInfo->sTReadFlags.bRf##cName) {                     \
        int iResult = mdb_cursor_renew(pTxn, sCur##cName);                                         \
        if (iResult) {                                                                             \
            throw(DB_ERROR(lmdbError("Failed to renew cursor: ", iResult).c_str()));               \
        }                                                                                          \
                                                                                                   \
        mTInfo->sTReadFlags.bRf##cName = true;                                                     \
    }

namespace CryptoNote {
#define TXN_PREFIX(iFlags)                                                                         \
    ;                                                                                              \
    FMdbTxnSafe sAutoTxn;                                                                          \
    FMdbTxnSafe *sTxnPtr = &sAutoTxn;                                                              \
    if (mBatchActive) {                                                                            \
        sTxnPtr = mWriteDBTxnSafe;                                                                 \
    } else {                                                                                       \
        if (auto sMdbResult = lmdbTxnBegin(mDbEnv, NULL, iFlags, sAutoTxn)) {                      \
            throw(DB_ERROR(lmdbError(std::string("Failed to create a transaction for the db in ")  \
                                             + __FUNCTION__ + ": ",                                \
                                     sMdbResult)                                                   \
                                   .c_str()));                                                     \
        }                                                                                          \
    }

#define TXN_PREFIX_READONLY()                                                                      \
    MDB_txn *pTxn;                                                                                 \
    FMdbTxnCursors *sCursor;                                                                       \
    FMdbTxnSafe sAutoTxn;                                                                          \
    bool bMyReadTxn = blockReadTxnStart(&pTxn, &sCursor);                                          \
    if (bMyReadTxn) {                                                                              \
        sAutoTxn.pThreadInfo = mTInfo.get();                                                       \
    } else {                                                                                       \
        sAutoTxn.uncheck();                                                                        \
    }
#define TXN_POSTFIX_READONLY()

#define TXN_POSTFIX_SUCCESS()                                                                      \
    do {                                                                                           \
        if (!mBatchActive) {                                                                       \
            sAutoTxn.commit();                                                                     \
        }                                                                                          \
    } while (0);

int BlockchainLMDB::compareUInt64(const MDB_val *a, const MDB_val *b)
{
    uint64_t va, vb;
    memcpy(&va, a->mv_data, sizeof(va));
    memcpy(&vb, b->mv_data, sizeof(vb));
    return (va < vb) ? -1 : va > vb;
}

int BlockchainLMDB::compareHash32(const MDB_val *a, const MDB_val *b)
{
    uint32_t *va = (uint32_t *)a->mv_data;
    uint32_t *vb = (uint32_t *)b->mv_data;
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
    const char *va = (const char *)a->mv_data;
    const char *vb = (const char *)b->mv_data;
    return strcmp(va, vb);
}

typedef struct FBlockInfo {
    uint64_t uBIHeight;
    uint64_t uBITimestamp;
    uint64_t uBIGeneratedCoins;
    uint64_t uBISize;
    CryptoNote::difficulty_type uBIDifficulty;
    Crypto::Hash sBIHash;
} FBlockInfo;

typedef struct FBlockHeight {
    Crypto::Hash sHash;
    uint64_t uHeight;
} FBlockHeight;

typedef struct FTransactionIndex {
    Crypto::Hash key;
    FTxData data;
} FTransactionIndex;

typedef struct FOutputKey {
    uint64_t uAmountIndex;
    uint64_t uOutputId;
    FOutputData sData;
} FOutputKey;

typedef struct FOutTx {
    uint64_t output_id;
    Crypto::Hash tx_hash;
    uint64_t local_index;
} FOutTx;

std::atomic<uint64_t> FMdbTxnSafe::pNumActiveTxns { 0 };
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
        while (pCreationGate.test_and_set())
            ;
        pNumActiveTxns++;
        pCreationGate.clear();
    }
}

FMdbTxnSafe::~FMdbTxnSafe()
{

    if (!pCheck) {
        // std::cout << "~FMdbTxnSafe::!pCheck" << std::endl;
        return;
    }

    if (pThreadInfo != nullptr) {
        mdb_txn_reset(pThreadInfo->sTReadTxn);
        // std::cout << "~FMdbTxnSafe::pThreadInfo != nullptr {mdb_txn_reset}" << std::endl;
        memset(&pThreadInfo->sTReadFlags, 0, sizeof(pThreadInfo->sTReadFlags));
        // std::cout << "~FMdbTxnSafe::pThreadInfo != nullptr {memset}" << std::endl;
    } else if (pTxn != nullptr) {
        std::cout << "~FMdbTxnSafe::pTxn != nullptr" << std::endl;
        if (pBatchTxn) {
            // this is a batch txn and should have been handled before this point for safety
            std::cout << "WARNING: mdb_txn_safe: m_txn is a batch txn and it's not NULL in "
                         "destructor - calling mdb_txn_abort()"
                      << std::endl;
        } else {
            // Example of when this occurs: a lookup fails, so a read-only txn is
            // aborted through this destructor. However, successful read-only txns
            // ideally should have been committed when done and not end up here.
            //
            // NOTE: not sure if this is ever reached for a non-batch write
            // transaction, but it's probably not ideal if it did.
            std::cout << "mdb_txn_safe: m_txn not NULL in destructor - calling mdb_txn_abort()"
                      << std::endl;
        }
        mdb_txn_abort(pTxn);
    }
    pNumActiveTxns--;
    // std::cout << "~FMdbTxnSafe::pNumActiveTxns0 = " << pNumActiveTxns << std::endl;
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
        throw(DB_ERROR(lmdbError(cMessage + ": ", sResult).c_str()));
    }

    pTxn = nullptr;
}

void FMdbTxnSafe::abort()
{
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
    while (pCreationGate.test_and_set())
        ;
}

void FMdbTxnSafe::waitNoActiveTxns()
{
    std::cout << "pNumActiveTxns: " << pNumActiveTxns;
    while (pNumActiveTxns > 0)
        ;
}

void FMdbTxnSafe::allowNewTxns()
{
    pCreationGate.clear();
}

void lmdbResized(MDB_env *sEnv)
{
    std::cout << "BlockchainLMDB::" << __func__;
    FMdbTxnSafe::preventNewTxns();

    std::cout << "LMDB map resize detected.";

    MDB_envinfo sLMDBEnvInfo;
    mdb_env_info(sEnv, &sLMDBEnvInfo);
    uint64_t uOldSize = sLMDBEnvInfo.me_mapsize;

    FMdbTxnSafe::waitNoActiveTxns();
    std::cout << "No active DB Txs active.";

    int iResult = mdb_env_set_mapsize(sEnv, 0);
    if (iResult) {
        throw(DB_ERROR(lmdbError("Failed to set new mapsize: ", iResult).c_str()));
    }

    mdb_env_info(sEnv, &sLMDBEnvInfo);
    uint64_t uNewSize = sLMDBEnvInfo.me_mapsize;
    std::cout << "LMDB Mapsize increased."
              << "  Old: " << uOldSize / (1024 * 1024) << "MiB"
              << ", New: " << uNewSize / (1024 * 1024) << "MiB";

    FMdbTxnSafe::allowNewTxns();
}

inline int lmdbTxnBegin(MDB_env *sEnv, MDB_txn *sParent, unsigned int iFlags, MDB_txn **sTxn)
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

void BlockchainLMDB::checkOpen() const
{
    // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    if (!pOpen) {
        throw DB_ERROR("DB operation attempted on a not-open DB instance");
    }
}

void BlockchainLMDB::doResize(uint64_t uIncreaseSize)
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    std::lock_guard<std::recursive_mutex> lockGuard(pSyncronizationLock);

    // const uint64_t uAddSize = (size_t) 1 << 30;
    const uint64_t uAddSize = 64 * 1024 * 1024;

    try {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". Try begin";
        boost::filesystem::path sPath(mFolder);
        boost::filesystem::space_info sSpaceInfo = boost::filesystem::space(sPath);

        if (sSpaceInfo.available < uAddSize) {
            mLogger(DEBUGGING, BRIGHT_CYAN)
                    << "BlockchainLMDB::" << __func__ << ". sSpaceInfo.available < uAddSize";
            return;
        } else {
            pIsResizing = true;
            mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". else";
            MDB_envinfo sLMDBEnvInfo;
            mdb_env_info(mDbEnv, &sLMDBEnvInfo);
            MDB_stat sLMDBStat;
            mdb_env_stat(mDbEnv, &sLMDBStat);
            uint64_t uOldMapSize = sLMDBEnvInfo.me_mapsize;
            uint64_t uNewMapSize = uOldMapSize + uAddSize;

            if (uIncreaseSize > 0) {
                mLogger(DEBUGGING, BRIGHT_CYAN)
                        << "BlockchainLMDB::" << __func__ << ". uIncreaseSize > 0";
                uNewMapSize = sLMDBEnvInfo.me_mapsize + uIncreaseSize;
            } else {
                uNewMapSize = 64 * 1024 * 1024;
            }

            uint32_t uMB = 1024 * 1024;

            mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                       << ". uIncreaseSize: " << (uIncreaseSize / uMB) << "MiB";
            mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                       << ". uAddSize: " << (uAddSize / uMB) << "MiB";
            mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                       << ". uNewMapSize: " << (uNewMapSize / uMB) << "MiB";
            mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                       << ". uOldMapSize: " << (uOldMapSize / uMB) << "MiB";
            uNewMapSize += (uNewMapSize % sLMDBStat.ms_psize);

            if (mWriteDBTxnSafe != nullptr) {
                mLogger(DEBUGGING, BRIGHT_CYAN)
                        << "BlockchainLMDB::" << __func__ << ". mWriteDBTxnSafe != nullptr";
                if (mBatchActive) {
                    throw(DB_ERROR(
                            "LMDB resizing not yet supported when batch transactions enabled!"));

                } else {
                    throw(DB_ERROR("attempting resize with write transaction in progress, this "
                                   "should not happen!"));
                }
            }

            int iResult = mdb_env_set_mapsize(mDbEnv, uNewMapSize);
            if (iResult) {
                throw(DB_ERROR(lmdbError("Failed to set new mapsize: ", iResult).c_str()));
            }

            mLogger(INFO, BRIGHT_CYAN) << "LMDB Mapsize increased."
                                       << "  Old: " << uOldMapSize / (1024 * 1024) << "MiB"
                                       << ", New: " << uNewMapSize / (1024 * 1024) << "MiB";

            FMdbTxnSafe::allowNewTxns();
            pIsResizing = false;
        }
    } catch (std::exception &e) {
        mLogger(ERROR, BRIGHT_RED) << "Error during resizing: " << e.what();
    }
}

bool BlockchainLMDB::needResize(uint64_t uIncreaseSize) const
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
#if defined(ENABLE_AUTO_RESIZE)
    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__ << ". ENABLE_AUTO_RESIZE: " << ENABLE_AUTO_RESIZE;
    MDB_envinfo sMEnvIn;
    mdb_env_info(mDbEnv, &sMEnvIn);
    MDB_stat sMStat;
    mdb_env_stat(mDbEnv, &sMStat);

    // uSizeUsed doesn't include data yet to be committed, which can be
    // significant size during batch transactions. For that, we estimate the size
    // needed at the beginning of the batch transaction and pass in the
    // additional size needed.
    mdb_size_t uSizeUsed = sMStat.ms_psize * sMEnvIn.me_last_pgno;
    uint64_t uMapSize = sMEnvIn.me_mapsize;
    float fResizePercent = RESIZE_PERCENT;
    double uActualPercent = (100. * uSizeUsed / uMapSize);
    uint32_t uMB = 1024 * 1024;

    if (uIncreaseSize == 0) {
        uIncreaseSize = 8 * uMB;
    }

    if ((uActualPercent == 5.0 || uActualPercent == 10.0 || uActualPercent == 25.0
         || uActualPercent == 50.0 || uActualPercent == 75.0
         || uActualPercent == (fResizePercent * 100.0))
        && uActualPercent != 0) {
        mLogger(INFO, BRIGHT_CYAN) << "uActualPercent: " << fmod(uActualPercent, 5.0f);
        mLogger(INFO, BRIGHT_CYAN) << "DB map size: " << (uMapSize / uMB) << "MiB";
        mLogger(INFO, BRIGHT_CYAN) << "Space used: " << (uSizeUsed / uMB) << "MiB";
        mLogger(INFO, BRIGHT_CYAN)
                << "Space remaining: " << ((uMapSize - uSizeUsed) / uMB) << "MiB";
        mLogger(INFO, BRIGHT_CYAN) << "Size threshold: " << (uIncreaseSize / uMB) << "MiB";
        if (uSizeUsed > 0) {
            mLogger(INFO, BRIGHT_CYAN)
                    << boost::format("Percent used: %.04f  Percent threshold: %.04f")
                            % (100. * uSizeUsed / uMapSize) % (100. * fResizePercent);
        }
    }

    if (uIncreaseSize > 0) {
        uint64_t uSize = (uMapSize - uSizeUsed);

        mLogger(TRACE, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". sMStat.ms_psize: " << sMStat.ms_psize;
        mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                    << ". sMEnvIn.me_last_pgno: " << sMEnvIn.me_last_pgno;
        mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uSize: " << uSize;
        mLogger(TRACE, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". uSizeUsed: " << uSizeUsed;
        mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uMapSize: " << uMapSize;
        mLogger(TRACE, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". uIncreaseSize: " << uIncreaseSize;
        if (uSize < uIncreaseSize) {
            mLogger(DEBUGGING, BRIGHT_CYAN) << "Threshold met (size-based)";
            return true;
        } else {
            return false;
        }
    }

    if ((double)uSizeUsed / uMapSize > fResizePercent) {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "Threshold met (percent-based)";
        return true;
    }

    return false;
#else
    return false;
#endif
}

void BlockchainLMDB::checkAndResizeForBatch(uint64_t uBatchNumBlocks, uint64_t uBatchBytes)
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    const uint64_t uMinIncSize = 32 * (1 << 20);
    uint64_t uThreshold = 0;
    uint64_t uIncrease;
    if (uBatchNumBlocks) {
        uThreshold = getEstimatedBatchSize(uBatchNumBlocks, uBatchBytes);
        mLogger(DEBUGGING, BRIGHT_CYAN) << "Calculated batch size: " << uThreshold;
        // The increased DB size could be a multiple of threshold_size, a fixed
        // size increase (> uThreshold), or other variations.
        //
        // Currently we use the greater of threshold size and a minimum size. The
        // minimum size increase is used to avoid frequent resizes when the batch
        // size is set to a very small numbers of blocks.
        uIncrease = (uThreshold > uMinIncSize) ? uThreshold : uMinIncSize;
        mLogger(DEBUGGING, BRIGHT_CYAN) << "Increase size: " << uIncrease;
    }

    // if threshold_size is 0 (i.e. number of blocks for batch not passed in), it
    // will fall back to the percent-based threshold check instead of the
    // size-based check
    FMdbTxnSafe::preventNewTxns();
    if (needResize(uThreshold)) {
        mLogger(INFO, BRIGHT_CYAN) << "DB Resize needed.";
        doResize(uIncrease);
    } else {
        FMdbTxnSafe::allowNewTxns();
    }
}

uint64_t BlockchainLMDB::getEstimatedBatchSize(uint64_t uBatchNumBlocks, uint64_t uBatchBytes) const
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    uint64_t uThreshold = 0;

    // Batch size estimate * batch safety factor = final size estimate
    // Takes into account "reasonable" block size increases in batch.
    float fBatchSafetyFactor = 1.8f;
    float fBatchFudgeFactor = fBatchSafetyFactor * uBatchNumBlocks;
    // estimate of stored block expanded from raw block, including denormalization and db overhead.
    // Note that this probably doesn't grow linearly with block size.
    float fDBExpandFactor = 4.5f;
    uint64_t uNumPrevBlocks = 500;
    // For resizing purposes, allow for at least 4k average block size.
    uint64_t uMinBlockSize = 4 * 1024;
    uint64_t uBlockStop = 0;
    uint64_t uHeight = height();
    if (uHeight > 1) {
        uBlockStop = uHeight - 1;
    }
    uint64_t uBlockStart = 0;
    if (uBlockStop >= uNumPrevBlocks) {
        uBlockStart = uBlockStop - uNumPrevBlocks + 1;
    }
    uint32_t uNumBlocksUsed = 0;
    uint64_t uTotalBlockSize = 0;
    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__ << ". Height: " << uHeight
            << " uBlockStart: " << uBlockStart << " uBlockStop: " << uBlockStop << ENDL;
    uint64_t uAvgBlockSize = 0;
    if (uBatchBytes) {
        uAvgBlockSize = uBatchBytes / uBatchNumBlocks;
        goto estimated;
    }

    if (uHeight == 0) {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "No existing blocks to check for average block size";
    } else if (mCumCount >= uNumPrevBlocks) {
        uAvgBlockSize = mCumSize / mCumCount;
        mLogger(DEBUGGING, BRIGHT_CYAN)
                << "Average block size across recent " << mCumCount << " blocks: " << uAvgBlockSize;
        mCumSize = 0;
        mCumCount = 0;
    } else {

        MDB_txn *sTxn;
        FMdbTxnCursors *sRCursors;
        mLogger(DEBUGGING, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". Before blockReadTxnStart";
        bool bReadTxn = blockReadTxnStart(&sTxn, &sRCursors);
        mLogger(DEBUGGING, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". bReadTxn: " << bReadTxn;
        for (uint64_t uBlockNum = uBlockStart; uBlockNum <= uBlockStop; ++uBlockNum) {
            // we have access to block weight, which will be greater or equal to block size,
            // so use this as a proxy. If it's too much off, we might have to check actual size,
            // which involves reading more data, so is not really wanted
            uint64_t uBlockSize = getBlockSize(uBlockNum);
            uTotalBlockSize += uBlockSize;
            // Track number of blocks being totalled here instead of assuming, in case
            // some blocks were to be skipped for being outliers.
            ++uNumBlocksUsed;
        }

        if (bReadTxn) {
            blockReadTxnStop();
            uAvgBlockSize = uTotalBlockSize / (uNumBlocksUsed ? uNumBlocksUsed : 1);
            mLogger(DEBUGGING, BRIGHT_CYAN) << "Average block size across recent " << uNumBlocksUsed
                                            << " blocks: " << uAvgBlockSize;
        }
    }

estimated:
    if (uAvgBlockSize < uMinBlockSize) {
        uAvgBlockSize = uMinBlockSize;
    }
    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__
            << ". Estimated average block size for batch: " << uAvgBlockSize;

    // bigger safety margin on smaller block sizes
    if (fBatchFudgeFactor < 5000.0) {
        fBatchFudgeFactor = 5000.0;
    }

    uThreshold = uAvgBlockSize * fDBExpandFactor * fBatchFudgeFactor;

    return uThreshold;
}

uint64_t BlockchainLMDB::getDBMapSize()
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    MDB_envinfo sMEnvIn;
    mdb_env_info(mDbEnv, &sMEnvIn);

    return sMEnvIn.me_mapsize;
}

uint64_t BlockchainLMDB::getDBUsedSize()
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    MDB_envinfo sMEnvIn;
    mdb_env_info(mDbEnv, &sMEnvIn);
    MDB_stat sMStat;
    mdb_env_stat(mDbEnv, &sMStat);

    return sMStat.ms_psize * sMEnvIn.me_last_pgno;
}

void BlockchainLMDB::addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                              const CryptoNote::difficulty_type &uCumulativeDifficulty,
                              const uint64_t &uCoinsGenerated, const Crypto::Hash &sBlockHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "1";

    checkOpen();
    FMdbTxnSafe::preventNewTxns();
    const uint64_t uMinIncSize = 32 * (1 << 20);
    if (needResize(uMinIncSize)) {
        mLogger(INFO, BRIGHT_CYAN) << "LMDB memory map needs to be resized, doing that now.";
        doResize(uMinIncSize);
    } else {
        FMdbTxnSafe::allowNewTxns();
    }

    {
        FMdbTxnCursors *sCursor = &mWriteCursors;
        uint64_t uHeight = height();

        CURSOR(BlockHeights)

        FBlockHeight sBlockHeight = { sBlockHash, uHeight };
        MDBValSet(sValHeight, sBlockHeight);
        if (mdb_cursor_get(sCurBlockHeights, (MDB_val *)&cZeroKVal, &sValHeight, MDB_GET_BOTH)
            == 0) {
            throw(BLOCK_EXISTS("Attempting to add block that's already in the db"));
        }

        if (uHeight > 0) {
            MDBValSet(sParentKey, block.previousBlockHash);
            int iResult = mdb_cursor_get(sCurBlockHeights, (MDB_val *)&cZeroKVal, &sParentKey,
                                         MDB_GET_BOTH);
            if (iResult) {
                throw(DB_ERROR(
                        lmdbError("Failed to get top block hash to check for new block's parent: ",
                                  iResult)
                                .c_str()));
            }

            FBlockHeight *sPrevHeight = (FBlockHeight *)sParentKey.mv_data;
            if (sPrevHeight->uHeight != uHeight - 1) {
                throw(BLOCK_PARENT_DNE("Top block is not new block's parent"));
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

        // This call to mdb_cursor_put will change height()
        FMdbValCopy<blobData> cBlob(sBlockBlob);
        iResult = mdb_cursor_put(sCurBlocks, &sValKey, &cBlob, MDB_APPEND);
        if (iResult) {
            throw(DB_ERROR(
                    lmdbError("Failed to add block blob to db transaction: ", iResult).c_str()));
        }

        FBlockInfo sBlockInfo;
        sBlockInfo.uBIHeight = uHeight;
        sBlockInfo.uBITimestamp = block.timestamp;
        sBlockInfo.uBIGeneratedCoins = uCoinsGenerated;
        sBlockInfo.uBISize = uBlockSize;
        sBlockInfo.uBIDifficulty = uCumulativeDifficulty;
        sBlockInfo.sBIHash = sBlockHash;

        MDBValSet(sValBlockInfo, sBlockInfo);
        iResult =
                mdb_cursor_put(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sValBlockInfo, MDB_APPENDDUP);
        if (iResult) {
            throw(DB_ERROR(
                    lmdbError("Failed to add block info to db transaction: ", iResult).c_str()));
        }

        iResult = mdb_cursor_put(sCurBlockHeights, (MDB_val *)&cZeroKVal, &sValHeight, 0);
        if (iResult) {
            throw(DB_ERROR(
                    lmdbError("Failed to add block height by hash to db transaction: ", iResult)
                            .c_str()));
        }

        mCumSize += uBlockSize;
        mCumCount++;
    }
}

Crypto::Hash BlockchainLMDB::getTopBlockHash() const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    uint64_t uHeight = height();

    if (uHeight != 0) {
        return getBlockHashFromHeight(uHeight - 1);
    }

    return NULL_HASH;
}

CryptoNote::Block BlockchainLMDB::getTopBlock() const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    uint64_t uHeight = height();

    CryptoNote::Block sBlock;
    if (uHeight > 0) {
        sBlock = getBlockFromHeight(uHeight - 1);
    }

    return sBlock;
}

uint64_t BlockchainLMDB::height() const
{
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    checkOpen();
    TXN_PREFIX_READONLY();
    int iResult;

    // Get current height
    MDB_stat sDBStats;
    iResult = mdb_stat(pTxn, mBlocks, &sDBStats);
    if (iResult) {
        throw(DB_ERROR(lmdbError("Failed to query m_blocks: ", iResult).c_str()));
    }

    uint64_t uRet = sDBStats.ms_entries;

    return uRet;
}

uint64_t BlockchainLMDB::numOutputs() const
{
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    int iResult;

    MDB_stat sDBStats;
    if ((iResult = mdb_stat(pTxn, mOutputTransactions, &sDBStats))) {
        // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". bad iResult: " <<
        // iResult;
        throw(DB_ERROR(lmdbError("Failed to query mOutputTransactions: ", iResult).c_str()));
    }

    uint64_t uRetVal = sDBStats.ms_entries;

    return uRetVal;
}

bool BlockchainLMDB::transactionExists(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionIndices);

    MDBValSet(sValHash, sHash);
    bool bTransactionFound = false;

    TIME_MEASURE_START(txTime1);
    auto getResult =
            mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult == 0) {
        bTransactionFound = true;
    } else if (getResult != MDB_NOTFOUND) {
        throw(DB_ERROR(
                lmdbError(std::string("DB error attempting to fetch transaction index from hash ")
                                  + Common::podToHex(sHash) + ": ",
                          getResult)
                        .c_str()));
    }

    TIME_MEASURE_FINISH(txTime1);
    gTimeTxExists += txTime1;

    TXN_POSTFIX_READONLY();

    if (!bTransactionFound) {
        mLogger(TRACE, BRIGHT_CYAN)
                << "Transaction with hash " << sHash << " not found in database";
        return false;
    }

    return true;
}

bool BlockchainLMDB::transactionExists(const Crypto::Hash &sHash, uint64_t &uTransactionId) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionIndices);

    MDBValSet(sValHash, sHash);

    TIME_MEASURE_START(txTime1);
    auto getResult =
            mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    TIME_MEASURE_FINISH(txTime1);
    gTimeTxExists += txTime1;

    if (!getResult) {
        FTransactionIndex *sTIx = (FTransactionIndex *)sValHash.mv_data;
        uTransactionId = sTIx->data.uTxID;
    }

    TXN_POSTFIX_READONLY();

    bool bRet = false;
    if (getResult == MDB_NOTFOUND) {
        mLogger(TRACE, BRIGHT_CYAN) << "Transaction with hash " << sHash << "not found in database";
    } else if (getResult) {
        throw(DB_ERROR(lmdbError("DB error attempting to fetch transaction from hash", getResult)
                               .c_str()));
    } else {
        bRet = true;
    }

    return bRet;
}

uint64_t BlockchainLMDB::getTransactionUnlockTime(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionIndices);

    MDBValSet(sValHash, sHash);
    auto getResult =
            mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(TX_DNE(lmdbError(std::string("tx data with hash ") + Common::podToHex(sHash)
                                       + " not found in db: ",
                               getResult)
                             .c_str()));
    } else if (getResult) {
        throw(DB_ERROR(
                lmdbError("DB error attempting to fetch tx data from hash: ", getResult).c_str()));
    }

    FTransactionIndex *sTIn = (FTransactionIndex *)sValHash.mv_data;
    uint64_t uRet = sTIn->data.uUnlockTime;

    return uRet;
}

bool BlockchainLMDB::getTransactionBlob(const Crypto::Hash &sHash,
                                        CryptoNote::blobData &sTransactionBlob) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionIndices);
    READ_CURSOR(Transactions);

    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__ << ". sHash: " << Common::podToHex(sHash);
    MDBValSet(sValHash, sHash);
    MDB_val sResult;

    auto getResult =
            mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult == 0) {
        FTransactionIndex *sTIx = (FTransactionIndex *)sValHash.mv_data;
        MDBValSet(sValTxId, sTIx->data.uTxID);
        getResult = mdb_cursor_get(sCurTransactions, &sValTxId, &sResult, MDB_SET);
    }

    if (getResult == MDB_NOTFOUND) {
        mLogger(DEBUGGING, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". sHash: " << Common::podToHex(sHash)
                << " not found in the DB.";
        return false;
    } else if (getResult) {
        throw(DB_ERROR(lmdbError("DB error attempting to fetch tx from hash", getResult).c_str()));
    }

    sTransactionBlob.assign(reinterpret_cast<char *>(sResult.mv_data), sResult.mv_size);

    // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". sTransactionBlob: "
    // << sTransactionBlob;

    TXN_POSTFIX_READONLY();

    return true;
}

uint64_t BlockchainLMDB::getTransactionCount() const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    checkOpen();
    TXN_PREFIX_READONLY();
    int iResult;

    MDB_stat sDBStats;
    if ((iResult = mdb_stat(pTxn, mTransactions, &sDBStats))) {
        throw(DB_ERROR(lmdbError("Failed to query m_txs: ", iResult).c_str()));
    }

    TXN_POSTFIX_READONLY();

    return sDBStats.ms_entries;
}

std::vector<CryptoNote::Transaction>
BlockchainLMDB::getTransactionList(const std::vector<Crypto::Hash> &vHashList) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    std::vector<CryptoNote::Transaction> vTxs;

    for (auto &sHash : vHashList) {
        vTxs.push_back(getTransaction(sHash));
    }

    return vTxs;
}

uint64_t BlockchainLMDB::getTransactionBlockHeight(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionIndices);

    MDBValSet(sValHash, sHash);
    auto getResult =
            mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(TX_DNE(std::string("FTxData with hash ")
                             .append(Common::podToHex(sHash))
                             .append(" not found in db")
                             .c_str()));
    } else if (getResult) {
        throw(DB_ERROR(
                lmdbError("DB error attempting to fetch tx height from hash", getResult).c_str()));
    }

    FTransactionIndex *sTIn = (FTransactionIndex *)sValHash.mv_data;
    uint64_t uRet = sTIn->data.uBlockID;

    return uRet;
}

uint64_t BlockchainLMDB::getNumOutputs(const uint64_t &uAmount) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(OutputAmounts);

    FMdbValCopy<uint64_t> sValAmount(uAmount);
    MDB_val sV;
    mdb_size_t sNumElems = 0;
    auto getResult = mdb_cursor_get(sCurOutputAmounts, &sValAmount, &sV, MDB_SET);
    if (getResult == MDB_SUCCESS) {
        mdb_cursor_count(sCurOutputAmounts, &sNumElems);
    } else if (getResult != MDB_NOTFOUND) {
        throw(DB_ERROR("DB error attempting to get number of outputs of an amount"));
    }

    return sNumElems;
}

FOutputData BlockchainLMDB::getOutputKey(const uint64_t &uAmount, const uint32_t &uIndex)
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ENDL
                                << "  uAmount: " << uAmount << ENDL
                                << "  uIndex: " << uIndex;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(OutputAmounts);

    uint64_t uSearchForAmount = uAmount;
    uint32_t uSearchForIndex = uIndex;

    MDBValSet(sValAmount, uSearchForAmount);
    MDBValSet(sValIndex, uSearchForIndex);
    auto getResult = mdb_cursor_get(sCurOutputAmounts, &sValAmount, &sValIndex, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(OUTPUT_DNE("Attempting to get output pubkey by index, but key does not exist"));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve an output pubkey from the db"));
    }

    const FOutputKey *sOKe = (const FOutputKey *)sValIndex.mv_data;
    FOutputData sRet;
    memcpy(&sRet, &sOKe->sData, sizeof(FOutputData));

    mdb_cursor_close(sCurOutputAmounts);

    return sRet;
}

FOutputData BlockchainLMDB::getOutputKey(const uint32_t &uGlobalIndex) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(OutputTransactions);
    READ_CURSOR(TransactionIndices);
    READ_CURSOR(Transactions);

    FOutputData sOd;
    MDBValSet(sValGlobIndex, uGlobalIndex);
    auto getResult = mdb_cursor_get(sCurOutputTransactions, (MDB_val *)&cZeroKVal, &sValGlobIndex,
                                    MDB_GET_BOTH);
    if (getResult) {
        throw(DB_ERROR("DB error attempting to fetch output tx hash"));
    }

    FOutTx *sOutTx = (FOutTx *)sValGlobIndex.mv_data;
    MDBValSet(sValHash, sOutTx->tx_hash);
    getResult =
            mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult) {
        throw(DB_ERROR(
                lmdbError(std::string("DB error attempting to fetch transaction index from hash ")
                                  + podToHex(sOutTx->tx_hash) + ": ",
                          getResult)
                        .c_str()));
    }

    FTransactionIndex *sTIn = (FTransactionIndex *)sValHash.mv_data;
    MDBValSet(sValTxId, sTIn->data.uTxID);
    MDB_val sRes;
    getResult = mdb_cursor_get(sCurTransactions, &sValTxId, &sRes, MDB_SET);
    if (getResult) {
        throw(DB_ERROR(lmdbError("DB error attempting to fetch tx from hash", getResult).c_str()));
    }

    CryptoNote::blobData sBlobData;
    sBlobData.assign(reinterpret_cast<char *>(sRes.mv_data), sRes.mv_size);

    CryptoNote::Transaction sTransaction;
    if (!parseAndValidateTransactionFromBlob(sBlobData, sTransaction)) {
        throw(DB_ERROR("Failed to parse tx from blob retrieved from the db"));
    }

    const CryptoNote::TransactionOutput sTxOutput = sTransaction.outputs[sOutTx->local_index];
    sOd.uUnlockTime = sTIn->data.uUnlockTime;
    sOd.uHeight = sTIn->data.uBlockID;
    sOd.sPublicKey = boost::get<CryptoNote::KeyOutput>(sTxOutput.target).key;

    return sOd;
}

void BlockchainLMDB::getOutputKeys(const uint64_t &uAmount, const std::vector<uint32_t> &vOffsets,
                                  std::vector<FOutputData> &vOutputs, bool bAllowPartial)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << " void";
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(OutputAmounts);

    uint64_t uSearchForAmount = uAmount;
    MDBValSet(sValAmount, uSearchForAmount);

    for (const auto &uIndex : vOffsets) {
        mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << " void" << ENDL
                                        << "Looking for output with amount: " << uAmount << ENDL
                                        << " and uIndex: " << uIndex;
        uint32_t uSearchForIndex = uIndex;
        MDBValSet(sValIndex, uSearchForIndex);

        auto getResult = mdb_cursor_get(sCurOutputAmounts, &sValAmount, &sValIndex, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            if (bAllowPartial) {
                mLogger(TRACE, BRIGHT_CYAN)
                        << "BlockchainLMDB::" << __func__ << "Partial result: " << vOutputs.size()
                        << "/" << vOffsets.size();
                break;
            } else {
                throw(OUTPUT_DNE(
                        (std::string("Attempting to get output pubkey by global index (amount ")
                         + boost::lexical_cast<std::string>(uAmount) + ", index "
                         + boost::lexical_cast<std::string>(uIndex)
                         + ", but key does not exist (current height "
                         + boost::lexical_cast<std::string>(height()) + ")")
                                .c_str()));
            }
        } else if (getResult) {
            throw(DB_ERROR(lmdbError("Error attempting to retrieve an output pubkey from the db",
                                     getResult)
                                   .c_str()));
        }

        FOutputData sData;
        const FOutputKey *sOKe = (const FOutputKey *)sValIndex.mv_data;
        memcpy(&sData, &sOKe->sData, sizeof(FOutputData));

        mdb_cursor_close(sCurOutputAmounts);

        vOutputs.push_back(sData);
    }
}

txOutIndex BlockchainLMDB::getOutputTransactionAndIndexFromGlobal(const uint64_t &uIndex) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(OutputTransactions);

    MDBValSet(sValOutId, uIndex);

    auto getResult =
            mdb_cursor_get(sCurOutputTransactions, (MDB_val *)&cZeroKVal, &sValOutId, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(OUTPUT_DNE("output with given index not in db"));
    } else if (getResult) {
        throw(DB_ERROR("DB error attempting to fetch output tx hash"));
    }

    FOutTx *sOutTx = (FOutTx *)sValOutId.mv_data;
    txOutIndex sRet = txOutIndex(sOutTx->tx_hash, sOutTx->local_index);

    return sRet;
}

void BlockchainLMDB::getOutputTransactionAndIndexFromGlobal(
        const std::vector<uint64_t> &vGlobalIndices, std::vector<txOutIndex> &vTxOutIndices) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    vTxOutIndices.clear();

    TXN_PREFIX_READONLY();
    READ_CURSOR(OutputTransactions);

    for (const uint64_t &uOutputId : vGlobalIndices) {
        MDBValSet(sValOutId, uOutputId);

        auto getResult = mdb_cursor_get(sCurOutputTransactions, (MDB_val *)&cZeroKVal, &sValOutId,
                                        MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw(OUTPUT_DNE("output with given index not in db"));
        } else if (getResult) {
            throw(DB_ERROR("DB error attempting to fetch output tx hash"));
        }

        FOutTx *sOuTx = (FOutTx *)sValOutId.mv_data;
        auto sRes = txOutIndex(sOuTx->tx_hash, sOuTx->local_index);
        vTxOutIndices.push_back(sRes);
    }
}

void BlockchainLMDB::getOutputTransactionAndIndex(const uint64_t &uAmount,
                                                  const std::vector<uint32_t> &vOffsets,
                                                  std::vector<txOutIndex> &vIndices) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    vIndices.clear();

    std::vector<uint64_t> vTxIndices;
    TXN_PREFIX_READONLY();

    READ_CURSOR(OutputAmounts);
    MDBValSet(sValAmount, uAmount);

    for (const auto &uIndex : vOffsets) {
        MDBValSet(sValIndex, uIndex);

        auto getResult = mdb_cursor_get(sCurOutputAmounts, &sValAmount, &sValIndex, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            throw(OUTPUT_DNE("Attempting to get output by index, but key does not exist"));
        } else if (getResult) {
            throw(DB_ERROR(
                    lmdbError("Error attempting to retrieve an output from the db", getResult)
                            .c_str()));
        }

        const FOutputKey *sOKe = (const FOutputKey *)sValIndex.mv_data;
        vTxIndices.push_back(sOKe->uOutputId);
    }

    if (vTxIndices.size() > 0) {
        getOutputTransactionAndIndexFromGlobal(vTxIndices, vIndices);
    }
}

txOutIndex BlockchainLMDB::getOutputTransactionAndIndex(const uint64_t &uAmount,
                                                        const uint32_t &uIndex) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    std::vector<uint32_t> vOffsets;
    std::vector<txOutIndex> vIndices;
    vOffsets.push_back(uIndex);
    getOutputTransactionAndIndex(uAmount, vOffsets, vIndices);
    if (!vIndices.size()) {
        throw(OUTPUT_DNE("Attempting to get an output index by amount and amount index, but amount "
                         "not found"));
    }

    return vIndices[0];
}

std::vector<uint64_t> BlockchainLMDB::getTransactionAmountOutputIndices(const uint64_t uTxId) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionOutputs);

    int iRes = 0;
    MDBValSet(sValTxId, uTxId);
    MDB_val sV;
    std::vector<uint64_t> vAmountOutputIndices;

    iRes = mdb_cursor_get(sCurTransactionOutputs, &sValTxId, &sV, MDB_SET);
    if (iRes == MDB_NOTFOUND) {

    } else if (iRes) {
        throw(DB_ERROR(
                lmdbError("DB error attempting to get data for TransactionOutputs[uTxId]", iRes)
                        .c_str()));
    }

    const uint64_t *uIndices = (const uint64_t *)sV.mv_data;
    int iNumOutputs = sV.mv_size / sizeof(uint64_t);

    vAmountOutputIndices.reserve(iNumOutputs);
    for (int i = 0; i < iNumOutputs; ++i) {
        vAmountOutputIndices.push_back(uIndices[i]);
    }

    uIndices = nullptr;

    return vAmountOutputIndices;
}

bool BlockchainLMDB::hasKeyImage(const Crypto::KeyImage &sImg) const
{
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    bool bRet;

    TXN_PREFIX_READONLY();
    READ_CURSOR(SpentKeys);

    MDB_val sValImg = { sizeof(sImg), (void *)&sImg };
    bRet = (mdb_cursor_get(sCurSpentKeys, (MDB_val *)&cZeroKVal, &sValImg, MDB_GET_BOTH) == 0);

    return bRet;
}

void BlockchainLMDB::addTxPoolTransaction(const CryptoNote::Transaction &sTransaction,
                                          const FTxPoolMeta &sDetails)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    FMdbTxnSafe::preventNewTxns();
    const uint64_t uMinIncSize = 32 * (1 << 20);
    if (needResize(uMinIncSize)) {
        mLogger(INFO, BRIGHT_CYAN) << "LMDB memory map needs to be resized, doing that now.";
        doResize(uMinIncSize);
    } else {
        FMdbTxnSafe::allowNewTxns();
    }

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TransactionPoolMeta)
    CURSOR(TransactionPoolBlob)

    const Crypto::Hash sHash = getObjectHash(sTransaction);

    MDB_val sValHash = { sizeof(sHash), (void *)&sHash };
    MDB_val sValMeta = { sizeof(sDetails), (void *)&sDetails };
    if (auto getResult =
                mdb_cursor_put(sCurTransactionPoolMeta, &sValHash, &sValMeta, MDB_NODUPDATA)) {
        if (getResult == MDB_KEYEXIST) {
            throw(DB_ERROR("Attempting to add TxPool tx meta that's already in the db"));
        } else {
            throw(DB_ERROR(lmdbError("Error adding TxPool tx meta to db transaction: ", getResult)
                                   .c_str()));
        }
    }

    FMdbValCopy<CryptoNote::blobData> sValBlob(transactionToBlob(sTransaction));
    if (auto getResult =
                mdb_cursor_put(sCurTransactionPoolBlob, &sValHash, &sValBlob, MDB_NODUPDATA)) {
        if (getResult == MDB_KEYEXIST) {
            throw(DB_ERROR("Attempting to add TxPool tx blob that's already in the db"));
        } else {
            throw(DB_ERROR(lmdbError("Error adding TxPool tx blob to db transaction: ", getResult)
                                   .c_str()));
        }
    }
}

void BlockchainLMDB::updateTxPoolTransaction(const Crypto::Hash &sHash, const FTxPoolMeta &sDetails)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TransactionPoolMeta)
    CURSOR(TransactionPoolBlob)

    MDB_val sValHash = { sizeof(sHash), (void *)&sHash };
    MDB_val sValMeta;
    auto getResult = mdb_cursor_get(sCurTransactionPoolMeta, &sValHash, &sValMeta, MDB_SET);
    getResult = mdb_cursor_del(sCurTransactionPoolMeta, 0);
    sValMeta = MDB_val({ sizeof(sDetails), (void *)&sDetails });

    if ((getResult = mdb_cursor_put(sCurTransactionPoolMeta, &sValHash, &sValMeta, MDB_NODUPDATA))
        != 0) {
        // Some log
    }
}

uint64_t BlockchainLMDB::getTxPoolTransactionCount() const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    int iRes;
    uint64_t uNumEntries = 0;

    TXN_PREFIX_READONLY();

    MDB_stat sDBStats;
    if ((iRes = mdb_stat(pTxn, mTransactionPoolMeta, &sDBStats))) {
        throw(DB_ERROR(lmdbError("Failed to query mTransactionPoolMeta: ", iRes).c_str()));
    }

    uNumEntries = sDBStats.ms_entries;

    return uNumEntries;
}

bool BlockchainLMDB::txPoolHasTransaction(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionPoolMeta)

    MDB_val sValHash = { sizeof(sHash), (void *)&sHash };
    auto getResult = mdb_cursor_get(sCurTransactionPoolMeta, &sValHash, NULL, MDB_SET);
    if (getResult != 0 && getResult != MDB_NOTFOUND) {
        throw(DB_ERROR(lmdbError("Error finding TxPool transaction meta: ", getResult).c_str()));
    }

    return getResult != MDB_NOTFOUND;
}

void BlockchainLMDB::removeTxPoolTransaction(const Crypto::Hash &sHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TransactionPoolMeta)
    CURSOR(TransactionPoolBlob)

    MDB_val sValHash = { sizeof(sHash), (void *)&sHash };
    auto getResult = mdb_cursor_get(sCurTransactionPoolMeta, &sValHash, NULL, MDB_SET);
    if (getResult != 0 && getResult != MDB_NOTFOUND) {
        // Uncomment this for debugging
        // throw(DB_ERROR(lmdbError("Error finding TxPool transaction meta to remove: ",
        // getResult).c_str()));
    }

    if (!getResult) {
        getResult = mdb_cursor_del(sCurTransactionPoolMeta, 0);
        if (getResult) {
            throw(DB_ERROR(lmdbError("Error adding removal of TxPool transaction metadata to db "
                                     "transaction: ",
                                     getResult)
                                   .c_str()));
        }
    }

    getResult = mdb_cursor_get(sCurTransactionPoolBlob, &sValHash, NULL, MDB_SET);
    if (getResult != 0 && getResult != MDB_NOTFOUND) {
        // Uncomment this for debugging
        // throw(DB_ERROR(lmdbError("Error finding TxPool transaction blob to remove: ",
        // getResult).c_str()));
    }

    if (!getResult) {
        getResult = mdb_cursor_del(sCurTransactionPoolBlob, 0);
        if (getResult) {
            throw(DB_ERROR(
                    lmdbError("Error adding removal of TxPool transaction blob to db transaction: ",
                              getResult)
                            .c_str()));
        }
    }
}

bool BlockchainLMDB::getTxPoolTransactionMeta(const Crypto::Hash &sHash,
                                              FTxPoolMeta &sDetails) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionPoolMeta)

    MDB_val sValHash = { sizeof(sHash), (void *)&sHash };
    MDB_val sVal;
    auto getResult = mdb_cursor_get(sCurTransactionPoolMeta, &sValHash, &sVal, MDB_SET);
    if (getResult == MDB_NOTFOUND) {
        // TODO: Add log
        return false;
    }

    if (getResult != 0) {
        throw(DB_ERROR(lmdbError("Error finding TxPool transaction meta: ", getResult).c_str()));
    }

    sDetails = *(const FTxPoolMeta *)sVal.mv_data;

    return true;
}

bool BlockchainLMDB::getTxPoolTransactionBlob(const Crypto::Hash &sHash,
                                              CryptoNote::blobData &sBlobData) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionPoolBlob)

    MDB_val sValHash = { sizeof(sHash), (void *)&sHash };
    MDB_val sVal;
    auto getResult = mdb_cursor_get(sCurTransactionPoolBlob, &sValHash, &sVal, MDB_SET);
    if (getResult == MDB_NOTFOUND) {
        mLogger(ERROR, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". "
                << lmdbError("Error finding TxPool transaction blob: ", getResult).c_str();
        return false;
    }

    if (getResult != 0) {
        mLogger(ERROR, BRIGHT_CYAN)
                << "BlockchainLMDB::" << __func__ << ". "
                << lmdbError("Error finding TxPool transaction blob: ", getResult).c_str();
        return false;
        throw(DB_ERROR(lmdbError("Error finding TxPool transaction blob: ", getResult).c_str()));
    }

    sBlobData.assign(reinterpret_cast<const char *>(sVal.mv_data), sVal.mv_size);

    return true;
}

CryptoNote::blobData BlockchainLMDB::getTxPoolTransactionBlob(const Crypto::Hash &sHash) const
{
    CryptoNote::blobData sBlobData;
    if (!getTxPoolTransactionBlob(sHash, sBlobData)) {
        throw(DB_ERROR("Tx not found in txpool: "));
    }

    return sBlobData;
}

bool BlockchainLMDB::forAllTxPoolTransactions(
        std::function<bool(const Crypto::Hash &, const FTxPoolMeta &, const CryptoNote::blobData *)>
                UFu,
        bool bIncludeBlob, bool bIncludeUnrelayedTransactions) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TransactionPoolMeta)
    READ_CURSOR(TransactionPoolBlob)

    MDB_val sValKey;
    MDB_val sValValue;
    bool bRet = true;

    MDB_cursor_op sOp = MDB_FIRST;

    while (1) {
        int iRes = mdb_cursor_get(sCurTransactionPoolMeta, &sValKey, &sValValue, sOp);
        sOp = MDB_NEXT;
        if (iRes == MDB_NOTFOUND) {
            break;
        }

        if (iRes) {
            throw(DB_ERROR(
                    lmdbError("Failed to enumerate TxPool transaction metadata: ", iRes).c_str()));
        }

        const Crypto::Hash &sHash = *(const Crypto::Hash *)sValKey.mv_data;
        const FTxPoolMeta &sDetail = *(const FTxPoolMeta *)sValValue.mv_data;
        const CryptoNote::blobData *sPassedBlob = NULL;
        CryptoNote::blobData sBlobData;
        if (bIncludeBlob) {
            MDB_val sValBlob;
            iRes = mdb_cursor_get(sCurTransactionPoolBlob, &sValKey, &sValBlob, MDB_SET);

            if (iRes == MDB_NOTFOUND) {
                throw(DB_ERROR("Failed to find TxPool transaction blob to match metadata"));
            }

            if (iRes) {
                throw(DB_ERROR(
                        lmdbError("Failed to enumerate TxPool transaction blob: ", iRes).c_str()));
            }

            sBlobData.assign(reinterpret_cast<const char *>(sValBlob.mv_data), sValBlob.mv_size);
            sPassedBlob = &sBlobData;
        }

        if (!UFu(sHash, sDetail, sPassedBlob)) {
            bRet = false;
            break;
        }
    }

    return bRet;
}

bool BlockchainLMDB::forAllKeyImages(std::function<bool(const Crypto::KeyImage &)> UFu) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(SpentKeys);

    MDB_val sValKey, sValValue;
    bool bRet = true;

    sValKey = cZeroKVal;
    MDB_cursor_op sOp = MDB_FIRST;

    while (1) {
        int iRet = mdb_cursor_get(sCurSpentKeys, &sValKey, &sValValue, sOp);
        sOp = MDB_NEXT;
        if (iRet == MDB_NOTFOUND) {
            break;
        }

        if (iRet < 0) {
            throw(DB_ERROR("Failed to enumerate key images"));
        }

        const Crypto::KeyImage sKeyImg = *(const Crypto::KeyImage *)sValValue.mv_data;

        if (!UFu(sKeyImg)) {
            bRet = false;
            break;
        }
    }

    return bRet;
}

bool BlockchainLMDB::addPaymentIndex(const CryptoNote::Transaction &sTransaction)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(PaymentIndex)

    int iRes;
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". After iRes";
    Crypto::Hash sPaymentID {};
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". get TxHash";
    Crypto::Hash sTxHash = getObjectHash(sTransaction);
    if (!getPaymentIdFromTxExtra(sTransaction.extra, sPaymentID)) {
        // mLogger(ERROR, BRIGHT_RED) << "Could not parse Payment ID from Tx Extra.";
        return false;
    }

    MDBValSet(sValPaymentID, sPaymentID);
    MDBValSet(sValTxHash, sTxHash);

    iRes = mdb_cursor_put(sCurPaymentIndex, &sValPaymentID, &sValTxHash, 0);
    if (iRes) {
        throw(DB_ERROR(lmdbError("Failed to add payment id to db transaction: ", iRes).c_str()));
    }

    return true;
}

bool BlockchainLMDB::getPaymentIndices(const Crypto::Hash &sPaymentID,
                                       std::vector<Crypto::Hash> &vTxHashes)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(PaymentIndex);

    bool bReturn = false;
    FMdbValCopy<Crypto::Hash> sValPaymentID(sPaymentID);
    MDB_val sResult;
    std::vector<Crypto::Hash> vRes;

    auto getResult = mdb_cursor_get(sCurPaymentIndex, &sValPaymentID, &sResult, MDB_SET);
    mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". getResult:" << getResult;
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get tx hash/es from PaymentID ")
                                .append(boost::lexical_cast<std::string>(sPaymentID))
                                .append(" failed -- PaymentID not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a tx hash/es from the db"));
    }

    Crypto::Hash &sResHash = *(Crypto::Hash *)sResult.mv_data;
    vRes.push_back(sResHash);

    while (mdb_cursor_get(sCurPaymentIndex, &sValPaymentID, &sResult, MDB_NEXT_DUP)) {
        sResHash = *(Crypto::Hash *)sResult.mv_data;
        vRes.push_back(sResHash);
    }

    vTxHashes = vRes;
    return true;
}

std::vector<Crypto::Hash> BlockchainLMDB::getPaymentIndices(const Crypto::Hash &sPaymentID)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    std::vector<Crypto::Hash> vTxHashes;
    if (!getPaymentIndices(sPaymentID, vTxHashes)) { }

    return vTxHashes;
}

bool BlockchainLMDB::removePaymentIndex(const Crypto::Hash &sPaymentID)
{
    mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(PaymentIndex)

    int iRes;
    MDB_val sKey = { sizeof(sPaymentID), (void *)&sPaymentID };

    auto getResult = mdb_cursor_get(sCurPaymentIndex, &sKey, (MDB_val *)&cZeroKey, MDB_GET_BOTH);
    if (getResult != 0 && getResult != MDB_NOTFOUND) {
        throw(DB_ERROR(lmdbError("Error finding PaymentID to remove", getResult).c_str()));
    }

    if (!getResult) {
        getResult = mdb_cursor_del(sCurPaymentIndex, 0);
        if (getResult) {
            throw(DB_ERROR(
                    lmdbError("Error removal of PaymentID to db transaction", getResult).c_str()));
        }
    }
}

bool BlockchainLMDB::addTimestampIndex(uint64_t uTimestamp, const Crypto::Hash &sTxHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TimestampIndex)

    int iRes;

    MDBValSet(sValTimestamp, uTimestamp);
    MDBValSet(sValTxHash, sTxHash);

    iRes = mdb_cursor_put(sCurTimestampIndex, &sValTimestamp, &sValTxHash, 0);
    if (iRes) {
        throw(DB_ERROR(lmdbError("Failed to add Timestamp to db transaction: ", iRes).c_str()));
    }

    return true;
}

bool BlockchainLMDB::getTimestampIndex(uint64_t uTimestamp, Crypto::Hash &sTxHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TimestampIndex);

    bool bReturn = true;
    FMdbValCopy<uint64_t> sKeyHeight(uTimestamp);
    MDB_val sResult;

    auto getResult = mdb_cursor_get(sCurTimestampIndex, &sKeyHeight, &sResult, MDB_SET);
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". getResult:" << getResult;
    if (getResult == MDB_NOTFOUND) {
        bReturn = false;
        throw(BLOCK_DNE(std::string("Attempt to get tx hash from timestamp ")
                                .append(boost::lexical_cast<std::string>(uTimestamp))
                                .append(" failed -- timestamp not in db")
                                .c_str()));
    } else if (getResult) {
        bReturn = false;
        throw(DB_ERROR("Error attempting to retrieve a block from the db"));
    }

    Crypto::Hash &sHash = *(Crypto::Hash *)sResult.mv_data;

    sTxHash = sHash;
    return bReturn;
}

bool BlockchainLMDB::getTimestampIndicesInRange(uint64_t uTimestampBegin, uint64_t uTimestampEnd,
                                                uint64_t uReturnHashesLimit,
                                                std::vector<Crypto::Hash> &vTxHashes,
                                                uint64_t &uHashesWithTimestamps)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    uint64_t uMaxHashNumber = 0;

    for (uint64_t uI = uTimestampBegin; uI <= uTimestampEnd && uMaxHashNumber < uReturnHashesLimit;
         ++uI) {
        Crypto::Hash sTempHash;
        if (!getTimestampIndex(uI, sTempHash)) {
            // Some log
            return false;
        }

        uHashesWithTimestamps++;
        vTxHashes.push_back(sTempHash);
    }

    return true;
}

bool BlockchainLMDB::removeTimestampIndex(uint64_t uTimestamp, const Crypto::Hash &sHash)
{
    mLogger(INFO, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TimestampIndex)

    int iRes;
    MDB_val sKey = { sizeof(uTimestamp), (void *)&uTimestamp };
    MDB_val sVal = { sizeof(sHash), (void *)&sHash };

    auto getResult = mdb_cursor_get(sCurTimestampIndex, &sKey, &sVal, MDB_GET_BOTH);
    if (getResult != 0 && getResult != MDB_NOTFOUND) {
        throw(DB_ERROR(lmdbError("Error finding Timestamp to remove", getResult).c_str()));
    }

    if (!getResult) {
        getResult = mdb_cursor_del(sCurTimestampIndex, 0);
        if (getResult) {
            throw(DB_ERROR(
                    lmdbError("Error removal of Timestamp to db transaction", getResult).c_str()));
        }
    }
}

bool BlockchainLMDB::addTimeToLifeIndex(const Crypto::Hash &sTxHash, uint64_t uTimeToLife)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TimeToLifeIndex)

    int iRes;

    MDBValSet(sValTxHash, sTxHash);
    MDBValSet(sValTtl, uTimeToLife);

    iRes = mdb_cursor_put(sCurTimestampIndex, &sValTxHash, &sValTtl, 0);
    if (iRes) {
        throw(DB_ERROR(lmdbError("Failed to add Timestamp to db transaction: ", iRes).c_str()));
    }

    return true;
}

uint64_t BlockchainLMDB::getTimeToLife(const Crypto::Hash &sTxHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(TimeToLifeIndex);

    FMdbValCopy<Crypto::Hash> sKeyHash(sTxHash);
    MDB_val sResult;

    auto getResult = mdb_cursor_get(sCurTimeToLifeIndex, &sKeyHash, &sResult, MDB_SET);
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". getResult:" << getResult;
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get ttl from TxHash ")
                                .append(boost::lexical_cast<std::string>(sTxHash))
                                .append(" failed -- sTxHash not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block from the db"));
    }

    uint64_t &uRes = *(uint64_t *)sResult.mv_data;

    return uRes;
}

bool BlockchainLMDB::removeTimeToLifeIndex(const Crypto::Hash &sTxHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TimeToLifeIndex)

    FMdbValCopy<Crypto::Hash> sKeyHash(sTxHash);
    MDB_val sResult;

    auto getResult = mdb_cursor_get(sCurTimeToLifeIndex, &sKeyHash, &sResult, MDB_SET);
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". getResult:" << getResult;
    if (getResult != 0 && getResult != MDB_NOTFOUND) {
        throw(DB_ERROR(lmdbError("Error finding ttl hash to remove", getResult).c_str()));
    }

    if (!getResult) {
        getResult = mdb_cursor_del(sCurTimeToLifeIndex, 0);
        if (getResult) {
            throw(DB_ERROR(
                    lmdbError("Error removal of ttl from db transaction", getResult).c_str()));
        }
    }
}

uint64_t BlockchainLMDB::addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                                  const CryptoNote::difficulty_type &uCumulativeDifficulty,
                                  const uint64_t &uCoinsGenerated,
                                  const std::vector<CryptoNote::Transaction> &transactions)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << "0";
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                << "0. tx size: " << transactions.size();
    checkOpen();
    uint64_t uHeight = height();

    try {
        BlockchainDB::addBlock(block, uBlockSize, uCumulativeDifficulty, uCoinsGenerated,
                               transactions);
    } catch (const DB_ERROR_TXN_START) {
        throw;
    } catch (...) {
        blockTxnAbort();
        throw;
    }

    return uHeight++;
}

void BlockchainLMDB::popBlock(CryptoNote::Block &sBlock,
                              std::vector<CryptoNote::Transaction> &vTransactions)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    blockTxnStart(false);

    try {
        BlockchainDB::popBlock(sBlock, vTransactions);
        blockTxnStop();
    } catch (...) {
        blockTxnAbort();
        throw;
    }
}

bool BlockchainLMDB::blockExists(const Crypto::Hash &sHash, uint64_t *height) const
{
    // mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockHeights);

    bool bReturn = false;
    MDBValSet(sValHash, sHash);
    auto getResult =
            mdb_cursor_get(sCurBlockHeights, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        // mLogger(DEBUGGING, BRIGHT_CYAN) << " Block with hash " << sHash << " not found in DB";
    } else if (getResult) {
        throw(DB_ERROR(lmdbError("DB error attempting to fetch block index from hash", getResult)
                               .c_str()));
    } else {
        if (height) {
            const auto *sBlockHeight = (const FBlockHeight *)sValHash.mv_data;
            *height = sBlockHeight->uHeight;
        }

        bReturn = true;
    }

    TXN_POSTFIX_READONLY();

    return bReturn;
}

CryptoNote::blobData BlockchainLMDB::getBlockBlob(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    CryptoNote::blobData sBlob = getBlockBlobFromHeight(getBlockHeight(sHash));

    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". sBlob: " << sBlob;

    return sBlob;
}

uint64_t BlockchainLMDB::getBlockHeight(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockHeights);

    MDBValSet(sValHash, sHash);
    auto getResult =
            mdb_cursor_get(sCurBlockHeights, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE("Attempted to retrieve non-existent block height"));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block height from the db"));
    }

    FBlockHeight *sBHe = (FBlockHeight *)sValHash.mv_data;
    uint64_t uRet = sBHe->uHeight;

    TXN_POSTFIX_READONLY();

    return uRet;
}

CryptoNote::BlockHeader BlockchainLMDB::getBlockHeader(const Crypto::Hash &sHash) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    // BlockHeader object is automatically cast from block object
    return getBlock(sHash);
}

CryptoNote::blobData BlockchainLMDB::getBlockBlobFromHeight(const uint64_t &uHeight) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(Blocks);

    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uHeight:" << uHeight;
    FMdbValCopy<uint64_t> sKeyHeight(uHeight);
    MDB_val sResult;
    auto getResult = mdb_cursor_get(sCurBlocks, &sKeyHeight, &sResult, MDB_SET);
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". getResult:" << getResult;
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get block from height ")
                                .append(boost::lexical_cast<std::string>(uHeight))
                                .append(" failed -- block not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block from the db"));
    }

    CryptoNote::blobData sBlobData;
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". sResult.mv_data: "
                                << reinterpret_cast<char *>(sResult.mv_data);
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                << ". sResult.mv_size: " << sResult.mv_size;
    sBlobData.assign(reinterpret_cast<char *>(sResult.mv_data), sResult.mv_size);

    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". sBlobData: " <<
    // sBlobData;

    TXN_POSTFIX_READONLY();

    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". return";
    return sBlobData;
}

uint64_t BlockchainLMDB::getBlockTimestamp(const uint64_t &uHeight) const
{
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockInfo);

    MDBValSet(sValHeight, uHeight);
    auto getResult =
            mdb_cursor_get(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sValHeight, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get block timestamp from height ")
                                .append(boost::lexical_cast<std::string>(uHeight))
                                .append(" failed -- block height not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block timestamp from the db"));
    }

    FBlockInfo *sBI = (FBlockInfo *)sValHeight.mv_data;
    uint64_t uRet = sBI->uBITimestamp;

    TXN_POSTFIX_READONLY();

    return uRet;
}

uint64_t BlockchainLMDB::getTopBlockTimestamp() const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    uint64_t uHeight = height();

    if (uHeight < 1) {
        return 0;
    }

    return getBlockTimestamp(uHeight - 1);
}

size_t BlockchainLMDB::getBlockSize(const uint64_t &uHeight) const
{
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockInfo);

    MDBValSet(sValHeight, uHeight);
    auto getResult =
            mdb_cursor_get(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sValHeight, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get block size from height ")
                                .append(boost::lexical_cast<std::string>(uHeight))
                                .append(" failed -- block size not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block size from the db"));
    }

    FBlockInfo *sBIn = (FBlockInfo *)sValHeight.mv_data;
    size_t uRet = sBIn->uBISize;

    TXN_POSTFIX_READONLY();

    return uRet;
}

CryptoNote::difficulty_type
BlockchainLMDB::getBlockCumulativeDifficulty(const uint64_t &uHeight) const
{
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockInfo);

    MDBValSet(sValHeight, uHeight);
    auto getResult =
            mdb_cursor_get(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sValHeight, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get cumulative difficulty from height ")
                                .append(boost::lexical_cast<std::string>(uHeight))
                                .append(" failed -- block height not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a cumulative difficulty from the db"));
    }

    FBlockInfo *sBI = (FBlockInfo *)sValHeight.mv_data;
    uint64_t uRet = sBI->uBIDifficulty;

    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uRet: " << uRet;

    return uRet;
}

CryptoNote::difficulty_type BlockchainLMDB::getBlockDifficulty(const uint64_t &uHeight) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
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
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockInfo);

    MDBValSet(sValHeight, uHeight);
    auto getResult =
            mdb_cursor_get(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sValHeight, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get generated coins from height ")
                                .append(boost::lexical_cast<std::string>(uHeight))
                                .append(" failed -- block size not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a total generated coins from the db"));
    }

    FBlockInfo *sBI = (FBlockInfo *)sValHeight.mv_data;
    uint64_t uRet = sBI->uBIGeneratedCoins;

    TXN_POSTFIX_READONLY();

    return uRet;
}

Crypto::Hash BlockchainLMDB::getBlockHashFromHeight(const uint64_t &uHeight) const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    TXN_PREFIX_READONLY();
    READ_CURSOR(BlockInfo);

    MDBValSet(sValHeight, uHeight);
    auto getResult =
            mdb_cursor_get(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sValHeight, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get block hash from height ")
                                .append(boost::lexical_cast<std::string>(uHeight))
                                .append(" failed -- block size not in db")
                                .c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block hash from the db"));
    }

    FBlockInfo *sBI = (FBlockInfo *)sValHeight.mv_data;
    Crypto::Hash sRet = sBI->sBIHash;

    TXN_POSTFIX_READONLY();

    return sRet;
}

std::vector<CryptoNote::Block> BlockchainLMDB::getBlocksRange(const uint64_t &uStartHeight,
                                                              const uint64_t &uEndHeight)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    std::vector<CryptoNote::Block> vBlocks;

    for (uint64_t uHeight = uStartHeight; uHeight <= uEndHeight; ++uHeight) {
        vBlocks.push_back(getBlockFromHeight(uHeight));
    }

    return vBlocks;
}

std::vector<Crypto::Hash> BlockchainLMDB::getHashesRange(const uint64_t &uStartHeight,
                                                         const uint64_t &uEndHeight)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    std::vector<Crypto::Hash> vHashes;

    for (uint64_t uHeight = uStartHeight; uHeight <= uEndHeight; ++uHeight) {
        vHashes.push_back(getBlockHashFromHeight(uHeight));
    }

    return vHashes;
}

void BlockchainLMDB::removeBlock()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    int iResult;
    uint64_t uHeight = height();

    if (uHeight < 1) {
        throw(BLOCK_DNE("Attempting to remove block from an empty DB"));
        return;
    }

    FMdbTxnCursors *sCursor = &mWriteCursors;
    CURSOR(BlockInfo)
    CURSOR(BlockHeights)
    CURSOR(Blocks)
    FMdbValCopy<uint64_t> sValHeight(uHeight - 1);
    MDB_val sHeight = sValHeight;

    if ((iResult = mdb_cursor_get(sCurBlockInfo, (MDB_val *)&cZeroKVal, &sHeight, MDB_GET_BOTH))) {
        throw(BLOCK_DNE(
                lmdbError("Attempting to remove block that's not in the db: ", iResult).c_str()));
    }

    // Need to use sHeight now; deleting from mBlockInfo will invalidate it
    FBlockInfo *sBi = (FBlockInfo *)sHeight.mv_data;
    FBlockHeight sBh = { sBi->sBIHash, 0 };

    sHeight.mv_data = (void *)&sBh;
    sHeight.mv_size = sizeof(sBh);

    if ((iResult =
                 mdb_cursor_get(sCurBlockHeights, (MDB_val *)&cZeroKVal, &sHeight, MDB_GET_BOTH))) {
        throw(DB_ERROR(
                lmdbError("Failed to locate block height by hash for removal: ", iResult).c_str()));
    }

    if ((iResult = mdb_cursor_del(sCurBlockHeights, 0))) {
        throw(DB_ERROR(
                lmdbError("Failed to add removal of block height by hash to db transaction: ",
                          iResult)
                        .c_str()));
    }

    if ((iResult = mdb_cursor_get(sCurBlocks, &sValHeight, NULL, MDB_SET))) {
        throw(DB_ERROR(lmdbError("Failed to locate block for removal: ", iResult).c_str()));
    }

    if ((iResult = mdb_cursor_del(sCurBlocks, 0))) {
        throw(DB_ERROR(
                lmdbError("Failed to add removal of block to db transaction: ", iResult).c_str()));
    }

    if ((iResult = mdb_cursor_del(sCurBlockInfo, 0))) {
        throw(DB_ERROR(lmdbError("Failed to add removal of block info to db transaction: ", iResult)
                               .c_str()));
    }
};

uint64_t BlockchainLMDB::addTransactionData(const Crypto::Hash &sBlockHash,
                                            const CryptoNote::Transaction &sTransaction,
                                            const Crypto::Hash &sTxHash)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;
    uint64_t uHeight = height();

    int iResult;
    uint64_t uTxId = getTransactionCount();
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ". uTxId: " << uTxId;
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                << ". sTxHash: " << Common::podToHex(sTxHash);

    CURSOR(Transactions)
    CURSOR(TransactionIndices)

    MDBValSet(sValTxId, uTxId);
    MDBValSet(sValTxHash, sTxHash);

    iResult = mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValTxHash,
                             MDB_GET_BOTH);

    if (iResult == 0) {
        FTransactionIndex *sTxIndexP = (FTransactionIndex *)sValTxHash.mv_data;
        throw(TX_EXISTS(
                std::string("Attempting to add transaction that's already in the db (tx id ")
                        .append(boost::lexical_cast<std::string>(sTxIndexP->data.uTxID))
                        .append(")")
                        .c_str()));
    } else if (iResult != MDB_NOTFOUND) {
        throw(DB_ERROR(lmdbError(std::string("Error checking if tx index exists for tx hash ")
                                         + Common::podToHex(sTxHash) + ": ",
                                 iResult)
                               .c_str()));
    }

    FTransactionIndex sTxIndex;
    sTxIndex.key = sTxHash;
    sTxIndex.data.uTxID = uTxId;
    sTxIndex.data.uUnlockTime = sTransaction.unlockTime;
    sTxIndex.data.uBlockID = uHeight;

    sValTxHash.mv_size = sizeof(sTxIndex);
    sValTxHash.mv_data = (void *)&sTxIndex;

    iResult = mdb_cursor_put(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValTxHash, 0);
    if (iResult) {
        throw(DB_ERROR(lmdbError("Failed to add tx data to db transaction: ", iResult).c_str()));
    }

    blobData sTxBlobData = transactionToBlob(sTransaction);
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                << ". sTxBlobData: " << sTxBlobData << ENDL << ENDL;
    FMdbValCopy<blobData> cBlob(sTxBlobData);
    iResult = mdb_cursor_put(sCurTransactions, &sValTxId, &cBlob, MDB_APPEND);
    if (iResult) {
        throw(DB_ERROR(lmdbError("Failed to add tx blob to db transaction: ", iResult).c_str()));
    }

    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__
            << ". Added Tx with hash: " << Common::podToHex(sTxHash) << ENDL << ", uTxID: " << uTxId
            << ENDL << ", sTransaction.unlockTime: " << sTransaction.unlockTime << ENDL
            << ", in block height: " << uHeight << ", successfully.";

    return uTxId;
}

void BlockchainLMDB::removeTransactionData(const Crypto::Hash &sTxHash,
                                           const CryptoNote::Transaction &tx)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    int iResult;
    uint64_t uTxId = getTransactionCount();
    FMdbTxnCursors *sCursor = &mWriteCursors;
    CURSOR(Transactions)
    CURSOR(TransactionIndices)
    CURSOR(TransactionOutputs)
    CURSOR(PaymentIndex)
    CURSOR(TimestampIndex)

    MDBValSet(sValHash, sTxHash);
    if (mdb_cursor_get(sCurTransactionIndices, (MDB_val *)&cZeroKVal, &sValHash, MDB_GET_BOTH)) {
        throw(TX_DNE("Attempting to remove transaction that isn't in the db"));
    }

    FTransactionIndex *sTIn = (FTransactionIndex *)sValHash.mv_data;
    MDBValSet(sValTxIndex, sTIn->data.uTxID);
    if ((iResult = mdb_cursor_get(sCurTransactions, &sValTxIndex, NULL, MDB_SET))) {
        throw(DB_ERROR(lmdbError("Failed to locate tx for removal: ", iResult).c_str()));
    }

    uint64_t uTempTxId = sTIn->data.uTxID;
    uint64_t uTempUnlockTime = sTIn->data.uUnlockTime;
    uint64_t uTempHeight = sTIn->data.uBlockID;
    Crypto::Hash sTempHash = sTIn->key;

    iResult = mdb_cursor_del(sCurTransactions, 0);
    if (iResult) {
        throw(DB_ERROR(
                lmdbError("Failed to add removal of tx to db transaction: ", iResult).c_str()));
    }

    // removeTxOutputs

    removeTransactionOutputs(sTIn->data.uTxID, tx);

    iResult = mdb_cursor_get(sCurTransactionOutputs, &sValTxIndex, NULL, MDB_SET);
    if (!iResult) {
        iResult = mdb_cursor_del(sCurTransactionOutputs, 0);
    }

    // Don't delete the tx_indices entry until the end, after we're done with val_tx_id
    if (mdb_cursor_del(sCurTransactionIndices, 0)) {
        throw(DB_ERROR("Failed to add removal of tx index to db transaction"));
    }

    // Remove PaymentIndex
    // Remove TimestampIndex

    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__
            << ". Removed Tx with hash: " << Common::podToHex(sTempHash) << ENDL
            << ", uTxID: " << uTempTxId << ENDL << ", sTransaction.unlockTime: " << uTempUnlockTime
            << ENDL << ", in block height: " << uTempHeight << ENDL << ", successfully.";
}

uint64_t BlockchainLMDB::addOutput(const Crypto::Hash &sTxHash,
                                   const CryptoNote::TransactionOutput &sTxOutput,
                                   const uint64_t &uIndex, const uint64_t &uUnlockTime)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << ENDL
                                << "  sTxHash: " << Common::podToHex(sTxHash) << ENDL
                                << " sTxOutput.amount: " << sTxOutput.amount << ENDL
                                << "  uIndex: " << uIndex << ENDL
                                << "  uUnlockTime: " << uUnlockTime;
    checkOpen();

    FMdbTxnCursors *sCursor = &mWriteCursors;

    uint64_t uHeight = height();
    uint64_t uNumOutputs = numOutputs();
    int result = 0;

    CURSOR(OutputTransactions)

    FOutTx sOutTx = { uNumOutputs, sTxHash, uIndex };
    MDBValSet(sValOutTx, sOutTx);
    result = mdb_cursor_put(sCurOutputTransactions, (MDB_val *)&cZeroKVal, &sValOutTx,
                            MDB_APPENDDUP);
    if (result) {
        throw(DB_ERROR(
                lmdbError("Failed to add output tx hash to db transaction: ", result).c_str()));
    }

    CURSOR(OutputAmounts)

    FOutputKey sOutKey;
    MDB_val sData;
    FMdbValCopy<uint64_t> sValAmount(sTxOutput.amount);
    result = mdb_cursor_get(sCurOutputAmounts, &sValAmount, &sData, MDB_SET);
    if (!result) {
        mdb_size_t uNumElements = 0;
        result = mdb_cursor_count(sCurOutputAmounts, &uNumElements);
        if (result) {
            throw(DB_ERROR(std::string("Failed to get number of outputs for amount: ")
                                   .append(mdb_strerror(result))
                                   .c_str()));
        }

        sOutKey.uAmountIndex = uNumElements;
    } else if (result != MDB_NOTFOUND) {
        throw(DB_ERROR(
                lmdbError("Failed to get output amount in db transaction: ", result).c_str()));
    } else {
        sOutKey.uAmountIndex = 0;
    }

    if (sTxOutput.target.type() == typeid(KeyOutput)) {
        sOutKey.sData.sPublicKey = boost::get<KeyOutput>(sTxOutput.target).key;
    } else if (sTxOutput.target.type() == typeid(KeyOutput)) {
        throw(DB_ERROR(std::string("Transaction output should be of type KeyOutput!").c_str()));
    }

    sOutKey.uOutputId = uNumOutputs;
    sOutKey.sData.uUnlockTime = uUnlockTime;
    sOutKey.sData.uHeight = uHeight;
    sData.mv_size = sizeof(FOutputKey);
    sData.mv_data = &sOutKey;

    if ((result = mdb_cursor_put(sCurOutputAmounts, &sValAmount, &sData, MDB_APPENDDUP))) {
        throw(DB_ERROR(
                lmdbError("Failed to add output pubkey to db transaction: ", result).c_str()));
    }

    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                    << ". Adding output with amount: " << sTxOutput.amount << ENDL
                                    << ", uAmountIndex: " << sOutKey.uAmountIndex << ENDL
                                    << ", uOutputId: " << sOutKey.uOutputId << ENDL
                                    << ", uUnlockTime: " << sOutKey.sData.uUnlockTime << ENDL
                                    << ", uHeight: " << sOutKey.sData.uHeight << ENDL
                                    << ", sPublicKey: " << Common::podToHex(sOutKey.sData.sPublicKey)
                                    << ENDL << ", successfully.";

    mdb_cursor_close(sCurOutputTransactions);
    mdb_cursor_close(sCurOutputAmounts);
    return sOutKey.uAmountIndex;
}

void BlockchainLMDB::addTransactionAmountOutputIndices(
        const uint64_t uTxId, const std::vector<uint64_t> &vAmountOutputIndices)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(TransactionOutputs)

    int result = 0;
    int iNumOutputs = vAmountOutputIndices.size();

    MDBValSet(sValTxId, uTxId);
    MDB_val sVal;
    sVal.mv_data = (void *)vAmountOutputIndices.data();
    sVal.mv_size = sizeof(uint64_t) * iNumOutputs;
    /*
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__
                                    << ". tx_outputs[tx_hash] size: "
                                    << sVal.mv_size;
*/
    result = mdb_cursor_put(sCurTransactionOutputs, &sValTxId, &sVal, MDB_APPEND);
    if (result) {
        throw(DB_ERROR(
                std::string(
                        "Failed to add <tx hash, amount output index array> to db transaction: ")
                        .append(mdb_strerror(result))
                        .c_str()));
    }
}

void BlockchainLMDB::removeTransactionOutputs(const uint64_t uTxId,
                                              const CryptoNote::Transaction &sTransaction)
{
    std::vector<uint64_t> vAmountOutputIndices = getTransactionAmountOutputIndices(uTxId);

    if (vAmountOutputIndices.empty()) {
        if (sTransaction.outputs.empty()) {

        } else {
            throw(DB_ERROR("tx has outputs, but no output indices found"));
        }
    }

    for (uint64_t i = sTransaction.outputs.size(); i-- > 0;) {
        uint64_t uAmount = sTransaction.outputs[i].amount;
        removeOutput(uAmount, vAmountOutputIndices[i]);
    }
}

void BlockchainLMDB::removeOutput(const uint64_t uAmount, const uint64_t &uOutIndex)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(OutputAmounts)
    CURSOR(OutputTransactions)

    MDBValSet(sValKey, uAmount);
    MDBValSet(sVal, uOutIndex);

    auto getResult = mdb_cursor_get(sCurOutputAmounts, &sValKey, &sVal, MDB_GET_BOTH);
    if (getResult) {
        throw(DB_ERROR(lmdbError("DB error attempting to get an output", getResult).c_str()));
    }

    const FOutputKey *sOKe = (const FOutputKey *)sVal.mv_data;
    MDBValSet(sValOTKe, sOKe->uOutputId);

    getResult =
            mdb_cursor_get(sCurOutputTransactions, (MDB_val *)&cZeroKVal, &sValOTKe, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(DB_ERROR("Unexpected: global output index not found in mOutputTransactions"));
    } else if (getResult) {
        throw(DB_ERROR(lmdbError("Error adding removal of output tx to db transaction", getResult)
                               .c_str()));
    }

    getResult = mdb_cursor_del(sCurOutputTransactions, 0);
    if (getResult) {
        throw(DB_ERROR(
                lmdbError(std::string("Error deleting output index ")
                                  .append(boost::lexical_cast<std::string>(uOutIndex).append(": "))
                                  .c_str(),
                          getResult)
                        .c_str()));
    }

    getResult = mdb_cursor_del(sCurOutputAmounts, 0);
    if (getResult) {
        throw(DB_ERROR(
                lmdbError(std::string("Error deleting amount for output index ")
                                  .append(boost::lexical_cast<std::string>(uOutIndex).append(": "))
                                  .c_str(),
                          getResult)
                        .c_str()));
    }
}

void BlockchainLMDB::addSpentKey(const KeyImage &sSpentKeyImage)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(SpentKeys)

    MDB_val sKey = { sizeof(sSpentKeyImage), (void *)&sSpentKeyImage };
    if (auto result = mdb_cursor_put(sCurSpentKeys, (MDB_val *)&cZeroKVal, &sKey, MDB_NODUPDATA)) {
        if (result == MDB_KEYEXIST) {
            throw(KEY_IMAGE_EXISTS("Attempting to add spent key image that's already in the db"));
        } else {
            throw(DB_ERROR(
                    lmdbError("Error adding spent key image to db transaction: ", result).c_str()));
        }
    }
}

void BlockchainLMDB::removeSpentKey(const Crypto::KeyImage &sSpentKeyImage)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    checkOpen();
    FMdbTxnCursors *sCursor = &mWriteCursors;

    CURSOR(SpentKeys)

    MDB_val sKey = { sizeof(sSpentKeyImage), (void *)&sSpentKeyImage };
    auto result = mdb_cursor_get(sCurSpentKeys, (MDB_val *)&cZeroKVal, &sKey, MDB_GET_BOTH);
    if (result != 0 && result != MDB_NOTFOUND) {
        throw(DB_ERROR(lmdbError("Error finding spent key to remove", result).c_str()));
    }

    if (!result) {
        result = mdb_cursor_del(sCurSpentKeys, 0);
        if (result) {
            throw(DB_ERROR(lmdbError("Error adding removal of key image to db transaction", result)
                                   .c_str()));
        }
    }
}

BlockchainLMDB::~BlockchainLMDB()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
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
    : BlockchainDB(sLogger), mLogger(sLogger, "LMDB")
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
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
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
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
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    return Tools::getDefaultDBType();
}

void BlockchainLMDB::open(const std::string &cFileName, const int iDBFlags)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
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
            throw DB_OPEN_FAILURE(
                    std::string("Failed to create directory ").append(cFileName).c_str());
        }
    }

    // Check for existing LMDB files in base directory
    boost::filesystem::path sOldFiles = sDir.parent_path();
    if (boost::filesystem::exists(sOldFiles / parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME)
        || boost::filesystem::exists(sOldFiles
                                     / parameters::CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME)) {
        mLogger(INFO, BRIGHT_CYAN) << "Found existing LMDB files in " << sOldFiles.string();
        mLogger(INFO, BRIGHT_CYAN)
                << "Move " << parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME << " and/or "
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

    // size_t uMapSize = DEFAULT_MAPSIZE;
    size_t uMapSize = 128 * 1024 * 1024;

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

    FMdbTxnSafe::preventNewTxns();
    if (needResize()) {
        mLogger(INFO, BRIGHT_CYAN) << "LMDB memory map needs to be resized, doing that now.";
        doResize(uMapSize);
    } else {
        FMdbTxnSafe::allowNewTxns();
    }

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
    lmdbOpen(sTxn, LMDB_BLOCKS, MDB_INTEGERKEY | MDB_CREATE, mBlocks,
             "Failed to open db handle for mBlocks");
    lmdbOpen(sTxn, LMDB_BLOCK_INFO, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
             mBlockInfo, "Failed to open db handle for mBlockInfo");
    lmdbOpen(sTxn, LMDB_BLOCK_HEIGHTS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
             mBlockHeights, "Failed to open db handle for mBlockHeights");

    lmdbOpen(sTxn, LMDB_TRANSACTIONS, MDB_INTEGERKEY | MDB_CREATE, mTransactions,
             "Failed to open db handle for mTransactions");
    lmdbOpen(sTxn, LMDB_TRANSACTION_INDICES,
             MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mTransactionIndices,
             "Failed to open db handle for mTransactionIndices");
    lmdbOpen(sTxn, LMDB_TRANSACTION_OUTPUTS, MDB_INTEGERKEY | MDB_CREATE, mTransactionOutputs,
             "Failed to open db handle for mTransactionOutputs");

    lmdbOpen(sTxn, LMDB_OUTPUT_TRANSACTIONS,
             MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mOutputTransactions,
             "Failed to open db handle for mOutputTransactions");
    lmdbOpen(sTxn, LMDB_OUTPUT_AMOUNTS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
             mOutputAmounts, "Failed to open db handle for mOutputTransactions");

    lmdbOpen(sTxn, LMDB_SPENT_KEYS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
             mSpentKeys, "Failed to open db handle for mSpentKeys");

    lmdbOpen(sTxn, LMDB_TRANSACTIONPOOL_META, MDB_CREATE, mTransactionPoolMeta,
             "Failed to open db handle for mTransactionPoolMeta");
    lmdbOpen(sTxn, LMDB_TRANSACTIONPOOL_BLOB, MDB_CREATE, mTransactionPoolBlob,
             "Failed to open db handle for mTransactionPoolBlob");

    lmdbOpen(sTxn, LMDB_PAYMENT_INDEX, MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mPaymentIndex,
             "Failed to open db handle for mPaymentIndex");
    lmdbOpen(sTxn, LMDB_TIMESTAMP_INDEX, MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, mTimestampIndex,
             "Failed to open db handle for mTimestampIndex");
    lmdbOpen(sTxn, LMDB_TIME_TO_LIFE_INDEX, MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
             mTimeToLifeIndex, "Failed to open db handle for mTimeToLifeIndex");

    lmdbOpen(sTxn, LMDB_PROPERTIES, MDB_CREATE, mProperties,
             "Failed to open db handle for mProperties");

    mdb_set_dupsort(sTxn, mSpentKeys, compareHash32);
    mdb_set_dupsort(sTxn, mBlockHeights, compareHash32);
    mdb_set_dupsort(sTxn, mTransactionIndices, compareHash32);
    mdb_set_dupsort(sTxn, mOutputAmounts, compareUInt64);
    mdb_set_dupsort(sTxn, mOutputTransactions, compareUInt64);
    mdb_set_dupsort(sTxn, mBlockInfo, compareUInt64);

    mdb_set_compare(sTxn, mTransactionPoolMeta, compareHash32);
    mdb_set_compare(sTxn, mTransactionPoolBlob, compareHash32);
    mdb_set_compare(sTxn, mPaymentIndex, compareHash32);
    mdb_set_compare(sTxn, mTimestampIndex, compareUInt64);
    mdb_set_compare(sTxn, mTimeToLifeIndex, compareHash32);
    mdb_set_compare(sTxn, mProperties, compareString);

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
        if (*(const uint32_t *)sV.mv_data > VERSION) {
            bCompatible = false;
        }

#if VERSION > 0
        else if (*(const uint32_t *)sV.mv_data < VERSION) {
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
                mLogger(DEBUGGING, BRIGHT_GREEN)
                        << "Successfully written Version " << VERSION << " to database";
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
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    if (mBatchActive) {
        mLogger(DEBUGGING, BRIGHT_CYAN)
                << "close() first calling batchAbort() due to active batch transaction";
        batchAbort();
    }

    this->sync();
    mTInfo.reset();

    mdb_env_close(mDbEnv);
    pOpen = false;
    pIsResizing = false;
}

void BlockchainLMDB::sync()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    checkOpen();

    if (isReadOnly()) {
        return;
    }

    // Does nothing unless LMDB environment was opened with MDB_NOSYNC or in part
    // MDB_NOMETASYNC. Force flush to be synchronous.
    if (auto sResult = mdb_env_sync(mDbEnv, true)) {
        throw(DB_ERROR(lmdbError("Failed to sync database: ", sResult).c_str()));
    }
}

void BlockchainLMDB::safeSyncMode(const bool bOnOff)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    mLogger(DEBUGGING, BRIGHT_CYAN) << "switching safe mode " << (bOnOff ? "on" : "off");
    mdb_env_set_flags(mDbEnv, MDB_NOSYNC | MDB_MAPASYNC, !bOnOff);
}

void BlockchainLMDB::reset()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    checkOpen();

    FMdbTxnSafe sTxn;

    if (auto sResult = lmdbTxnBegin(mDbEnv, NULL, 0, sTxn)) {
        throw(DB_ERROR(lmdbError("Failed to create a transaction for the db: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mBlocks, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mBlocks: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mBlockInfo, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mBlockInfo: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mBlockHeights, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mBlockHeights: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mTransactions, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mTransactions: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mTransactionIndices, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mTransactionIndices: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mTransactionOutputs, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mTransactionOutputs: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mOutputAmounts, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mOutputAmounts: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mOutputTransactions, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mOutputTransactions: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mSpentKeys, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mSpentKeys: ", sResult).c_str()));
    }

    if (auto sResult = mdb_drop(sTxn, mProperties, 0)) {
        throw(DB_ERROR(lmdbError("Failed to drop mProperties: ", sResult).c_str()));
    }

    FMdbValCopy<const char *> k("version");
    FMdbValCopy<uint32_t> v(VERSION);

    if (auto sResult = mdb_put(sTxn, mProperties, &k, &v, 0)) {
        throw(DB_ERROR(lmdbError("Failed to write version to database: ", sResult).c_str()));
    }

    sTxn.commit();
    mCumSize = 0;
    mCumCount = 0;
    resetStats();
}

bool BlockchainLMDB::lock()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    checkOpen();

    return false;
}

void BlockchainLMDB::unlock()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    checkOpen();
}

void BlockchainLMDB::blockTxnStart(bool bReadOnly)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

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
        throw(DB_ERROR_TXN_START(
                (std::string("Attempted to start new write txn when write txn already exists in ")
                 + __func__)
                        .c_str()));
    }

    if (!mBatchActive) {
        mWriter = boost::this_thread::get_id();
        mWriteDBTxnSafe = new FMdbTxnSafe();

        if (auto sDBRes = lmdbTxnBegin(mDbEnv, NULL, 0, *mWriteDBTxnSafe)) {
            delete mWriteDBTxnSafe;
            mWriteDBTxnSafe = nullptr;
            throw(DB_ERROR_TXN_START(
                    lmdbError("Failed to create a transaction for the db: ", sDBRes).c_str()));
        }
        memset(&mWriteCursors, 0, sizeof(mWriteCursors));
        if (mTInfo.get()) {
            if (mTInfo->sTReadFlags.bRfTxn) {
                mdb_txn_reset(mTInfo->sTReadTxn);
            }
            memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
        }
    } else if (mWriter != boost::this_thread::get_id()) {
        throw(DB_ERROR_TXN_START(
                (std::string("Attempted to start new write txn when batch txn already exists in ")
                 + __func__)
                        .c_str()));
    }
}

void BlockchainLMDB::blockTxnAbort()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

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
        throw(DB_ERROR(
                (std::string("BlockchainLMDB::") + __func__
                 + std::string(
                         ": block-level DB transaction abort called when write txn doesn't exist"))
                        .c_str()));
    }
}

void BlockchainLMDB::blockTxnStop()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    if (!mWriteDBTxnSafe) {
        // throw(DB_ERROR_TXN_START((std::string("Attempted to stop write txn when no such txn
        // exists in ")+__FUNCTION__).c_str()));
    }

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
    // mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    bool bRet = false;
    FMdbThreadInfo *sTInfo;

    if (mWriteDBTxnSafe && mWriter == boost::this_thread::get_id()) {
        *sMdbTxn = mWriteDBTxnSafe->pTxn;
        *sCurs = (FMdbTxnCursors *)&mWriteCursors;

        return bRet;
    }

    if (!(sTInfo = mTInfo.get()) || mdb_txn_env(sTInfo->sTReadTxn) != mDbEnv) {
        sTInfo = new FMdbThreadInfo;
        mTInfo.reset(sTInfo);

        memset(&sTInfo->sTReadCursors, 0, sizeof(sTInfo->sTReadCursors));
        memset(&sTInfo->sTReadFlags, 0, sizeof(sTInfo->sTReadFlags));

        if (auto iMdbRes = lmdbTxnBegin(mDbEnv, nullptr, MDB_RDONLY, &sTInfo->sTReadTxn)) {
            throw(DB_ERROR_TXN_START(
                    lmdbError("Failed to create a read transaction for the db: ", iMdbRes)
                            .c_str()));
        }

        bRet = true;
    } else if (!sTInfo->sTReadFlags.bRfTxn) {
        if (auto iMdbRes = lmdbTxnRenew(sTInfo->sTReadTxn)) {
            throw(DB_ERROR_TXN_START(
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
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    mdb_txn_reset(mTInfo->sTReadTxn);
    memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
}

void BlockchainLMDB::setBatchTransactions(bool bBatchTransactions)
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    if ((bBatchTransactions) && mBatchTransactions) {
        // TODO: some log
    }

    mBatchTransactions = bBatchTransactions;
}

bool BlockchainLMDB::batchStart(uint64_t uBatchNumBlocks, uint64_t uBatchBytes)
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    if (!mBatchTransactions) {
        throw(DB_ERROR("batch transactions not enabled"));
    }

    if (mBatchActive) {
        return false;
    }

    if (mWriteBatchDBTxnSafe != nullptr) {
        return false;
    }

    if (mWriteDBTxnSafe) {
        throw(DB_ERROR("batch transaction attempted, but m_write_txn already in use"));
    }

    checkOpen();
    mWriter = boost::this_thread::get_id();

    checkAndResizeForBatch(uBatchNumBlocks, uBatchNumBlocks);
    mLogger(DEBUGGING, BRIGHT_CYAN)
            << "BlockchainLMDB::" << __func__ << ". Before new FMdbTxnSafe()";
    mWriteBatchDBTxnSafe = new FMdbTxnSafe();

    // NOTE: need to make sure it's destroyed properly when done
    if (auto getDBRes = lmdbTxnBegin(mDbEnv, NULL, 0, *mWriteBatchDBTxnSafe)) {
        delete mWriteBatchDBTxnSafe;
        mWriteBatchDBTxnSafe = nullptr;
        throw(DB_ERROR(lmdbError("Failed to create a transaction for the db: ", getDBRes).c_str()));
    }

    // indicates this transaction is for batch transactions, but not whether it's
    // active
    mWriteBatchDBTxnSafe->pBatchTxn = true;
    mWriteDBTxnSafe = mWriteBatchDBTxnSafe;
    mBatchActive = true;

    memset(&mWriteCursors, 0, sizeof(mWriteCursors));
    if (mTInfo.get()) {
        if (mTInfo->sTReadFlags.bRfTxn) {
            mdb_txn_reset(mTInfo->sTReadTxn);
        }
        memset(&mTInfo->sTReadFlags, 0, sizeof(mTInfo->sTReadFlags));
    }

    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__ << " successfully started";
    return true;
}

void BlockchainLMDB::batchCommit()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    if (!mBatchTransactions) {
        throw(DB_ERROR("Batch transactions not enabled."));
    }

    if (!mBatchActive) {
        throw(DB_ERROR("Batch transaction not in progress."));
    }

    if (mWriteBatchDBTxnSafe == nullptr) {
        throw(DB_ERROR("Batch transaction not in progress."));
    }

    if (mWriter != boost::this_thread::get_id()) {
        throw(DB_ERROR("Batch transactions owned by another thread."));
    }

    checkOpen();
    mWriteDBTxnSafe->commit();
    mWriteDBTxnSafe = nullptr;

    delete mWriteBatchDBTxnSafe;
    mWriteBatchDBTxnSafe = nullptr;
    memset(&mWriteCursors, 0, sizeof(mWriteCursors));
}

void BlockchainLMDB::batchAbort()
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;

    if (!mBatchTransactions) {
        throw(DB_ERROR("Batch transactions not enabled."));
    }

    if (!mBatchActive) {
        throw(DB_ERROR("Batch transaction not in progress."));
    }

    if (mWriteBatchDBTxnSafe == nullptr) {
        throw(DB_ERROR("Batch transaction not in progress."));
    }

    if (mWriter != boost::this_thread::get_id()) {
        throw(DB_ERROR("Batch transactions owned by another thread."));
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

void BlockchainLMDB::batchCleanup()
{
    mLogger(DEBUGGING, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    mWriteDBTxnSafe = nullptr;
    delete mWriteDBTxnSafe;
    mWriteBatchDBTxnSafe = nullptr;
    delete mWriteBatchDBTxnSafe;
    mBatchActive = false;
    memset(&mWriteCursors, 0, sizeof(mWriteCursors));
}

void BlockchainLMDB::batchStop()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    if (!mBatchTransactions) {
        throw(DB_ERROR("Batch transactions not enabled."));
    }

    if (!mBatchActive) {
        throw(DB_ERROR("Batch transaction not active."));
    }

    if (mWriteBatchDBTxnSafe == nullptr) {
        throw(DB_ERROR("Batch transaction not in progress."));
    }

    if (mWriter != boost::this_thread::get_id()) {
        throw(DB_ERROR("Batch transactions owned by another thread."));
    }

    checkOpen();
    TIME_MEASURE_START(uTime)
    try {
        mWriteDBTxnSafe->commit();
        TIME_MEASURE_FINISH(uTime)
        gTimeCommit += uTime;
        batchCleanup();
    } catch (const std::exception &e) {
        batchCleanup();
        throw;
    }
}

bool BlockchainLMDB::isReadOnly() const
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    unsigned int iFlags;
    auto getResult = mdb_env_get_flags(mDbEnv, &iFlags);
    if (getResult) {
        throw(DB_ERROR(lmdbError("Error getting database environment info: ", getResult).c_str()));
    }

    if (iFlags & MDB_RDONLY) {
        return true;
    }

    return false;
}

void BlockchainLMDB::fixUp()
{
    mLogger(TRACE, BRIGHT_CYAN) << "BlockchainLMDB::" << __func__;
    BlockchainDB::fixUp();
}

void BlockchainLMDB::resetStats() { }

void BlockchainLMDB::showStats() { }
}
