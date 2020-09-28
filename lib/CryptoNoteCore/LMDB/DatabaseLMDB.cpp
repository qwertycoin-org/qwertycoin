// Copyright (c) 2017-2018, The Masari Project
// Copyright (c) 2014-2018, The Monero Project
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

#include <cstring>  // memcpy
#include <memory>   // std::unique_ptr
#include <random>

#include <boost/current_function.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/variant/get.hpp>

#include <Common/FileMappedVector.h>
#include <Common/StringTools.h>
#include <Common/Util.h>

#include <crypto/crypto.h>
#include <crypto/hash.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/TransactionUtils.h>
#include <CryptoNoteCore/LMDB/BinaryArrayDataType.h>
#include <CryptoNoteCore/LMDB/DatabaseLMDB.h>

#include <Global/CryptoNoteConfig.h>

#include <Logging/LoggerRef.h>

#if defined(__i386) || defined(__x86_64)
#define MISALIGNED_OK 1
#endif

using namespace Common;
using namespace Crypto;
using namespace CryptoNote;

// Increase when the DB structure changes
#define VERSION 1

namespace {
#define MDBValSet(var, val) MDB_val var = {sizeof(val), (void *) &val}

template <typename T>
struct MDBValCopy: public MDB_val
{
    MDBValCopy(const T &t) : tCopy(t)
    {
        mv_size = sizeof(T);
        mv_data = &tCopy;
    }

private:
    T tCopy;
};

template <>
struct MDBValCopy<CryptoNote::blobData> : public MDB_val
{
    MDBValCopy(const CryptoNote::blobData &bD) : data(new char[bD.size()])
    {
        memcpy(data.get(), bD.data(), bD.size());
        mv_size = bD.size();
        mv_data = data.get();
    }

private:
    std::unique_ptr<char[]> data;
};

template <>
struct MDBValCopy<const char *> : public MDB_val
{
    MDBValCopy(const char *s) :
            size(strlen(s) + 1), // include the NUL, makes it easier for compares
            data(new char[size])
    {
        mv_size = size;
        mv_data = data.get();
        memcpy(mv_data, s, size);
    }

private:
    size_t size;
    std::unique_ptr<char[]> data;
};

int compareUInt64(const MDB_val *a, const MDB_val *b)
{
    const uint64_t vA = *(const uint64_t *) a->mv_data;
    const uint64_t vB = *(const uint64_t *) b->mv_data;
    return (vA < vB) ? -1 : vA > vB;
}

int compareHash32(const MDB_val *a, const MDB_val *b)
{
    uint32_t *vA = (uint32_t *) a->mv_data;
    uint32_t *vB = (uint32_t *) b->mv_data;
    for (int n = 7; n >= 0; n--) {
        if (vA[n] == vB[n]) {
            continue;
        }
        return vA[n] < vB[n] ? -1 : 1;
    }

    return 0;
}

int compareString(const MDB_val *a, const MDB_val *b)
{
    const char *vA = (const char *) a->mv_data;
    const char *vB = (const char *) b->mv_data;
    return strcmp(vA, vB);
}

/*!
 * DB Schema:
 *
 * Table           |     Key       |       Data
 * -------------------------------------------------------
 * Blocks               block ID        block blob
 * BlockHeights         block hash      block height
 * BlockInfo            block ID        {block metadata}
 *
 * Txs                  txn ID          txn blob
 * TxIndices            txn hash        {txn ID, metadata}
 * TxOutputs            txn ID          [txn amount output indices]
 *
 * OutputTxs            output ID       {txn hash, local index}
 * OutputAmounts        amount          [{amount output index, metadata}...]
 *
 * SpentKeys            input hash      -
 *
 * TxPoolMeta           txn hash        txn metadata
 * TxPoolBlob           txn hash        txn blob
 *
 * Note: where the data items are of uniform size, DUPFIXED tables have
 * been used to save space. In most of these cases, a dummy "zerokval"
 * key is used when accessing the table; the Key listed above will be
 * attached as a prefix on the Data to serve as the DUPSORT key.
 * (DUPFIXED saves 8 bytes per record.)
 *
 * The output_amounts table doesn't use a dummy key, but uses DUPSORT.
 */
const char* const LMDB_BLOCKS = "Blocks";
const char* const LMDB_BLOCK_HEIGHTS = "BlockHeights";
const char* const LMDB_BLOCK_INFO = "BlockInfo";

const char* const LMDB_TXS = "Txs";
const char* const LMDB_TX_INDICES = "TxIndices";
const char* const LMDB_TX_OUTPUTS = "TxOutputs";

const char* const LMDB_OUTPUT_TXS = "OutputTxs";
const char* const LMDB_OUTPUT_AMOUNTS = "OutputAmounts";
const char* const LMDB_SPENT_KEYS = "SpentKeys";

const char* const LMDB_TXPOOL_META = "TxPoolMeta";
const char* const LMDB_TXPOOL_BLOB = "TxPoolBlob";

const char* const LMDB_HF_STARTING_HEIGHTS = "HfStartingHeights";
const char* const LMDB_HF_VERSIONS = "HfVersions";

const char* const LMDB_PROPERTIES = "Properties";

const char zeroKey[8] = {0};
const MDB_val zeroKVal = {sizeof(zeroKey), (void *)zeroKey };

const std::string LMDBError(const std::string &errorString, int mdbRes)
{
    const std::string fullString = errorString + mdb_strerror(mdbRes);
    return fullString;
}

inline void LMDBDBOpen(MDB_txn *txn,
                       const char *name,
                       int flags,
                       MDB_dbi &dbI,
                       const std::string &errorString)
{
    if (auto res = mdb_dbi_open(txn, name, flags, &dbI))
    {
        throw (CryptoNote::DB_OPEN_FAILURE((LMDBError(errorString + " : ", res) +
                                            std::string(" - you may want to start with --db-salvage"))
                                                   .c_str()));
    }
}
} // namespace

#define CURSOR(name)                                                                    \
    if (!mCur ## name) {                                                                \
        int result = mdb_cursor_open(*mWriteTxn, m ## name, &mCur ## name);            \
        if (result) {                                                                   \
            throw DB_ERROR(LMDBError("Failed to open cursor: ", result).c_str());       \
        }                                                                               \
    }

#define RCURSOR(name)                                                                   \
    if (!mCur ## name) {                                                                \
        int result = mdb_cursor_open(mTxn, m ## name, (MDB_cursor **)&mCur ## name);    \
        if (result) {                                                                   \
            throw(DB_ERROR(LMDBError("Failed to open cursor: ", result).c_str()));      \
        }                                                                               \
        if (mCursors != &mWCursors) {                                                   \
            mTInfo->mTiFlags.mRf ## name = true;                                       \
        }                                                                               \
    } else if (mCursors != & mWCursors && !mTInfo->mTiFlags.mRf ## name) {              \
        int result = mdb_cursor_renew(mTxn, mCur ## name);                              \
        if (result) {                                                                   \
            throw(DB_ERROR(LMDBError("Failed to renew cursor: ", result).c_str()));     \
        }                                                                               \
        mTInfo->mTiFlags.mRf ## name = true;                                            \
    }

namespace CryptoNote {
typedef struct mdbBlockInfo
{
    uint64_t biHeight;
    uint64_t biTimestamp;
    uint64_t biCoins;
    uint64_t biSize;
    uint64_t biDiff;
    Crypto::Hash biHash;
} mdbBlockInfo;

typedef struct blockHeight
{
    Crypto::Hash bhHash;
    uint64_t bhHeight;
} blockHeight;

typedef struct txIndex
{
    Crypto::Hash key;
    TxDataT data;
} txIndex;

typedef struct outKey
{
    uint64_t amountIndex;
    uint64_t outputId;
    OutputDataT data;
} outKey;

typedef struct outTx
{
    uint64_t outputId;
    Crypto::Hash txHash;
    uint64_t localIndex;
} outTx;

std::atomic<uint64_t> mdbTxnSafe::numActiveTxns{0};
std::atomic_flag mdbTxnSafe::creationGate = ATOMIC_FLAG_INIT;

mdbThreadInfo::~mdbThreadInfo()
{
    MDB_cursor **pCursor = &mTiCursors.mTxcBlocks;
    unsigned i;
    for (i = 0; i < sizeof(mdbTxnCursors) / sizeof(MDB_cursor *); i++) {
        if (pCursor[i]) {
            mdb_cursor_close(pCursor[i]);
        }
    }

    if (mTiRtxn) {
        mdb_txn_abort(mTiRtxn);
    }
}

mdbTxnSafe::mdbTxnSafe(const bool check)
        : mTxn(NULL),
          mTInfo(NULL),
          mCheck(check)
{
    if (check) {
        while (creationGate.test_and_set());
        numActiveTxns++;
        creationGate.clear();
    }
}

mdbTxnSafe::~mdbTxnSafe()
{
    if (!mCheck) {
        return;
    }

    if (mTInfo != nullptr) {
        mdb_txn_reset(mTInfo->mTiRtxn);
        memset(&mTInfo->mTiFlags, 0, sizeof(mTInfo->mTiFlags));
    } else if (mTxn != nullptr) {
        if (mBatchTxn) {
            /*!
             * TODO: Add log here
             */
        } else {
            /*!
             * Example of when this occurs: a lookup fails, so a read-only txn is
             * aborted through this destructor. However, successful read-only txns
             * ideally should have been committed when done and not end up here.
             *
             * NOTE: not sure if this is ever reached for a non-batch write
             * transaction, but it's probably not ideal if it did.
             *
             * TODO: Add log here
             */
        }
        mdb_txn_abort(mTxn);
    }
    numActiveTxns--;
}

void mdbTxnSafe::uncheck()
{
    numActiveTxns--;
    mCheck = false;
}

void mdbTxnSafe::commit(std::string message)
{
    if (message.size() == 0) {
        message = "Failed to commit a transaction to the db";
    }

    if (auto result = mdb_txn_commit(mTxn)) {
        mTxn = nullptr;
        throw(DB_ERROR(LMDBError(message + ": ", result).c_str()));
    }
    mTxn = nullptr;
}

void mdbTxnSafe::abort()
{
    if (mTxn != nullptr) {
        mdb_txn_abort(mTxn);
        mTxn = nullptr;
    } else {
        /*!
         * TODO: Add log here
         */
    }
}

uint64_t mdbTxnSafe::numActiveTx() const
{
    return numActiveTxns;
}

void mdbTxnSafe::preventNewTxns()
{
    while (creationGate.test_and_set());
}

void mdbTxnSafe::waitNoActiveTxns()
{
    while (numActiveTxns > 0);
}

void mdbTxnSafe::allowNewTxns()
{
    creationGate.clear();
}

void lmdbResized(MDB_env *env)
{
    mdbTxnSafe::preventNewTxns();

    MDB_envinfo mdbEnvInfo;

    mdb_env_info (env, &mdbEnvInfo);
    uint64_t old = mdbEnvInfo.me_mapsize;

    mdbTxnSafe::waitNoActiveTxns();

    int result = mdb_env_set_mapsize(env, 0);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to set new mapsize: ", result).c_str()));
    }

    mdb_env_info(env, &mdbEnvInfo);

    mdbTxnSafe::allowNewTxns();
}

inline int lmdbTxnBegin(MDB_env *env, MDB_txn *parent, unsigned int flags, MDB_txn **txn)
{
    int res = mdb_txn_begin(env, parent, flags, txn);
    if (res == MDB_MAP_RESIZED) {
        lmdbResized(env);
        res = mdb_txn_begin(env, parent, flags, txn);
    }
    return res;
}

inline int lmdbTxnRenew(MDB_txn *txn)
{
    int res = mdb_txn_renew(txn);
    if (res == MDB_MAP_RESIZED) {
        lmdbResized(mdb_txn_env(txn));
        res = mdb_txn_renew(txn);
    }
    return res;
}

void BlockchainLMDB::doResize(uint64_t sizeIncrease)
{
    std::lock_guard<std::recursive_mutex> lockGuard(mSynchronizationLock);

    const uint64_t addSize = 64 * Constants::MEGABYTE;

    //check disk capacity
    try {
        boost::filesystem::path path(mFolder);
        boost::filesystem::space_info spaceInfo = boost::filesystem::space(path);
        if (spaceInfo.available < addSize) {
            /*!
             * TODO: Add log here
             *
             * !! WARNING: Insufficient free space to extend database !!:
             */
            return;
        }
    } catch (...) {
        /*!
         * TODO: Add log here
         *
         * print something but proceed.
         *
         * => Unable to query free disk space.
         */
    }

    MDB_envinfo mdbEnvinfo;
    mdb_env_info(mEnv, &mdbEnvinfo);

    MDB_stat mdbStat;
    mdb_env_stat(mEnv, &mdbStat);

    // add 64 MB per resize, instead of doing a percentage increase
    uint64_t newMapsize = (double)mdbEnvinfo.me_mapsize + addSize;

    /*!
     * If given, use increase_size instead of above way of resizing.
     * This is currently used for increasing by an estimated size at start of new
     * batch txn.
     */
    if (sizeIncrease > 0) {
        newMapsize = mdbEnvinfo.me_mapsize + sizeIncrease;
    }

    newMapsize += (newMapsize % mdbStat.ms_psize);

    mdbTxnSafe::preventNewTxns();

    if (mWriteTxn != nullptr) {
        if (mBatchActive) {
            throw(DB_ERROR("lmdb resizing not yet supported when batch transactions enabled!"));
        } else {
            throw(DB_ERROR("attempting resize with write transaction in progress, this should not happen!"));
        }
    }

    mdbTxnSafe::waitNoActiveTxns();

    int result = mdb_env_set_mapsize(mEnv, newMapsize);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to set new mapsize: ", result).c_str()));
    }

    mdbTxnSafe::allowNewTxns();
}

void BlockchainLMDB::addBlock(const CryptoNote::Block &block,
                              const size_t &blockSize,
                              const uint64_t &cumulativeDifficulty,
                              const uint64_t &coinsGenerated,
                              const Crypto::Hash &blockHash)
{
    checkOpen();

    {
        mdbTxnCursors *mCursors = &mWCursors;
        uint64_t mHeight = height();

        CURSOR(BlockHeights);
        blockHeight bH = {blockHash, mHeight};
        MDBValSet(valH, bH);
        if (mdb_cursor_get(mCurBlockHeights, (MDB_val *)&zeroKVal, &valH, MDB_GET_BOTH) == 0) {

        }

        if (mHeight > 0) {
            MDBValSet(parentKey, block.previousBlockHash);
            int result = mdb_cursor_get(mCurBlockHeights, (MDB_val *)&zeroKVal, &parentKey, MDB_GET_BOTH);
            if (result) {
                std::cout << "mHeight: " << mHeight << std::endl;
                std::cout << "parentKey: " << block.previousBlockHash << std::endl;
                throw(DB_ERROR(LMDBError("Failed to get top block hash to check for new block's parent: ",
                                         result).c_str()));
            }
            blockHeight *prev = (blockHeight * )parentKey.mv_data;
            if (prev->bhHeight != mHeight -1) {
                throw(BLOCK_PARENT_DNE("Top block is not new block's parent"));
            }
        }
        int result = 0;

        MDBValSet(key, mHeight);

        CURSOR(Blocks)
        CURSOR(BlockInfo)

        // this call to mdb_cursor_put will change height()
        MDBValCopy<blobData> blob(blockToBlob(block));
        result = mdb_cursor_put(mCurBlocks, &key, &blob, MDB_APPEND);
        if (result) {
            throw (DB_ERROR(LMDBError("Failed to add block to db transaction: ", result).c_str()));
        }

        mdbBlockInfo bI;
        bI.biHeight         = mHeight;
        bI.biTimestamp      = block.timestamp;
        bI.biCoins          = coinsGenerated;
        bI.biSize           = blockSize;
        bI.biDiff           = cumulativeDifficulty;
        bI.biHash           = blockHash;

        MDBValSet(val, bI);
        result = mdb_cursor_put(mCurBlockInfo, (MDB_val *)&zeroKVal, &val, MDB_APPENDDUP);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to add block info into db transaction: ", result).c_str()));
        }

        result = mdb_cursor_put(mCurBlockHeights, (MDB_val *)&zeroKVal, &valH, 0);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to add block height by hash to db transaction: ", result).c_str()));
        }

        mCumSize += blockSize;
        mCumCount++;
    }
}

void BlockchainLMDB::removeBlock()
{
    int result;

    checkOpen();
    uint64_t mHeight = height();

    if (mHeight < 1) {
        throw(BLOCK_DNE("Attempting to remove block from an empty blockchain"));
        return;
    }

    mdbTxnCursors *mCursors = &mWCursors;
    CURSOR(BlockInfo)
    CURSOR(BlockHeights)
    CURSOR(Blocks)
    MDBValCopy<uint64_t> k(mHeight - 1);
    MDB_val h = k;
    if ((result = mdb_cursor_get(mCurBlockInfo, (MDB_val *)&zeroKVal, &h, MDB_GET_BOTH))) {
        /*!
         * TODO: Add log here
         */
    }

    // have to use h now; deleting from mBlockInfo will invalidate it
    mdbBlockInfo *bI = (mdbBlockInfo *)h.mv_data;
    blockHeight bH = {bI->biHash, 0};
    h.mv_data = (void *)&bH;
    h.mv_size = sizeof(bH);
    if ((result = mdb_cursor_get(mCurBlockHeights, (MDB_val *)&zeroKVal, &h, MDB_GET_BOTH))) {
        /*!
         * TODO: Add log here
         */
    }

    if ((result = mdb_cursor_del(mCurBlockHeights, 0))) {
        /*!
         * TODO: Add log here
         */
    }

    if ((result = mdb_cursor_get(mCurBlocks, &k, NULL, MDB_SET))) {
        /*!
         * TODO: Add log here
         */
    }

    if ((result = mdb_cursor_del(mCurBlocks, 0))) {
        /*!
         * TODO: Add log here
         */
    }

    if ((result = mdb_cursor_del(mCurBlockInfo, 0))) {
        /*!
         * TODO: Add log here
         */
    }
}

uint64_t BlockchainLMDB::addTransactionData(const Crypto::Hash &blockHash,
                                            const CryptoNote::Transaction &tx,
                                            const Crypto::Hash &txHash)
{
    checkOpen();
    mdbTxnCursors *mCursors = &mWCursors;
    uint64_t mHeight = height();

    int result;
    uint64_t txId = getTxCount();

    CURSOR(Txs)
    CURSOR(TxIndices)

    MDBValSet(valTxId, txId);
    MDBValSet(valH, txHash);
    result = mdb_cursor_get(mCurTxIndices, (MDB_val *)&zeroKVal, &valH, MDB_GET_BOTH);
    if (result == 0) {
        txIndex *tip = (txIndex *)valH.mv_data;
        {}
    } else if (result != MDB_NOTFOUND) {
        {}
    }

    txIndex tI;
    tI.key = txHash;
    tI.data.txId = txId;
    tI.data.unlockTime = tx.unlockTime;
    tI.data.blockId = mHeight; // we dont need blockHash since we know mHeight

    valH.mv_size = sizeof(tI);
    valH.mv_data = (void *)&tI;

    result = mdb_cursor_put(mCurTxIndices, (MDB_val *)&zeroKVal, &valH, 0);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to add tx data to db transaction: ", result).c_str()));
    }

    MDBValCopy<blobData> blob(txToBlob(tx));
    result = mdb_cursor_put(mCurTxs, &valTxId, &blob, MDB_APPEND);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to add tx blob to db transaction: ", result).c_str()));
    }

    return txId;
}

/*!
 * TODO: compare pros and cons of looking up the tx hashs tx index once and
 *       passing it in to functions like this
 */
void BlockchainLMDB::removeTransactionData(const Crypto::Hash &txHash,
                                           const CryptoNote::Transaction &tx)
{
    int result;

    checkOpen();

    {
        mdbTxnCursors *mCursors = &mWCursors;

        CURSOR(TxIndices)
        CURSOR(Txs)
        CURSOR(TxOutputs)

        MDBValSet(valH, txHash);

        if (mdb_cursor_get(mCurTxIndices, (MDB_val *)&zeroKVal, &valH, MDB_GET_BOTH)) {
            {}
        }
        txIndex *tip = (txIndex *)valH.mv_data;
        MDBValSet(valTxId, tip->data.txId);

        if ((result = mdb_cursor_get(mCurTxs, &valTxId, NULL, MDB_SET))) {
            {}
        }

        result = mdb_cursor_del(mCurTxs, 0);
        if (result) {
            {}
        }

        removeTxOutputs(tip->data.txId, tx);

        result = mdb_cursor_get(mCurTxOutputs, &valTxId, NULL, MDB_SET);
        if (!result) {
            result = mdb_cursor_del(mCurTxOutputs, 0);
        }

        // Dont delete the txIndices entry until the end, after we're done with valTxId
        if (mdb_cursor_del(mCurTxIndices, 0)) {
            throw(DB_ERROR("Failed to add removal of tx index to db transaction"));
        }
    }
}

uint64_t BlockchainLMDB::addOutput(const Crypto::Hash &txHash,
                                   const CryptoNote::TransactionOutput &txOutput,
                                   const uint64_t &localIndex,
                                   const uint64_t &unlockTime)
{
    checkOpen();

    mdbTxnCursors *mCursors = &mWCursors;
    uint64_t mHeight = height();
    uint64_t mNumOutputs = numOutputs();

    int result = 0;

    CURSOR(OutputTxs)
    CURSOR(OutputAmounts)

    outTx oT = {mNumOutputs, txHash, localIndex};
    MDBValSet(voT, oT);

    result = mdb_cursor_put(mCurOutputTxs, (MDB_val *)&zeroKVal, &voT, MDB_APPENDDUP);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to add output tx hash to db transaction: ", result).c_str()));
    }

    outKey oK;
    MDB_val data;
    MDBValCopy<uint64_t> valAmount(txOutput.amount);

    result = mdb_cursor_get(mCurOutputAmounts, &valAmount, &data, MDB_SET);
    if (!result) {
        {
            mdb_size_t numElems = 0;
            result = mdb_cursor_count(mCurOutputAmounts, &numElems);
            if (result) {
                throw(DB_ERROR(
                        std::string("Failed to get number of outputs for amount: ")
                                .append(mdb_strerror(result)).c_str()));
            }
            oK.amountIndex = numElems;
        }
    }
    else if (result != MDB_NOTFOUND) {
        throw(DB_ERROR(LMDBError("Failed to get output amount in db transaction: ", result).c_str()));
    }
    else {
        oK.amountIndex = 0;
    }

    if (txOutput.target.type() == typeid(KeyOutput)) {
        oK.data.publicKey = boost::get<KeyOutput>(txOutput.target).key;
    } else if (txOutput.target.type() == typeid (MultisignatureOutput)) {
        throw(DB_ERROR(std::string("Transaction output should be of type KeyOutput!").c_str()));
    }

    oK.outputId = mNumOutputs;
    oK.data.unlockTime = unlockTime;
    oK.data.height = mHeight;
    data.mv_size = sizeof(outKey);
    data.mv_data = &oK;

    if ((result = mdb_cursor_put(mCurOutputAmounts, &valAmount, &data, MDB_APPENDDUP))) {
        throw(DB_ERROR(LMDBError("Failed to add output pubkey to db transaction: ", result).c_str()));
    }

    return oK.amountIndex;
}

void BlockchainLMDB::addTxAmountOutputIndices(const uint64_t txId,
                                              const std::vector<uint64_t> &amountOutputIndices)
{
    checkOpen();

    mdbTxnCursors *mCursors = &mWCursors;
    CURSOR(TxOutputs)

    int result = 0;

    int numOutputs = amountOutputIndices.size();

    MDBValSet(kTxId, txId);
    MDB_val v;
    v.mv_data = (void *)amountOutputIndices.data();
    v.mv_size = sizeof(uint64_t) * numOutputs;

    result = mdb_cursor_put(mCurTxOutputs, &kTxId, &v, MDB_APPEND);
    if (result) {
        throw(DB_ERROR(std::string("Failed to add <tx hash, amount output index array> to db transaction: ")
                               .append(mdb_strerror(result)).c_str()));
    }
}

void BlockchainLMDB::removeTxOutputs(const uint64_t txId,
                                     const CryptoNote::Transaction &tx)
{
    std::vector<uint64_t> amountOutputIndices = getTxAmountOutputIndices(txId);

    if (amountOutputIndices.empty()) {
        if (tx.outputs.empty()) {

        } else {
            throw(DB_ERROR("tx has outputs, but no output indices found"));
        }
    }

    for (size_t i = tx.outputs.size(); i-- > 0;) {
        uint64_t amount = tx.outputs[i].amount;
        removeOutput(amount, amountOutputIndices[i]);
    }
}

void BlockchainLMDB::removeOutput(const uint64_t amount, const uint64_t &outIndex)
{
    checkOpen();

    mdbTxnCursors *mCursors = &mWCursors;

    CURSOR(OutputAmounts)
    CURSOR(OutputTxs)

    MDBValSet(k, amount);
    MDBValSet(v, outIndex);

    auto result = mdb_cursor_get(mCurOutputAmounts, &k, &v, MDB_GET_BOTH);
    if (result) {
        throw(DB_ERROR(LMDBError("DB error attempting to get an output", result).c_str()));
    }

    const outKey *oK = (const outKey *)v.mv_data;
    MDBValSet(oTxK, oK->outputId);
    result = mdb_cursor_get(mCurOutputTxs, (MDB_val *)&zeroKVal, &oTxK, MDB_GET_BOTH);
    if (result == MDB_NOTFOUND) {
        throw(DB_ERROR("Unexpected: global output index not found in m_output_txs"));
    } else if (result) {
        throw(DB_ERROR(LMDBError("Error adding removal of output tx to db transaction", result).c_str()));
    }

    result = mdb_cursor_del(mCurOutputTxs, 0);
    if (result) {
        throw(DB_ERROR(LMDBError(std::string("Error deleting output index ")
                                         .append(boost::lexical_cast<std::string>(outIndex)
                                                         .append(": ")).c_str(), result).c_str()));
    }

    /*!
     * now delete the amount
     */
    result = mdb_cursor_del(mCurOutputAmounts, 0);
    if (result) {
        throw(DB_ERROR(LMDBError(std::string("Error deleting amount for output index ")
                                         .append(boost::lexical_cast<std::string>(outIndex)
                                                         .append(": ")).c_str(), result).c_str()));
    }
}

void BlockchainLMDB::addSpentKey(const Crypto::KeyImage &kImage)
{
    checkOpen();

    mdbTxnCursors *mCursors = &mWCursors;

    CURSOR(SpentKeys);

    MDB_val k = {sizeof(kImage), (void *)&kImage};
    if (auto result = mdb_cursor_put(mCurSpentKeys, (MDB_val *)&zeroKVal, &k, MDB_NODUPDATA)) {
        if (result == MDB_KEYEXIST) {

        } else {

        }
    }
}

void BlockchainLMDB::removeSpentKey(const Crypto::KeyImage &kImage)
{
    checkOpen();
    mdbTxnCursors *mCursors = &mWCursors;

    CURSOR(SpentKeys)

    MDB_val k = {sizeof(kImage), (void *)&kImage};
    auto result = mdb_cursor_get(mCurSpentKeys, (MDB_val *)&zeroKVal, &k, MDB_GET_BOTH);
    if (result != 0 && result != MDB_NOTFOUND) {
        throw(DB_ERROR(LMDBError("Error finding spent key to remove", result).c_str()));
    }

    if (!result) {
        result = mdb_cursor_del(mCurSpentKeys, 0);
        if (result) {
            throw(DB_ERROR(LMDBError("Error adding removal of key image to db transaction", result).c_str()));
        }
    }
}

CryptoNote::blobData BlockchainLMDB::outputToBlob(const CryptoNote::TransactionOutput &output) const
{
    CryptoNote::blobData b;
    if (!CryptoNote::Serial::TSerializableObjectToBlob(output, b)) {
        throw(DB_ERROR("Error serializing output to blob"));
    }

    return b;
}

CryptoNote::TransactionOutput BlockchainLMDB::outputFromBlob(const CryptoNote::blobData &blob)
{
    std::stringstream ss;
    ss << blob;
    BinaryArchive<false> ba(ss);
    TransactionOutput tO;
    if (!Serial::serialize(ba, tO)) {
        throw(DB_ERROR("Error deserializing tx output blob"));
    }

    return tO;
}

void BlockchainLMDB::checkOpen() const
{
    if (!mOpen) {
        throw DB_ERROR("DB operation attempted on a not-open DB instance");
    }
}

BlockchainLMDB::~BlockchainLMDB()
{
    if (mBatchActive) {
        try {
            batchAbort();
        } catch (...) {

        }
    }

    if (mOpen) {
        close();
    }
}

BlockchainLMDB::BlockchainLMDB(bool batchTransactions): BlockchainDB()
{
    mFolder = "thishsouldnotexistbecauseitisgibberish";

    mBatchTransactions = batchTransactions;
    mWriteTxn = nullptr;
    mWriteBatchTxn = nullptr;
    mBatchActive = false;
    mCumSize = 0;
    mCumCount = 0;

    mHardfork = nullptr;
}

void BlockchainLMDB::open(const std::string &filename, const int dbFlags)
{
    int result;
    int mDbFlags = MDB_NORDAHEAD;

    if (mOpen) {
        throw DB_OPEN_FAILURE("Attempted to open db, but it's already open");
    }

    boost::filesystem::path direc(filename);
    if (boost::filesystem::exists(direc)) {
        if (!boost::filesystem::is_directory(direc)) {
            throw DB_OPEN_FAILURE("LMDB needs a directory path, but a file was passed");
        }
    } else {
        if (!boost::filesystem::create_directories(direc)) {
            throw DB_OPEN_FAILURE(std::string("Failed to create directory ").append(filename).c_str());
        }
    }

    /*!
     * check for existing LMDB files in base directory
     */
    boost::filesystem::path oldFiles = direc.parent_path();
    if (boost::filesystem::exists(oldFiles / parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME) ||
        boost::filesystem::exists(oldFiles / parameters::CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME)) {
        throw DB_ERROR("Database could not be opened");
    }

    mFolder = filename;

#ifdef __OpenBSD__
    if ((mDbFlags & MDB_WRITEMAP) == 0) {
            mDbFlags |= MDB_WRITEMAP;
        }
#endif
    /*!
     * set the LMDB environment
     */
    if ((result = mdb_env_create(&mEnv))) {
        throw DB_ERROR(LMDBError("Failed to create lmdb environment: ", result).c_str());
    }

    if ((result = mdb_env_set_maxdbs(mEnv, 20))) {
        throw DB_ERROR(LMDBError("Failed to set max number of dbs: ", result).c_str());
    }

    int threads = 4;
    /*!
     * maxreaders default is 126, leave some slots for other read processes
     */
    if (threads > 110 &&
        (result = mdb_env_set_maxreaders(mEnv, threads + 16))) {
        throw DB_ERROR(LMDBError("Failed to set max number of readers: ", result).c_str());
    }

    size_t mapsize = (size_t)1 << LMDB::SHIFTING_VAL;

    if (dbFlags & DBF_FAST) {
        mDbFlags |= MDB_NOSYNC;
    }
    if (dbFlags & DBF_FASTEST) {
        mDbFlags |= MDB_NOSYNC | MDB_WRITEMAP | MDB_MAPASYNC;
    }
    if (dbFlags & DBF_RDONLY) {
        mDbFlags = MDB_RDONLY;
    }
    if (dbFlags & DBF_SALVAGE) {
        mDbFlags |= MDB_PREVSNAPSHOT;
    }

    if (auto result = mdb_env_open(mEnv, filename.c_str(), mDbFlags, 0644)) {
        throw DB_ERROR(LMDBError("Failed to open lmdb environment: ", result).c_str());
    }

    MDB_envinfo mEI;
    mdb_env_info(mEnv, &mEI);
    uint64_t curMapsize = (double)mEI.me_mapsize;

    if (curMapsize < mapsize) {
        if (auto result = mdb_env_set_mapsize(mEnv, mapsize)) {
            throw DB_ERROR(LMDBError("Failed to set max memory map size: ", result).c_str());
        }

        mdb_env_info(mEnv, &mEI);
        curMapsize = (double)mEI.me_mapsize;
    }

    int txnFlags = 0;
    if (mDbFlags & MDB_RDONLY) {
        txnFlags |= MDB_RDONLY;
    }

    /*!
     * get a read/write MDB_txn, depending on mDbFlags
     */
    mdbTxnSafe txnSafe;
    if (auto mDbRes = mdb_txn_begin(mEnv, NULL, txnFlags, txnSafe)) {
        throw DB_ERROR(LMDBError("Failed to create a transaction for the db: ", mDbRes).c_str());
    }

    /*!
     * open necessary databases and set properties as needed
     * uses macros to avoid having to change things too many places
     */
    LMDBDBOpen (txnSafe,
                LMDB_BLOCKS, MDB_INTEGERKEY | MDB_CREATE,
                mBlocks, "Failed to open db handle for mBlocks");

    LMDBDBOpen(txnSafe,
               LMDB_BLOCK_INFO, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
               mBlockInfo, "Failed to open db handle for mBlockInfo");
    LMDBDBOpen(txnSafe,
               LMDB_BLOCK_HEIGHTS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
               mBlockHeights, "Failed to open db handle for mBlockHeights");

    LMDBDBOpen(txnSafe,
               LMDB_TXS, MDB_INTEGERKEY | MDB_CREATE,
               mTxs, "Failed to open db handle for mTxs");
    LMDBDBOpen(txnSafe,
               LMDB_TX_INDICES, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
               mTxIndices, "Failed to open db handle for mTxIndices");
    LMDBDBOpen(txnSafe,
               LMDB_TX_OUTPUTS, MDB_INTEGERKEY | MDB_CREATE,
               mTxOutputs, "Failed to open db handle for mTxOutputs");

    LMDBDBOpen(txnSafe,
               LMDB_OUTPUT_TXS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
               mOutputTxs, "Failed to open db handle for mOutputTxs");
    LMDBDBOpen(txnSafe,
               LMDB_OUTPUT_AMOUNTS, MDB_INTEGERKEY | MDB_DUPSORT | MDB_DUPFIXED | MDB_CREATE,
               mOutputAmounts, "Failed to open db handle for mOutputAmounts");

    LMDBDBOpen(txnSafe,
               LMDB_SPENT_KEYS, MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
               mSpentKeys, "Failed to open db handle for mSpentKeys");

    LMDBDBOpen(txnSafe,
               LMDB_TXPOOL_META, MDB_CREATE,
               mTxPoolMeta, "Failed to open db handle for mTxPoolMeta");
    LMDBDBOpen(txnSafe,
               LMDB_TXPOOL_BLOB, MDB_CREATE,
               mTxPoolBlob, "Failed to open db handle for mTxPoolBlob");

    /*!
     * this subdb is dropped on sight, so it may not be present when we open the DB.
     * Since we use MDB_CREATE, we'll get an exception if we open read-only and it does not exist.
     * So we don't open for read-only, and also not drop below. It is not used elsewhere.
     */
    if (!(mDbFlags & MDB_RDONLY)) {
        LMDBDBOpen(txnSafe,
                   LMDB_HF_STARTING_HEIGHTS, MDB_CREATE,
                   mHfStartingHeights, "Failed to open db handle for mHfStartingHeights");
    }

    LMDBDBOpen(txnSafe,
               LMDB_HF_VERSIONS, MDB_INTEGERKEY | MDB_CREATE,
               mHfVersions, "Failed to open db handle for mHfVersions");

    LMDBDBOpen(txnSafe,
               LMDB_PROPERTIES, MDB_CREATE, mProperties, "Failed to open db handle for mProperties");

    mdb_set_dupsort(txnSafe, mSpentKeys, compareHash32);
    mdb_set_dupsort(txnSafe, mBlockHeights, compareHash32);
    mdb_set_dupsort(txnSafe, mTxIndices, compareHash32);
    mdb_set_dupsort(txnSafe, mOutputAmounts, compareUInt64);
    mdb_set_dupsort(txnSafe, mOutputTxs, compareUInt64);
    mdb_set_dupsort(txnSafe, mBlockInfo, compareUInt64);

    mdb_set_compare(txnSafe, mTxPoolMeta, compareHash32);
    mdb_set_compare(txnSafe, mTxPoolBlob, compareHash32);
    mdb_set_compare(txnSafe, mProperties, compareString);

    if (!(mDbFlags & MDB_RDONLY)) {
        result = mdb_drop(txnSafe, mHfStartingHeights, 1);
        if (result && result != MDB_NOTFOUND) {
            throw DB_ERROR(LMDBError("Failed to drop mHfStartingHeights: ", result).c_str());
        }
    }

    /*!
     * get and keep current height
     */
    MDB_stat dbStats;
    if ((result = mdb_stat(txnSafe, mBlocks, &dbStats))) {
        throw DB_ERROR(LMDBError("Failed to query mBlocks: ", result).c_str());
    }
    uint64_t mHeight = dbStats.ms_entries;

    bool compatible = true;

    MDBValCopy<const char *> k("Version");
    MDB_val v;
    auto getResult = mdb_get(txnSafe, mProperties, &k, &v);
    if (getResult == MDB_SUCCESS) {
        if (*(const uint32_t *)v.mv_data > VERSION) {
            compatible = false;
        }

#if VERSION > 0
        else if (*(const uint32_t *)v.mv_data < VERSION) {
            /*!
             * Note that there was a schema change within version 0 as well.
             * See commit e5d2680094ee15889934fe28901e4e133cda56f2 2015/07/10
             * We don't handle the old format previous to that commit.
             */
            txnSafe.commit();
            mOpen = true;
            migrate(*(const uint32_t *)v.mv_data);
            return;
        }
#endif
    } else {
        /*!
         * if not found, and the DB is non-empty, this is probably
         * an "old" version 0, which we don't handle. If the DB is
         * empty it's fine.
         */
        if (VERSION > 0 && mHeight > 0) {
            compatible = false;
        }
    }

    if (!compatible) {
        txnSafe.abort();
        mdb_env_close(mEnv);
        mOpen = false;
        return;
    }

    if (!(mDbFlags & MDB_RDONLY)) {
        /*!
         * only write version on an empty DB
         */
        if (mHeight == 0) {
            MDBValCopy<const char *> k("Version");
            MDBValCopy<uint32_t> v(VERSION);
            auto putResult = mdb_put(txnSafe, mProperties, &k, &v, 0);
            if (putResult != MDB_SUCCESS) {
                txnSafe.abort();
                mdb_env_close(mEnv);
                mOpen = false;
                return;
            }
        }
    }

    /*!
     * commit the transaction
     */
    txnSafe.commit();
    mOpen = true;
    // init should be finished here
}

void BlockchainLMDB::close()
{
    if (mBatchActive) {
        batchAbort();
    }
    this->sync();
    mTInfo.reset();

    /*!
     * FIXME: not yet thread sage! Use with care
     */
    mdb_env_close(mEnv);
    mOpen = false;
}

void BlockchainLMDB::sync()
{
    checkOpen ();

    /*!
     * Does nothing unless LMDB environment was opened with MDB_NOSYNC or in part
     * MDB_NOMETASYNC. Force flush to be synchronous.
     */
    if (auto result = mdb_env_sync(mEnv, true)) {
        throw(DB_ERROR(LMDBError("Failed to sync database: ", result).c_str()));
    }
}

void BlockchainLMDB::safeSyncMode(const bool onOff)
{
    mdb_env_set_flags (mEnv, MDB_NOSYNC|MDB_MAPASYNC, !onOff);
}

void BlockchainLMDB::reset()
{
    checkOpen ();

    mdbTxnSafe txnSafe;

    if (auto result = lmdbTxnBegin (mEnv, NULL, 0, txnSafe)) {
        throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
    }

    if (auto result = mdb_drop(txnSafe, mBlocks, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mBlocks: ", result).c_str()));
    }

    if (auto result = mdb_drop(txnSafe, mBlockInfo, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mBlockInfo: ", result).c_str()));
    }

    if (auto result = mdb_drop(txnSafe, mBlockHeights, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mBlockHeights: ", result).c_str()));
    }

    if (auto result = mdb_drop (txnSafe, mTxs, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mTxs: ", result).c_str()));
    }

    if (auto result = mdb_drop (txnSafe, mTxIndices, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mTxIndices: ", result).c_str()));
    }

    if (auto result = mdb_drop (txnSafe, mTxOutputs, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mTxOutputs: ", result).c_str()));
    }

    if (auto result = mdb_drop(txnSafe, mOutputTxs, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop mOutputTxs: ", result).c_str()));
    }

    if (auto result = mdb_drop (txnSafe, mOutputAmounts, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop m_output_amounts: ", result).c_str()));
    }

    if (auto result = mdb_drop (txnSafe, mSpentKeys, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop m_spent_keys: ", result).c_str()));
    }

    (void)mdb_drop (txnSafe, mHfStartingHeights, 0); // this one is dropped in new code

    if (auto result = mdb_drop(txnSafe, mHfVersions, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop m_hf_versions: ", result).c_str()));
    }

    if (auto result = mdb_drop (txnSafe, mProperties, 0)) {
        throw(DB_ERROR(LMDBError("Failed to drop m_properties: ", result).c_str()));
    }

    // init with current version
    MDBValCopy<const char *> k("version");
    MDBValCopy<uint32_t> v(VERSION);

    if (auto result = mdb_put(txnSafe, mProperties, &k, &v,0)) {
        throw(DB_ERROR(LMDBError("Failed to write version to database: ", result).c_str()));
    }

    txnSafe.commit ();
    mCumSize = 0;
    mCumCount = 0;
}

std::vector<std::string> BlockchainLMDB::getFilenames() const
{
    std::vector<std::string> filenames;

    boost::filesystem::path datafile(mFolder);
    datafile /= CryptoNote::parameters::CRYPTONOTE_BLOCKCHAINDATA_FILENAME;
    boost::filesystem::path lockfile(mFolder);
    lockfile /= CryptoNote::parameters::CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME;

    filenames.push_back(datafile.string());
    filenames.push_back(lockfile.string());

    return filenames;
}

std::string BlockchainLMDB::getDBName() const
{
    return std::string("LMDB");
}

// TODO: this
bool BlockchainLMDB::lock()
{
    checkOpen();

    return false;
}

// TODO: this
void BlockchainLMDB::unlock()
{
    checkOpen();
}

#define TXN_PREFIX(flags); \
        mdbTxnSafe autoTxn; \
        mdbTxnSafe* txnPtr = &autoTxn; \
        if (mBatchActive) \
            txnPtr = mWriteTxn; \
        else { \
            if (auto mdbRes = lmdbTxnBegin(mEnv, NULL, flags, autoTxn)) {\
                throw(DB_ERROR(LMDBError(std::string("Failed to create a transaction for the db in ") \
                                          +__FUNCTION__+ \
                                          ": ", mdbRes).c_str())); \
            } \
        } \

#define TXN_PREFIX_RDONLY()                             \
        MDB_txn *mTxn;                                      \
        mdbTxnCursors *mCursors;                            \
        mdbTxnSafe autoTxn;                                 \
        bool myRtxn = blockRTxnStart(&mTxn, &mCursors);     \
        if (myRtxn) autoTxn.mTInfo = mTInfo.get();          \
        else autoTxn.uncheck()

#define TXN_POSTFIX_RDONLY()

#define TXN_POSTFIX_SUCCESS()                           \
        do {                                                \
            if (! mBatchActive)                             \
                autoTxn.commit();                           \
        } while(0)

/*!
 * The below two macros are for DB access within block add/remove, whether
 * regular batch txn is in use or not. m_write_txn is used as a batch txn, even
 * if it's only within block add/remove.
 *
 * DB access functions that may be called both within block add/remove and
 * without should use these. If the function will be called ONLY within block
 * add/remove, m_write_txn alone may be used instead of these macros.
 */

#define TXN_BLOCK_PREFIX(flags);        \
        mdbTxnSafe autoTxn;                 \
        mdbTxnSafe *txnPtr = &autoTxn;      \
        if (mBatchActive || mWriteTxn) {    \
            txnPtr = mWriteTxn;             \
        } else {                            \
            if (auto mdbRes = lmdbTxnBegin(mEnv, NULL, flags, autoTxn)) {   \
                throw(DB_ERROR(LMDBError(std::string("Failed to create a transaction for the db in ")   \
                                        +__FUNCTION__+": ",                                              \
                                        mdbRes).c_str()));                                              \
            }                                                                                           \
        }                                                                                               \

#define TXN_BLOCK_POSTFIX_SUCCESS()         \
        do {                                    \
            if (!mBatchActive && !mWriteTxn) {  \
                autoTxn.commit();               \
            }                                   \
        } while (0)

void BlockchainLMDB::addTxPoolTx(const CryptoNote::Transaction &tx, const TxPoolTxMetaT &meta)
{
    checkOpen ();

    mdbTxnCursors *mCursors = &mWCursors;

    CURSOR (TxPoolMeta)
    CURSOR (TxPoolBlob)

    MDB_val k = {sizeof (tx), (void * )&tx};
    MDB_val v;

    auto result = mdb_cursor_get(mCurTxPoolMeta, &k, &v, MDB_SET);
    result = mdb_cursor_del(mCurTxPoolMeta, 0);
    v = MDB_val({sizeof (meta), (void *)&meta});
    if ((result = mdb_cursor_put(mCurTxPoolMeta, &k, &v, MDB_NODUPDATA)) != 0) {}
}

void BlockchainLMDB::updateTxPoolTx(const Crypto::Hash &txId, const TxPoolTxMetaT &meta)
{
    checkOpen ();

    mdbTxnCursors *mCursors = &mWCursors;

    CURSOR(TxPoolMeta)
    CURSOR(TxPoolBlob)

    MDB_val k = {sizeof (txId), (void *)&txId};
    MDB_val v;

    auto result = mdb_cursor_get(mCurTxPoolMeta, &k, &v, MDB_SET);
    result = mdb_cursor_del(mCurTxPoolMeta, 0);
    v = MDB_val({sizeof (meta), (void *)&meta});
    if ((result = mdb_cursor_put(mCurTxPoolMeta, &k, &v, MDB_NODUPDATA)) != 0) {}
}

uint64_t BlockchainLMDB::getTxPoolTxCount() const
{
    checkOpen ();

    int result;
    uint64_t numEntries = 0;

    TXN_PREFIX_RDONLY();

    /*!
     * No filtering, we can get the number of tx the "fast" way
     */
    MDB_stat dbStats;

    if((result = mdb_stat(mTxn, mTxPoolMeta, &dbStats))) {
        throw(DB_ERROR(LMDBError ("Failed to query mTxPoolMeta: ", result).c_str()));
    }
    numEntries = dbStats.ms_entries;

    TXN_POSTFIX_RDONLY ();

    return numEntries;
}

bool BlockchainLMDB::txPoolHasTx(const Crypto::Hash &txId) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR (TxPoolMeta)

    MDB_val k = {sizeof (txId), (void *)&txId};
    auto result = mdb_cursor_get (mCurTxPoolMeta, &k, NULL, MDB_SET);
    if (result != 0 && result != MDB_NOTFOUND) {
        throw(DB_ERROR(LMDBError ("Error finding txpool tx meta: ", result).c_str()));
    }

    TXN_POSTFIX_RDONLY ();

    return result != MDB_NOTFOUND;
}

void BlockchainLMDB::removeTxPoolTx(const Crypto::Hash &txId)
{
    checkOpen ();

    mdbTxnCursors *mCursors = &mWCursors;

    CURSOR (TxPoolMeta)
    CURSOR (TxPoolBlob)

    MDB_val k = {sizeof (txId), (void *)&txId};
    auto result = mdb_cursor_get(mCurTxPoolMeta, &k, NULL, MDB_SET);
    if (result != 0 && result != MDB_NOTFOUND) {}

    if (!result) {
        result = mdb_cursor_del (mCurTxPoolMeta, 0);
        if (result) {}
    }

    result = mdb_cursor_get(mCurTxPoolMeta, &k, NULL, MDB_SET);
    if (result != 0 && result != MDB_NOTFOUND) {}

    if (!result) {
        result = mdb_cursor_del(mCurTxPoolBlob, 0);
        if (result) {}
    }
}

bool BlockchainLMDB::getTxPoolTxMeta(const Crypto::Hash &txId, TxPoolTxMetaT &meta) const
{
    checkOpen();

    TXN_PREFIX_RDONLY();
    RCURSOR (TxPoolMeta)

    MDB_val k = {sizeof (txId), (void *)&txId};
    MDB_val v;

    auto result = mdb_cursor_get (mCurTxPoolMeta, &k, &v, MDB_SET);
    if (result == MDB_NOTFOUND) {
        return false;
    }

    if (result != 0) {}

    meta = *(const TxPoolTxMetaT *)v.mv_data;
    TXN_POSTFIX_RDONLY();

    return true;
}

bool BlockchainLMDB::getTxPoolTxBlob(const Crypto::Hash &txId, CryptoNote::blobData &bD) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR(TxPoolBlob)

    MDB_val k = {sizeof (txId), (void *)&txId};
    MDB_val v;

    auto result = mdb_cursor_get(mCurTxPoolBlob, &k, &v, MDB_SET);
    if (result) {
        return false;
    }

    if (result != 0) {}

    bD.assign(reinterpret_cast<const char*>(v.mv_data), v.mv_size);
    TXN_POSTFIX_RDONLY();

    return true;
}

CryptoNote::blobData BlockchainLMDB::getTxPoolTxBlob(const Crypto::Hash &txId) const
{
    CryptoNote::blobData bd;
    if (!getTxPoolTxBlob (txId, bd)) {}
    return bd;
}

bool BlockchainLMDB::forAllTxPoolTxes(std::function<bool(const Crypto::Hash &,
                                                         const TxPoolTxMetaT &,
                                                         const CryptoNote::blobData *)> f,
                                      bool includeBlob,
                                      bool includeUnrelayedTxes) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (TxPoolMeta)
    RCURSOR (TxPoolBlob)

    MDB_val k;
    MDB_val v;
    bool ret = true;

    MDB_cursor_op op = MDB_FIRST;

    while (1) {
        int result = mdb_cursor_get(mCurTxPoolMeta, &k ,&v, op);
        op = MDB_NEXT;
        if (result == MDB_NOTFOUND) {
            break;
        }

        if (result) {
            throw(DB_ERROR(LMDBError ("Failed to enumerate txpool tx metadata: ", result).c_str()));
        }

        const Crypto::Hash txid = *(const Crypto::Hash*)k.mv_data;
        const TxPoolTxMetaT &meta = *(const TxPoolTxMetaT *)v.mv_data;
        const CryptoNote::blobData* passedBD = NULL;
        CryptoNote::blobData bD;
        if (includeBlob) {
            MDB_val b;
            result = mdb_cursor_get (mCurTxPoolBlob, &k, &b, MDB_SET);
            if (result == MDB_NOTFOUND) {
                throw(DB_ERROR("Failed to find txpool tx blob to match metadata"));
            }

            if (result) {
                throw(DB_ERROR(LMDBError ("Failed to enumerate txpool tx blob: ", result).c_str()));
            }

            bD.assign(reinterpret_cast<const char *>(b.mv_data), b.mv_size);
            passedBD = &bD;
        }

        if (!f(txid, meta, passedBD)) {
            ret = false;
            break;
        }
    }

    TXN_POSTFIX_RDONLY();

    return ret;
}

bool BlockchainLMDB::blockExists(const Crypto::Hash &h, uint64_t *height) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (BlockHeights)

    bool ret = false;
    MDBValSet (key, h);
    auto getResult = mdb_cursor_get(mCurBlockHeights, (MDB_val *)&zeroKVal, &key, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {

    } else if (getResult) {
        throw (DB_ERROR(LMDBError ("DB error attempting to fetch block index from hash", getResult).c_str()));
    } else {
        if (height) {
            const blockHeight *bHp = (const blockHeight *)key.mv_data;
            *height = bHp->bhHeight;
        }

        ret = true;
    }

    TXN_POSTFIX_RDONLY ();

    return ret;
}

CryptoNote::blobData BlockchainLMDB::getBlockBlob(const Crypto::Hash &h) const
{
    checkOpen ();
    CryptoNote::blobData bd = getBlockBlobFromHeight (getBlockHeight (h));
    return bd;
}

uint64_t BlockchainLMDB::getBlockHeight(const Crypto::Hash &h) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR (BlockHeights)

    MDBValSet (key, h);
    auto getResult = mdb_cursor_get (mCurBlockHeights, (MDB_val *)&zeroKVal, &key, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {}
    else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block height from the db"));
    }

    blockHeight *bHp = (blockHeight *)key.mv_data;
    uint64_t ret = bHp->bhHeight;
    TXN_POSTFIX_RDONLY();

    return ret;
}

CryptoNote::BlockHeader BlockchainLMDB::getBlockHeader(const Crypto::Hash &h) const
{
    checkOpen();

    return getBlock (h);
}

CryptoNote::blobData BlockchainLMDB::getBlockBlobFromHeight(const uint64_t &height) const
{
    checkOpen();

    TXN_PREFIX_RDONLY ();
    RCURSOR (Blocks);

    MDBValCopy<uint64_t> key(height);
    MDB_val result;
    auto getResult = mdb_cursor_get (mCurBlocks, &key, &result, MDB_SET);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get block from height ")
                                .append(boost::lexical_cast<std::string>(height))
                                .append(" failed -- block not in db").c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block from the db"));
    }

    CryptoNote::blobData bd;
    bd.assign(reinterpret_cast<char*>(result.mv_data), result.mv_size);

    TXN_POSTFIX_RDONLY();

    return bd;
}

uint64_t BlockchainLMDB::getBlockTimestamp(const uint64_t &height) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR (BlockInfo);

    MDBValSet (result, height);
    auto getResult = mdb_cursor_get(mCurBlockInfo, (MDB_val *)&zeroKVal, &result, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw (BLOCK_DNE(std::string("Attempt to get timestamp from height ")
                                 .append(boost::lexical_cast<std::string>(height))
                                 .append(" failed -- timestamp not in db").c_str()));
    } else if (getResult) {
        throw (DB_ERROR("Error attempting to retrieve a timestamp from the db"));
    }

    mdbBlockInfo *bI = (mdbBlockInfo *)result.mv_data;
    uint64_t ret = bI->biTimestamp;
    TXN_POSTFIX_RDONLY();

    return ret;
}

uint64_t BlockchainLMDB::getTopBlockTimestamp() const
{
    checkOpen ();
    uint64_t mHeight = height();

    if (mHeight < 1) {
        return 0;
    }

    return getBlockTimestamp (mHeight - 1);
}

size_t BlockchainLMDB::getBlockSize(const uint64_t &height) const
{
    checkOpen();

    TXN_PREFIX_RDONLY ();
    RCURSOR(BlockInfo);

    MDBValSet (result, height);
    auto getResult = mdb_cursor_get(mCurBlockInfo, (MDB_val *)&zeroKVal, &result, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw (BLOCK_DNE(std::string("Attempt to get block size from height ")
                                 .append(boost::lexical_cast<std::string>(height))
                                 .append(" failed -- block size not in DB").c_str()));
    } else if (getResult) {
        throw (DB_ERROR("Error attempting to retrieve a block size from the db"));
    }

    mdbBlockInfo *bI = (mdbBlockInfo *)result.mv_data;
    size_t ret = bI->biSize;
    TXN_POSTFIX_RDONLY ();

    return ret;
}

uint64_t BlockchainLMDB::getBlockCumulativeDifficulty(const uint64_t &height) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR (BlockInfo);

    MDBValSet(result, height);
    auto getResult = mdb_cursor_get(mCurBlockInfo, (MDB_val *)&zeroKVal, &result, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get cumulative difficulty from height ")
                                .append(boost::lexical_cast<std::string>(height))
                                .append(" failed -- difficulty not in db").c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a cumulative difficulty from the db"));
    }

    mdbBlockInfo *bI = (mdbBlockInfo *)result.mv_data;
    uint64_t ret = bI->biDiff;
    TXN_POSTFIX_RDONLY();

    return ret;
}

uint64_t BlockchainLMDB::getBlockDifficulty(const uint64_t &height) const
{
    checkOpen ();

    uint64_t diff0 = 0;
    uint64_t diff1 = 0;

    diff0 = getBlockCumulativeDifficulty (height);

    if (height > 0) {
        diff1 = getBlockCumulativeDifficulty (height - 1);
    }

    return (diff0 - diff1);
}

uint64_t BlockchainLMDB::getBlockAlreadyGeneratedCoins(const uint64_t &height) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR (BlockInfo);

    MDBValSet (result, height);
    auto getResult = mdb_cursor_get (mCurBlockInfo, (MDB_val *)&zeroKVal, &result, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get generated coins from height ")
                                .append(boost::lexical_cast<std::string>(height))
                                .append(" failed -- block size not in db").c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a total generated coins from the db"));
    }

    mdbBlockInfo *bI = (mdbBlockInfo *)result.mv_data;
    uint64_t ret = bI->biCoins;
    TXN_POSTFIX_RDONLY();

    return ret;
}

/*
uint64_t BlockchainLMDB::getBlockAlreadyGeneratedTransactions(const uint64_t &height) const
{
    checkOpen();

    TXN_PREFIX_RDONLY();
    RCURSOR(BlockInfo);

    MDBValSet(result, height);
    auto getResult = mdb_cursor_get(mCurBlockInfo, (MDB_val *)&zeroKVal, &result, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get generated transactions from height ")
                                .append(boost::lexical_cast<std::string>(height))
                                .append(" failed -- block size not in db").c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a total generated transactions from the db"));
    }

    mdbBlockInfo *bI = (mdbBlockInfo *)result.mv_data;
    uint64_t ret = bI->biTransactions;
    TXN_POSTFIX_RDONLY();

    return ret;
}
*/

Crypto::Hash BlockchainLMDB::getBlockHashFromHeight(const uint64_t &height) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR (BlockInfo);

    MDBValSet (result, height);

    auto getResult = mdb_cursor_get(mCurBlockInfo, (MDB_val *)&zeroKVal, &result, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        throw(BLOCK_DNE(std::string("Attempt to get hash from height")
                                .append(boost::lexical_cast<std::string>(height))
                                .append(" failed -- block info not in db").c_str()));
    } else if (getResult) {
        throw(DB_ERROR("Error attempting to retrieve a block hash from the db"));
    }

    mdbBlockInfo *blockInfo = (mdbBlockInfo *)result.mv_data;
    Crypto::Hash ret = blockInfo->biHash;
    TXN_POSTFIX_RDONLY();

    return ret;
}

std::vector<CryptoNote::Block> BlockchainLMDB::getBlocksRange(const uint64_t &h1, const uint64_t &h2) const
{
    checkOpen ();
    std::vector<CryptoNote::Block> v;

    for (uint64_t height = h1; height <= h2; ++height) {
        v.push_back((getBlockFromHeight (height)));
    }

    return v;
}

std::vector<Crypto::Hash> BlockchainLMDB::getHashesRange(const uint64_t &h1, const uint64_t &h2) const
{
    checkOpen ();
    std::vector<Crypto::Hash> v;

    for (uint64_t height = h1; height <= h2; ++height) {
        v.push_back(getBlockHashFromHeight (height));
    }

    return v;
}

Crypto::Hash BlockchainLMDB::getTopBlockHash() const
{
    checkOpen ();
    uint64_t mHeight = height () - 1;

    if (mHeight > 0) {
        return getBlockHashFromHeight (mHeight);
    }

    return NULL_HASH;
}

CryptoNote::Block BlockchainLMDB::getTopBlock() const
{
    checkOpen ();
    uint64_t mHeight = height();

    CryptoNote::Block b;
    if (mHeight > 0) {
        b = getBlockFromHeight (mHeight - 1);
    }

    return b;
}

uint64_t BlockchainLMDB::height() const
{
    checkOpen ();
    TXN_PREFIX_RDONLY ();
    int result;

    /*!
     * get current height
     */
    MDB_stat dbStats;
    if ((result = mdb_stat (mTxn, mBlocks, &dbStats))) {
        throw (DB_ERROR(LMDBError ("Failed to query mBlocks: ", result).c_str()));
    }

    return dbStats.ms_entries;
}

uint64_t BlockchainLMDB::numOutputs() const
{
    checkOpen ();
    TXN_PREFIX_RDONLY();
    int result;

    /*!
     * get current height
     */
    MDB_stat dbStats;
    if ((result = mdb_stat (mTxn, mOutputTxs, &dbStats))) {
        throw(DB_ERROR(LMDBError ("Failed to query mOutputTxs: ", result).c_str()));
    }

    return dbStats.ms_entries;
}

bool BlockchainLMDB::txExists(const Crypto::Hash &h) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (TxIndices);
    RCURSOR(Txs);

    MDBValSet(key, h);
    bool txFound = false;

    auto getResult = mdb_cursor_get (mCurTxIndices, (MDB_val *)&zeroKVal, &key, MDB_GET_BOTH);
    if (getResult == 0) {
        txFound = true;
    } else if (getResult != MDB_NOTFOUND) {
        throw (DB_ERROR(LMDBError (std::string("DB error attempting to fetch transaction index from hash")
                                   + Common::podToHex (h)
                                   + ": ", getResult).c_str()));
    }

    TXN_POSTFIX_RDONLY();

    if (!txFound) {
        return false;
    }

    /*!
     * Below not needed due to above comment.
     */
    if (getResult == MDB_NOTFOUND) {
        throw (DB_ERROR(std::string("transaction with hash")
                                .append(Common::podToHex (h))
                                .append(" not found at index").c_str()));
    } else if (getResult) {
        throw (DB_ERROR(LMDBError (std::string("DB error attempting to fetch transaction ")
                                   + Common::podToHex (h)
                                   + " at index: ", getResult).c_str()));
    }

    return true;
}

bool BlockchainLMDB::txExists(const Crypto::Hash &h, uint64_t &txIndex) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (TxIndices);

    MDBValSet (v, h);

    auto getResult = mdb_cursor_get (mCurTxIndices, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
    if (!getResult) {
        CryptoNote::txIndex *tip = (CryptoNote::txIndex *)v.mv_data;
        txIndex = tip->data.txId;
    }

    TXN_POSTFIX_RDONLY();

    bool ret = false;
    if (getResult == MDB_NOTFOUND) {

    } else if (getResult) {
        throw (DB_ERROR(LMDBError ("DB error attempting to fetch transactions from hash", getResult).c_str()));
    } else {
        ret = true;
    }

    return ret;
}

uint64_t BlockchainLMDB::getTxUnlockTime(const Crypto::Hash &h) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (TxIndices);

    MDBValSet (v, h);
    auto getResult = mdb_cursor_get (mCurTxIndices, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {

    } else if (getResult) {
        throw (DB_ERROR(LMDBError ("DB error attempting to fetch tx data from hash: ", getResult).c_str()));
    }

    CryptoNote::txIndex *tip = (CryptoNote::txIndex *)v.mv_data;
    uint64_t ret = tip->data.unlockTime;
    TXN_POSTFIX_RDONLY();

    return ret;
}

bool BlockchainLMDB::getTxBlob(const Crypto::Hash &h, CryptoNote::blobData &tx) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (TxIndices);
    RCURSOR(Txs);

    MDBValSet(v, h);
    MDB_val result;
    auto getResult = mdb_cursor_get (mCurTxIndices, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
    if (getResult == 0) {
        CryptoNote::txIndex *tip = (CryptoNote::txIndex *)v.mv_data;
        MDBValSet(valTxId, tip->data.txId);
        getResult = mdb_cursor_get (mCurTxs, &valTxId, &result, MDB_SET);
    }

    if (getResult == MDB_NOTFOUND) {
        return false;
    } else if (getResult) {
        throw (DB_ERROR(LMDBError ("DB error attempting to fetch tx from hash", getResult).c_str()));
    }

    tx.assign(reinterpret_cast<char *>(result.mv_data), result.mv_size);

    TXN_POSTFIX_RDONLY();

    return true;
}

uint64_t BlockchainLMDB::getTxCount() const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    int result;

    MDB_stat dbStats;
    if ((result = mdb_stat(mTxn, mTxs, &dbStats))) {
        throw (DB_ERROR(LMDBError ("Failed to query mTxs: ", result).c_str()));
    }

    TXN_POSTFIX_RDONLY ();

    return dbStats.ms_entries;
}

std::vector<CryptoNote::Transaction> BlockchainLMDB::getTxList(const std::vector<Crypto::Hash> &hList) const
{
    checkOpen ();
    std::vector<CryptoNote::Transaction> v;

    for (auto &h: hList) {
        v.push_back (getTx(h));
    }

    return v;
}

uint64_t BlockchainLMDB::getTxBlockHeight(const Crypto::Hash &h) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR(TxIndices);

    MDBValSet(v, h);
    auto getResult = mdb_cursor_get(mCurTxIndices, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {
        {}
    } else if (getResult) {
        throw(DB_ERROR(LMDBError ("DB error attempting to fetch tx height from hash", getResult).c_str()));
    }

    CryptoNote::txIndex *tip = (CryptoNote::txIndex *)v.mv_data;
    uint64_t ret = tip->data.blockId;
    TXN_POSTFIX_RDONLY ();
    return ret;
}

uint64_t BlockchainLMDB::getNumOutputs(const uint64_t &amount) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (OutputAmounts);

    MDBValCopy<uint64_t> k(amount);
    MDB_val v;
    mdb_size_t numElems = 0;
    auto result = mdb_cursor_get (mCurOutputAmounts, &k, &v, MDB_SET);
    if (result == MDB_SUCCESS) {
        mdb_cursor_count (mCurOutputAmounts, &numElems);
    } else if (result != MDB_NOTFOUND) {
        throw (DB_ERROR("DB error attempting to get number of outputs of an amount"));
    }

    TXN_POSTFIX_RDONLY();

    return numElems;
}

/*!
 * This is a lot harder now that we've removed the outputKeys index
 */
OutputDataT BlockchainLMDB::getOutputKey(const uint32_t &globalIndex) const
{
    checkOpen ();
    TXN_PREFIX_RDONLY();
    RCURSOR (OutputTxs);
    RCURSOR (TxIndices);
    RCURSOR (Txs);

    OutputDataT oD;
    MDBValSet (v, globalIndex);
    auto getResult = mdb_cursor_get(mCurOutputTxs, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
    if (getResult) {
        throw (DB_ERROR("DB error attempting to fetch output tx hash"));
    }

    outTx *oT = (outTx *)v.mv_data;

    MDBValSet (valH, oT->txHash);
    getResult = mdb_cursor_get(mCurTxIndices, (MDB_val *)&zeroKVal, &valH, MDB_GET_BOTH);
    if (getResult) {
        throw (DB_ERROR(LMDBError (std::string("DB error attempting to fetch transaction index from hash ")
                                   + podToHex(oT->txHash)
                                   + ": ",
                                   getResult).c_str()));
    }

    txIndex *tip = (txIndex *)valH.mv_data;
    MDBValSet(valTxId, tip->data.txId);
    MDB_val result;
    getResult = mdb_cursor_get (mCurTxs, &valTxId, &result, MDB_SET);
    if (getResult) {
        throw (DB_ERROR(LMDBError ("DB error attempting to fetch tx from hash", getResult).c_str()));
    }

    CryptoNote::blobData bD;
    bD.assign(reinterpret_cast<char *>(result.mv_data), result.mv_size);

    CryptoNote::Transaction tx;
    if (!parseAndValidateTxFromBlob (bD, tx)) {
        throw (DB_ERROR("Failed to parse tx from blob retrieved from the db"));
    }

    const CryptoNote::TransactionOutput txOutput = tx.outputs[oT->localIndex];
    oD.unlockTime = tip->data.unlockTime;
    oD.height = tip->data.blockId;
    oD.publicKey = boost::get<CryptoNote::KeyOutput>(txOutput.target).key;

    TXN_POSTFIX_RDONLY();

    return oD;
}

OutputDataT BlockchainLMDB::getOutputKey(const uint64_t &amount, const uint32_t &index)
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR(OutputAmounts);

    MDBValSet (k, amount);
    MDBValSet (v, index);
    auto getResult = mdb_cursor_get (mCurOutputAmounts, &k, &v, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {

    } else if (getResult) {
        throw (DB_ERROR("Error attempting to retrieve an output pubkey from the db"));
    }

    OutputDataT ret;
    const outKey  *oKP = (const outKey *)v.mv_data;
    memcpy(&ret, &oKP->data, sizeof (OutputDataT));
    TXN_POSTFIX_RDONLY();

    return ret;
}

txOutIndex BlockchainLMDB::getOutputTxAndIndexFromGlobal(const uint64_t &index) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR(OutputTxs);

    MDBValSet (v, index);
    auto getResult = mdb_cursor_get(mCurOutputTxs, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
    if (getResult == MDB_NOTFOUND) {

    } else if (getResult) {
        throw (DB_ERROR("DB error attempting to fetch output tx hash"));
    }

    outTx *oT = (outTx *)v.mv_data;
    txOutIndex ret = txOutIndex(oT->txHash, oT->localIndex);

    TXN_POSTFIX_RDONLY();
    return ret;
}

txOutIndex BlockchainLMDB::getOutputTxAndIndex(const uint64_t &amount, const uint32_t &index) const
{
    std::vector<uint32_t> offsets;
    std::vector<txOutIndex> indices;
    offsets.push_back(index);
    getOutputTxAndIndex (amount, offsets, indices);
    if (!indices.size()) {

    }

    return indices [0];
}

std::vector<uint64_t> BlockchainLMDB::getTxAmountOutputIndices(const uint64_t txId) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR(TxOutputs);

    int result = 0;
    MDBValSet (kTxId, txId);
    MDB_val v;
    std::vector<uint64_t> amountOutputIndices;

    result = mdb_cursor_get (mCurTxOutputs, &kTxId, &v, MDB_SET);
    if (result == MDB_NOTFOUND) {

    } else if (result) {
        throw (DB_ERROR(LMDBError ("DB error attempting to get data for txOutputs[txIndex]", result).c_str()));
    }

    const uint64_t *indices = (const uint64_t *)v.mv_data;
    int numOutputs = v.mv_size / sizeof (uint64_t);

    amountOutputIndices.reserve(numOutputs);
    for (int i = 0; i < numOutputs; ++i) {
        amountOutputIndices.push_back(indices[i]);
    }
    indices = nullptr;

    TXN_POSTFIX_RDONLY();

    return amountOutputIndices;
}

bool BlockchainLMDB::hasKeyImage(const Crypto::KeyImage &kImg) const
{
    checkOpen ();

    bool ret;

    TXN_PREFIX_RDONLY();
    RCURSOR(SpentKeys);

    MDB_val k = {sizeof (kImg), (void *)&kImg};
    ret = (mdb_cursor_get (mCurSpentKeys, (MDB_val *)&zeroKVal, &k, MDB_GET_BOTH) == 0);

    TXN_POSTFIX_RDONLY ();

    return ret;
}

bool BlockchainLMDB::forAllKeyImages(std::function<bool (const Crypto::KeyImage &)> f) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (SpentKeys);

    MDB_val k, v;
    bool fRet = true;

    k = zeroKVal;
    MDB_cursor_op oP = MDB_FIRST;
    while (1) {
        int ret = mdb_cursor_get (mCurSpentKeys, &k, &v, oP);
        oP = MDB_NEXT;
        if (ret == MDB_NOTFOUND) {
            break;
        }

        if (ret < 0) {
            throw (DB_ERROR("Failed to enumerate key images"));
        }

        const Crypto::KeyImage keyImage = *(const Crypto::KeyImage *)v.mv_data;

        if (!f(keyImage)) {
            fRet = false;
            break;
        }
    }

    TXN_POSTFIX_RDONLY();

    return fRet;
}

bool BlockchainLMDB::forBlocksRange(const uint64_t &h1,
                                    const uint64_t &h2,
std::function<bool (uint64_t,
const Crypto::Hash &,
const CryptoNote::Block &)> f) const
{
checkOpen ();

TXN_PREFIX_RDONLY ();
RCURSOR (Blocks);

MDB_val k, v;
bool fRet = true;

MDB_cursor_op oP;
if (h1) {
k = MDB_val{sizeof (h1), (void *)&h1};
oP = MDB_SET;
} else {
oP = MDB_FIRST;
}

while (1) {
int ret = mdb_cursor_get(mCurBlocks, &k, &v, oP);
oP = MDB_NEXT;
if (ret == MDB_NOTFOUND) {
break;
}

if (ret) {
throw (DB_ERROR("Failed to enumerate blocks"));
}

uint64_t height = *(const uint64_t *)k.mv_data;
CryptoNote::blobData bD;
bD.assign(reinterpret_cast<char *>(v.mv_data), v.mv_size);
CryptoNote::Block bT;
bool r = parseAndValidateBlockFromBlob(bD, bT);

if (!r) {
throw (DB_ERROR("Failed to parse block from blob retrieved from the db"));
}

Crypto::Hash hash;
if (!get_block_hash (bT, hash)) {
throw (DB_ERROR("Failed to get block hash from blob retrieved from the db"));
}

if (!f(height, hash, bT)) {
fRet = false;
break;
}

if (height >= h2) {
break;
}
}

TXN_POSTFIX_RDONLY();

return fRet;
}

bool BlockchainLMDB::forAllTransactions(std::function<bool (const Crypto::Hash &,
                                                            const CryptoNote::Transaction &)> f) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY();
    RCURSOR (Txs);
    RCURSOR (TxIndices);

    MDB_val k, v;
    bool fRet = true;

    MDB_cursor_op oP = MDB_FIRST;
    while (1) {
        int ret = mdb_cursor_get (mCurTxIndices, &k, &v,  oP);
        oP = MDB_NEXT;
        if (ret == MDB_NOTFOUND) {
            break;
        }

        if (ret) {
            throw (DB_ERROR(LMDBError ("Failed to enumerate transactions: ", ret).c_str()));
        }

        txIndex *tI = (txIndex *)v.mv_data;
        const Crypto::Hash hash = tI->key;
        k.mv_data = (void *)&tI->data.txId;
        k.mv_size = sizeof (tI->data.txId);
        ret = mdb_cursor_get (mCurTxs, &k, &v, MDB_SET);
        if (ret == MDB_NOTFOUND) {
            break;
        }

        if (ret) {
            throw (DB_ERROR(LMDBError("Failed to enumerate transactions: ", ret).c_str()));
        }

        CryptoNote::Transaction tx;
        CryptoNote::blobData bD;
        bD.assign(reinterpret_cast<char *>(v.mv_data), v.mv_size);

        if (!parseAndValidateTxFromBlob(bD, tx)) {
            throw(DB_ERROR("Failed to parse tx from retrieved from db"));
        }

        if (!f(hash, tx)) {
            fRet = false;
            break;
        }
    }

    TXN_POSTFIX_RDONLY();

    return fRet;
}

bool BlockchainLMDB::forAllOutputs(std::function<bool (uint64_t amount,
const Crypto::Hash &txHash,
        uint64_t height,
size_t txIndex)> f) const
{
checkOpen ();

TXN_PREFIX_RDONLY();
RCURSOR(OutputAmounts);

MDB_val k, v;
bool fRet = true;

MDB_cursor_op oP = MDB_FIRST;
while (1) {
int ret = mdb_cursor_get(mCurOutputAmounts, &k, &v, oP);
oP = MDB_NEXT;
if (ret == MDB_NOTFOUND) {
break;
}

if (ret) {
throw(DB_ERROR("Failed to enumerate outputs"));
}

uint64_t amount = *(const uint64_t *)k.mv_data;
outKey *oK = (outKey *)v.mv_data;
txOutIndex tOI = getOutputTxAndIndexFromGlobal(oK->outputId);
if (!f(amount, tOI.first, oK->data.height, tOI.second)) {
fRet = false;
break;
}
}

TXN_POSTFIX_RDONLY();

return fRet;
}

bool BlockchainLMDB::forAllOutputs(uint64_t amount, const std::function<bool (uint64_t height)> &f) const
{
checkOpen ();

TXN_PREFIX_RDONLY ();
RCURSOR(OutputAmounts);

MDBValSet (k, amount);
MDB_val v;
bool fRet = true;

MDB_cursor_op oP = MDB_SET;
while (1) {
int ret = mdb_cursor_get(mCurOutputAmounts, &k, &v, oP);
oP = MDB_NEXT_DUP;
if (ret == MDB_NOTFOUND) {
break;
}

if (ret) {
throw (DB_ERROR("Failed to enumerate outputs"));
}

uint64_t outAmount = *(const uint64_t *)k.mv_data;
if (amount != outAmount) {
fRet = false;
break;
}

const outKey *oK = (const outKey *)v.mv_data;
if(!f(oK->data.height)) {
fRet = false;
break;
}
}

TXN_POSTFIX_RDONLY();

return fRet;
}

bool BlockchainLMDB::batchStart(uint64_t batchNumBlocks, uint64_t batchBytes)
{
if (!mBatchTransactions) {
throw(DB_ERROR("batch transactions not enabled"));
}

if (mBatchActive) {
return false;
}

if (mWriteBatchTxn != nullptr) {
return false;
}

if (mWriteTxn) {
throw(DB_ERROR("batch transaction attempted, but mWriteTxn already in use"));
}

checkOpen ();

mWriter = boost::this_thread::get_id ();

mWriteBatchTxn = new mdbTxnSafe();
/*!
 * NOTE: need to make sure it's destroyed properly when done
 */
if (auto mdbRes = lmdbTxnBegin (mEnv, NULL, 0, *mWriteBatchTxn)) {
delete mWriteBatchTxn;
mWriteBatchTxn = nullptr;
throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", mdbRes).c_str()));
}

/*!
 * indicates this transaction is for batch transactions, but not whether it's
 * active
 */
mWriteBatchTxn->mBatchTxn = true;
mWriteTxn = mWriteBatchTxn;

mBatchActive = true;
memset (&mWCursors, 0, sizeof (mWCursors));
if (mTInfo.get ()) {
if (mTInfo->mTiFlags.mRfTxn) {
mdb_txn_reset (mTInfo->mTiRtxn);
}
memset(&mTInfo->mTiFlags, 0, sizeof (mTInfo->mTiFlags));
}

return true;
}

void BlockchainLMDB::batchCommit()
{
    if (!mBatchTransactions) {
        throw(DB_ERROR("batch transactions not enabled"));
    }

    if (!mBatchActive) {
        throw(DB_ERROR("batch transaction not in progress"));
    }

    if (mWriteBatchTxn == nullptr) {
        throw(DB_ERROR("batch transaction not in progress"));
    }

    if (mWriter != boost::this_thread::get_id ()) {
        throw(DB_ERROR("batch transaction owned by other thread"));
    }

    checkOpen ();
    mWriteTxn->commit ();

    mWriteTxn = nullptr;
    delete mWriteBatchTxn;
    mWriteBatchTxn = nullptr;
    memset(&mWCursors, 0, sizeof (mWCursors));
}

void BlockchainLMDB::cleanupBatch()
{
    /*!
     * for destruction of batch transaction
     */
    mWriteTxn = nullptr;
    delete mWriteBatchTxn;
    mWriteBatchTxn = nullptr;
    mBatchActive = false;
    memset (&mWCursors, 0, sizeof (mWCursors));
}

void BlockchainLMDB::batchStop()
{
    if (!mBatchTransactions) {
        throw(DB_ERROR("batch transactions not enabled"));
    }

    if (!mBatchActive) {}
    if (mWriteBatchTxn == nullptr) {}
    if (mWriter != boost::this_thread::get_id ()) {}

    checkOpen ();

    try {
        mWriteTxn->commit ();
        cleanupBatch ();
    } catch (const std::exception &e) {
        cleanupBatch();
        throw;
    }
}

void BlockchainLMDB::batchAbort()
{
    if (!mBatchTransactions) {
        throw(DB_ERROR("batch transactions not enabled"));
    }

    if (!mBatchActive) {}
    if (mWriteBatchTxn == nullptr) {}
    if (mWriter != boost::this_thread::get_id ()) {}

    checkOpen ();
    /*!
     * for destruction of batch transaction
     */
    mWriteTxn = nullptr;

    /*!
     * explicitly call in case mdb_env_close() (BlockchainLMDB::close())
     * called before BlockchainLMDB destructor called.
     */
    mWriteBatchTxn->abort ();
    delete mWriteBatchTxn;
    mWriteBatchTxn = nullptr;
    mBatchActive = false;
    memset(&mWCursors, 0, sizeof (mWCursors));
}

void BlockchainLMDB::setBatchTransactions(bool batchTransactions)
{
    if ((batchTransactions) && (mBatchTransactions)) {}

    mBatchTransactions = batchTransactions;
}

bool BlockchainLMDB::blockRTxnStart(MDB_txn **mTxn, mdbTxnCursors **mCur) const
{
    bool ret = false;
    mdbThreadInfo *tInfo;
    if (mWriteTxn && mWriter == boost::this_thread::get_id ()) {
        *mTxn = mWriteTxn->mTxn;
        *mCur = (mdbTxnCursors *)&mWCursors;

        return ret;
    }

    /*!
     * Check for existing info and force reset if env doesn't match -
     * only happens if env was opened/closed multiple times in same process
     */
    if (!(tInfo = mTInfo.get ()) || mdb_txn_env(tInfo->mTiRtxn) != mEnv) {
        tInfo = new mdbThreadInfo;
        mTInfo.reset(tInfo);
        memset(&tInfo->mTiCursors, 0, sizeof(tInfo->mTiCursors));
        memset(&tInfo->mTiFlags, 0, sizeof(tInfo->mTiFlags));

        if (auto mdbRes = lmdbTxnBegin(mEnv, NULL, MDB_RDONLY, &tInfo->mTiRtxn)) {
            throw(DB_ERROR_TXN_START(LMDBError("Failed to create a read transaction for the db: ",
                                               mdbRes).c_str()));
        }
        ret = true;
    } else if (!tInfo->mTiFlags.mRfTxn) {
        if (auto mdbRes = lmdbTxnRenew(tInfo->mTiRtxn)) {
            throw(DB_ERROR_TXN_START(LMDBError("Failed to renew a read transaction for the db: ",
                                               mdbRes).c_str()));
        }
        ret = true;
    }

    if (ret) {
        tInfo->mTiFlags.mRfTxn = true;
    }
    *mTxn = tInfo->mTiRtxn;
    *mCur = &tInfo->mTiCursors;

    return ret;
}

void BlockchainLMDB::blockRTxnStop() const
{
    mdb_txn_reset(mTInfo->mTiRtxn);
    memset(&mTInfo->mTiFlags, 0, sizeof(mTInfo->mTiFlags));
}

void BlockchainLMDB::blockTxnStart(bool readonly)
{
    if (readonly) {
        MDB_txn *mdbTxn;
        mdbTxnCursors *mCur;
        blockRTxnStart (&mdbTxn, &mCur);
        return;
    }

    /*!
     * Distinguish the exceptions here from exceptions that would be thrown while
     * using the txn and committing it.
     *
     * If an exception is thrown in this setup, we don't want the caller to catch
     * it and proceed as if there were an existing write txn, such as trying to
     * call blockTxnAbort(). It also indicates a serious issue which will
     * probably be thrown up another layer.
     */
    if (!mBatchActive && mWriteTxn) {
        throw(DB_ERROR_TXN_START((std::string("Attempted to start new write txn "
                                              "when write txn already exists in ")
                                  +__FUNCTION__).c_str()));
    }

    if (!mBatchActive) {
        mWriter = boost::this_thread::get_id ();
        mWriteTxn = new mdbTxnSafe();
        if (auto mdbRes = lmdbTxnBegin(mEnv, NULL, 0, *mWriteTxn)) {
            delete mWriteTxn;
            mWriteTxn = nullptr;
            throw(DB_ERROR_TXN_START(LMDBError("Failed to create a transaction for the db: ",
                                               mdbRes).c_str()));
        }

        memset(&mWCursors, 0, sizeof (mWCursors));
        if (mTInfo.get()) {
            if (mTInfo->mTiFlags.mRfTxn) {
                mdb_txn_reset(mTInfo->mTiRtxn);
            }
            memset(&mTInfo->mTiFlags, 0, sizeof(mTInfo->mTiFlags));
        }
    } else if (mWriter != boost::this_thread::get_id ()) {
        throw(DB_ERROR_TXN_START((std::string("Attempted to start new write txn when batch txn already exists in ")
                                  +__FUNCTION__).c_str()));
    }
}

void BlockchainLMDB::blockTxnStop()
{
    if (mWriteTxn && mWriter == boost::this_thread::get_id ()) {
        if (!mBatchActive) {
            mWriteTxn->commit();

            delete mWriteTxn;
            mWriteTxn = nullptr;
            memset(&mWCursors, 0, sizeof (mWCursors));
        }
    } else if (mTInfo->mTiRtxn) {
        mdb_txn_reset(mTInfo->mTiRtxn);
        memset(&mTInfo->mTiFlags, 0, sizeof(mTInfo->mTiFlags));
    }
}

void BlockchainLMDB::blockTxnAbort()
{
    if (mWriteTxn && mWriter == boost::this_thread::get_id ()) {
        if (!mBatchActive) {
            delete mWriteTxn;
            mWriteTxn = nullptr;
            memset(&mWCursors, 0, sizeof (mWCursors));
        }
    } else if (mTInfo->mTiRtxn) {
        mdb_txn_reset (mTInfo->mTiRtxn);
        memset (&mTInfo->mTiFlags, 0, sizeof (mTInfo->mTiFlags));
    } else {
        /*!
         * This would probably mean an earlier exception was caught, but then we
         * proceeded further than we should have.
         */
        throw(DB_ERROR((std::string("BlockchainLMDB::") + __func__ +
                        std::string(": block-level DB transaction abort called when write txn doesn't exist"))
                               .c_str()));
    }
}

uint64_t BlockchainLMDB::addBlock(const CryptoNote::Block &block,
                                  const size_t &blockSize,
                                  const uint64_t &cumulativeDifficulty,
                                  const uint64_t &coinsGenerated,
                                  const std::vector<CryptoNote::Transaction> &txs)
{
    checkOpen ();
    uint64_t mHeight = height ();

    try {
        BlockchainDB::addBlock (block,
                                blockSize,
                                cumulativeDifficulty,
                                coinsGenerated,
                                txs);
    } catch (const DB_ERROR_TXN_START &e) {
        throw;
    } catch (...) {
        blockTxnAbort ();
        throw;
    }

    return ++mHeight;
}

void BlockchainLMDB::popBlock(CryptoNote::Block &block,
                              std::vector<CryptoNote::Transaction> &txs)
{
    checkOpen ();

    blockTxnStart (false);

    try {
        BlockchainDB::popBlock (block, txs);
        blockTxnStop ();
    } catch (...) {
        blockTxnAbort ();
        throw;
    }
}

void BlockchainLMDB::removeSpentKeys(std::vector<Crypto::KeyImage> &kImages)
{
    for (auto image : kImages) {
        removeSpentKey(image);
    }
}

void BlockchainLMDB::getOutputTxAndIndexFromGlobal(const std::vector<uint64_t> &globalIndices,
                                                   std::vector<txOutIndex> &txOutIndices) const
{
    checkOpen ();
    txOutIndices.clear();

    TXN_PREFIX_RDONLY();
    RCURSOR(OutputTxs);

    for (const uint64_t &outputId : globalIndices) {
        MDBValSet(v, outputId);

        auto getResult = mdb_cursor_get(mCurOutputTxs, (MDB_val *)&zeroKVal, &v, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {

        } else if (getResult) {
            throw(DB_ERROR("DB error attempting to fetch output tx hash"));
        }

        outTx *oT = (outTx *)v.mv_data;
        auto result = txOutIndex(oT->txHash, oT->localIndex);
        txOutIndices.push_back(result);
    }

    TXN_POSTFIX_RDONLY();
}

void BlockchainLMDB::getOutputKey(const uint64_t &amount,
                                  const std::vector<uint32_t> &offsets,
                                  std::vector<OutputDataT> &outputs,
                                  bool allowPartial)
{
    checkOpen();
    outputs.clear();

    TXN_PREFIX_RDONLY ();
    RCURSOR (OutputAmounts);

    MDBValSet(k, amount);
    for (const auto &index : offsets) {
        MDBValSet (v, index);

        auto getResult = mdb_cursor_get(mCurOutputAmounts, &k, &v, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {
            if (allowPartial) {
                break;
            }

            {}
        } else if (getResult) {
            throw(DB_ERROR(LMDBError("Error attempting to retrieve an output pubkey from the db",
                                     getResult).c_str()));

        }

        OutputDataT data;
        const outKey *oKp = (const outKey *)v.mv_data;
        memcpy(&data, &oKp->data, sizeof (OutputDataT));

        outputs.push_back(data);
    }

    TXN_POSTFIX_RDONLY();
}

void BlockchainLMDB::getOutputTxAndIndex(const uint64_t &amount,
                                         const std::vector<uint32_t> &offsets,
                                         std::vector<txOutIndex> &indices) const
{
    checkOpen();
    indices.clear();

    std::vector<uint64_t> txIndices;

    TXN_PREFIX_RDONLY ();
    RCURSOR (OutputAmounts);

    MDBValSet (k, amount);
    for (const auto &index : offsets) {
        MDBValSet(v, index);

        auto getResult = mdb_cursor_get (mCurOutputAmounts, &k, &v, MDB_GET_BOTH);
        if (getResult == MDB_NOTFOUND) {

        } else if (getResult) {
            throw(DB_ERROR(LMDBError("Error attempting to retrieve an output from the db",
                                     getResult).c_str()));
        }

        const outKey *oKp = (const outKey *)v.mv_data;
        txIndices.push_back(oKp->outputId);
    }

    if (txIndices.size() > 0) {
        getOutputTxAndIndexFromGlobal (txIndices, indices);
    }
}

std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t>>
BlockchainLMDB::getOutputHistorgram(const std::vector<uint64_t> &amounts,
                                    bool unlocked,
                                    uint64_t recentCutoff) const
{
    checkOpen ();

    TXN_PREFIX_RDONLY ();
    RCURSOR(OutputAmounts);

    std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t>> histogram;
    MDB_val k, v;

    if (amounts.empty()) {
        MDB_cursor_op oP = MDB_FIRST;
        while (1) {
            int ret = mdb_cursor_get (mCurOutputAmounts, &k, &v, oP);
            oP = MDB_NEXT_NODUP;

            if (ret == MDB_NOTFOUND) {
                break;
            }

            if (ret) {
                throw(DB_ERROR(LMDBError ("Failed to enumerate outputs: ", ret).c_str()));
            }

            mdb_size_t numElems = 0;
            mdb_cursor_count(mCurOutputAmounts, &numElems);
            uint64_t amount = *(const uint64_t*)k.mv_data;
            histogram[amount] = std::make_tuple(numElems, 0, 0);
        }
    } else {
        for (const auto &amount : amounts) {
            MDBValCopy<uint64_t> k(amount);
            int ret = mdb_cursor_get(mCurOutputAmounts, &k, &v, MDB_SET);
            if (ret == MDB_NOTFOUND) {
                histogram[amount] = std::make_tuple(0, 0, 0);
            } else if (ret == MDB_SUCCESS) {
                mdb_size_t numElems = 0;
                mdb_cursor_count(mCurOutputAmounts, &numElems);
                histogram[amount] = std::make_tuple (numElems, 0, 0);
            } else {
                throw(DB_ERROR(LMDBError("Failed to enumerate outputs: ", ret).c_str()));
            }
        }
    }

    if (unlocked || recentCutoff > 0) {
        const uint64_t blockchainHeight = height();
        for (std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t>>::iterator i = histogram.begin();
                i != histogram.end();
        ++i) {
            uint64_t amount = i->first;
            uint64_t numElems = std::get<0>(i->second);
            while (numElems > 0) {
                const txOutIndex tOI = getOutputTxAndIndex (amount, numElems - 1);
                const uint64_t height = getTxBlockHeight (tOI.first);
                if (height + CryptoNote::parameters::CRYPTONOTE_TX_SPENDABLE_AGE <= blockchainHeight) {
                    break;
                }
                --numElems;
            }
            /*!
             * modifying second does not invalidate the iterator
             */
            std::get<1>(i->second) = numElems;

            if (recentCutoff > 0) {
                uint64_t recent = 0;
                while (numElems > 0) {
                    const txOutIndex tOI = getOutputTxAndIndex (amount, numElems - 1);
                    const uint64_t height = getTxBlockHeight (tOI.first);
                    const uint64_t ts = getBlockTimestamp (height);
                    if (ts < recentCutoff) {
                        break;
                    }
                    --numElems;
                    ++recent;
                }
                /*!
                 * modifying second does not invalidate the iterator
                 */
                std::get<2>(i->second) = recent;
            }
        }
    }

    TXN_POSTFIX_RDONLY();

    return histogram;
}

void BlockchainLMDB::checkHardForkInfo()
{
}

void BlockchainLMDB::dropHardForkInfo()
{
    checkOpen ();

    TXN_PREFIX(0);

    auto result = mdb_drop(*txnPtr, mHfStartingHeights, 1);
    if (result) {}

    result = mdb_drop(*txnPtr, mHfVersions, 1);

    if (result) {}

    TXN_POSTFIX_SUCCESS ();
}

void BlockchainLMDB::setHardForkVersion(uint64_t height, uint8_t version)
{
checkOpen();

TXN_BLOCK_PREFIX(0);

MDBValCopy<uint64_t> valKey(height);
MDBValCopy<uint8_t> valValue(version);
int result;
result = mdb_put(*txnPtr, mHfVersions, &valKey, &valValue, MDB_APPEND);
if (result == MDB_KEYEXIST) {
result = mdb_put (*txnPtr, mHfVersions, &valKey, &valValue, 0);
}

if (result) {}

TXN_BLOCK_POSTFIX_SUCCESS ();
}

uint8_t BlockchainLMDB::getHardForkVersion(uint64_t height) const
{
checkOpen ();

if (height == 0) {
const uint8_t version = 1;
return version;
}

TXN_PREFIX_RDONLY ();
RCURSOR(HfVersions);

MDBValCopy<uint64_t> valKey(height);
MDB_val valRet;
auto result = mdb_cursor_get (mCurHfVersions, &valKey, &valRet, MDB_SET);

if (result == MDB_NOTFOUND || result) {
throw(DB_ERROR(LMDBError("Error attempting to retrieve a hard fork version at height "
                         + boost::lexical_cast<std::string>(height)
                         + " from the db: ",
                         result).c_str()));
}

uint8_t ret = *(const uint8_t *)valRet.mv_data;
TXN_POSTFIX_RDONLY ();

return ret;
}

bool BlockchainLMDB::isReadOnly() const
{
    unsigned int flags;
    auto result = mdb_env_get_flags (mEnv, &flags);
    if (result) {
        throw(DB_ERROR(LMDBError("Error getting database environment info: ", result).c_str()));
    }

    if (flags & MDB_RDONLY) {
        return true;
    }

    return false;
}

void BlockchainLMDB::fixup()
{
    BlockchainDB::fixup();
}

#define RENAME_DB(name)                                         \
        k.mv_data = (void *)name;                                   \
        k.mv_size = sizeof(name) - 1;                               \
        result = mdb_cursor_open(txn, 1, &cCur);                     \
        if (result) {                                               \
            throw(DB_ERROR(LMDBError("Failed to open a cursor for " \
                                     name ": ", result).c_str()));  \
        }                                                           \
        result = mdb_cursor_get(cCur, &k, NULL, MDB_SET_KEY);       \
        if (result) {                                               \
            throw(DB_ERROR(LMDBError("Failed to open a cursor for " \
                                     name ": ", result).c_str()));  \
        }                                                           \
        ptr = (char *)k.mv_data;                                    \
        ptr[sizeof(name)-2] = 's';

void BlockchainLMDB::migrate0To1()
{
    uint64_t i, mHeight, z;
    int result;
    mdbTxnSafe txn(false);
    MDB_val k, v;
    char *ptr;

    std::cout << "Migrating blockchain from DB version 0 to 1 - this may take a while:" << std::endl;

    do {
        result = mdb_txn_begin(mEnv, NULL, 0, txn);

        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }

        MDB_stat dbStats;

        if ((result = mdb_stat(txn, mBlocks, &dbStats))) {
            throw(DB_ERROR(LMDBError("Failed to query mBlocks: ", result).c_str()));
        }

        mHeight = dbStats.ms_entries;
        std::cout << "Total number of blocks: " << mHeight << std::endl;
        std::cout << "block migration will update BlockHeights, BlockInfo, and HfVersions..." << std::endl;

        MDB_dbi oHeights;

        unsigned int flags;
        result = mdb_dbi_flags(txn, mBlockHeights, &flags);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to retrieve blockHeights flags: ", result).c_str()));
        }

        /*!
         * if the flags are what we expect, this table has already been migrated
         */
        if ((flags & (MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED)) ==
            (MDB_INTEGERKEY|MDB_DUPSORT|MDB_DUPFIXED)) {
            txn.abort ();
            break;
        }

        /*!
         * the BlockHeights table name is the same but the old version and new version
         * have incompatible DB flags. Create a new table with the right flags. We want
         * the name to be similar to the old name so that it will occupy the same location
         * in the DB.
         */
        oHeights = mBlockHeights;
        LMDBDBOpen(txn,
                   "BlockHeightR",
                   MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
                   mBlockHeights,
                   "Failed to open db handle for BlockHeightR");
        mdb_set_dupsort(txn, mBlockHeights, compareHash32);

        MDB_cursor *cOld, *cCur;
        blockHeight bH;
        MDBValSet(nV, bH);

        /*!
         * old table was k(hash), v(height).
         * new table is DUPFIXED, k(zeroval), v{hash, height}.
         */
        i = 0;
        z = mHeight;
        while(1) {
            if (!(i % 2000)) {
                if (i) {
                    std::cout << i << " / " << z << "  \r" << std::flush;
                    txn.commit();
                    result = mdb_txn_begin(mEnv, NULL, 0, txn);
                    if (result) {
                        throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
                    }
                }

                result = mdb_cursor_open(txn, oHeights, &cCur);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for BlockHeightR: ", result).c_str()));
                }

                if (!i) {
                    MDB_stat ms;
                    mdb_stat (txn, mBlockHeights, &ms);
                    i = ms.ms_entries;
                }
            }

            result = mdb_cursor_get(cOld, &k, &v, MDB_NEXT);
            if (result == MDB_NOTFOUND) {
                txn.commit();
                break;
            } else if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from BlockHeights: ", result).c_str()));
            }

            bH.bhHash   = *(Crypto::Hash *)k.mv_data;
            bH.bhHeight = *(uint64_t *)v.mv_data;
            result = mdb_cursor_put(cCur, (MDB_val *)&zeroKVal, &nV, MDB_APPENDDUP);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to put a record into BlockHeightR: ", result).c_str()));
            }

            /*!
             * we delete the old records immediately, so the overall DB and mapsize should not grow.
             * This is a little slower than just letting mdb_drop() delete it all at the end, but
             * it saves a significant amount of disk space.
             */
            result = mdb_cursor_del(cOld, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from BlockHeights: ", result).c_str()));
            }
            i++;
        }

        result = mdb_txn_begin (mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }
        /*!
         * delete the old table
         */
        result = mdb_drop (txn, oHeights, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete old BlockHeights table: ", result).c_str()));
        }

        RENAME_DB("BlockHeightR");

        /*!
         * close and reopen to get old dbi slot back
         */
        mdb_dbi_close (mEnv, mBlockHeights);
        LMDBDBOpen (txn,
                    "BlockHeights",
                    MDB_INTEGERKEY | MDB_DUPSORT | MDB_DUPFIXED,
                    mBlockHeights,
                    "Failed to open db handle for BlockHeights");
        mdb_set_dupsort(txn, mBlockHeights, compareHash32);
        txn.commit();
    } while(0);

    /*!
     * old tables are k(height), v(value).
     * new table is DUPFIXED, k(zeroval), v{height, values...}.
     */
    do {
        MDB_dbi coins;
        result = mdb_txn_begin(mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }
        result = mdb_dbi_open (txn, "BlockCoins", 0, &coins);
        if (result == MDB_NOTFOUND) {
            txn.abort ();
            break;
        }
        MDB_dbi diffs, hashes, sizes, timestamps;
        mdbBlockInfo bI;
        MDBValSet (nV, bI);

        LMDBDBOpen (txn, "BlockDiffs", 0, diffs, "Failed to open db handle for BlockDiffs");
        LMDBDBOpen (txn, "BlockHashes", 0, hashes, "Failed to open db handle for BlockHashes");
        LMDBDBOpen (txn, "BlockSizes", 0, sizes, "Failed to open db handle for BlockSizes");
        LMDBDBOpen (txn, "BlockTimestamps", 0, timestamps, "Failed to open db handle for BlockTimestamps");
        MDB_cursor *cCur, *cCoins, *cDiffs, *cHashes, *cSizes, *cTimestamps;
        i = 0;
        z = mHeight;

        while (1) {
            MDB_val k, v;
            if (!(i % 2000)) {
                if (i) {
                    std::cout << i << " / " << z << "  \r" << std::flush;
                    txn.commit();
                    result = mdb_txn_begin(mEnv, NULL, 0, txn);
                    if (result ) {
                        throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
                    }
                }

                result = mdb_cursor_open (txn, mBlockInfo, &cCur);
                if (result) {
                    throw(DB_ERROR(LMDBError ("Failed to open a cursor for BlockInfo: ", result).c_str()));
                }

                result = mdb_cursor_open (txn, coins, &cCoins);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for BlockCoins: ", result).c_str()));
                }

                result = mdb_cursor_open (txn, diffs, &cDiffs);
                if (result) {
                    throw(DB_ERROR(LMDBError ("Failed to open a cursor for BlockDiffs: ", result).c_str()));
                }

                result = mdb_cursor_open (txn, hashes, &cHashes);
                if (result) {
                    throw(DB_ERROR(LMDBError ("Failed to open a cursor for BlockHashes: ", result).c_str()));
                }

                result = mdb_cursor_open (txn, sizes, &cSizes);
                if (result) {
                    throw(DB_ERROR(LMDBError ("Failed to open a cursor for BlockSizes: ", result).c_str()));
                }

                result = mdb_cursor_open (txn, timestamps, &cTimestamps);
                if (result) {
                    throw(DB_ERROR(LMDBError ("Failed to open a cursor for BlockTimestamps: ", result).c_str()));
                }

                if (!i) {
                    MDB_stat ms;
                    mdb_stat (txn, mBlockInfo, &ms);
                    i = ms.ms_entries;
                }
            }

            result = mdb_cursor_get (cCoins, &k, &v, MDB_NEXT);
            if (result == MDB_NOTFOUND) {
                break;
            } else if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from BlockCoins: ", result).c_str()));
            }
            bI.biHeight = *(uint64_t *)k.mv_data;
            bI.biCoins = *(uint64_t *)v.mv_data;

            result = mdb_cursor_get (cDiffs, &k, &v, MDB_NEXT);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from BlockDiffs: ", result).c_str()));
            }
            bI.biDiff = *(uint64_t *)v.mv_data;

            result = mdb_cursor_get(cDiffs, &k, &v, MDB_NEXT);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from BlockHashes: ", result).c_str()));
            }
            bI.biHash = *(Crypto::Hash *)v.mv_data;

            result = mdb_cursor_get (cSizes, &k, &v, MDB_NEXT);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from BlockSizes: ", result).c_str()));
            }
            if (v.mv_size == sizeof(uint32_t)) {
                bI.biSize = *(uint32_t *)v.mv_data;
            } else {
                bI.biSize = *(uint64_t *)v.mv_data;
            }

            result = mdb_cursor_get (cTimestamps, &k, &v, MDB_NEXT);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from BlockTimestamps: ", result).c_str()));
            }
            bI.biTimestamp = *(uint64_t *)v.mv_data;

            result = mdb_cursor_put (cCur, (MDB_val *)&zeroKVal, &nV, MDB_APPENDDUP);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to put a record into BlockInfo: ", result).c_str()));
            }

            result = mdb_cursor_del (cCoins, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from BlockCoins: ", result).c_str()));
            }

            result = mdb_cursor_del (cDiffs, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from BlockDiffs: ", result).c_str()));
            }

            result = mdb_cursor_del (cHashes, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from BlockHashes: ", result).c_str()));
            }

            result = mdb_cursor_del (cSizes, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from BlockSizes: ", result).c_str()));
            }

            result = mdb_cursor_del (cTimestamps, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from BlockTimestamps: ", result).c_str()));
            }
            i++;
        }

        mdb_cursor_close (cTimestamps);
        mdb_cursor_close (cSizes);
        mdb_cursor_close (cHashes);
        mdb_cursor_close (cDiffs);
        mdb_cursor_close (cCoins);

        result = mdb_drop (txn, timestamps, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete BlockTimestamps from the db: ", result).c_str()));
        }

        result = mdb_drop (txn, sizes, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete BlockSizes from the db: ", result).c_str()));
        }

        result = mdb_drop (txn, hashes, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete BlockHashes from the db: ", result).c_str()));
        }

        result = mdb_drop (txn, diffs, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete BlockDiffs from the db: ", result).c_str()));
        }

        result = mdb_drop (txn, coins, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete BlockCoins from the db: ", result).c_str()));
        }

        txn.commit ();
    } while(0);

    do {
        MDB_dbi  oHfV;
        unsigned int flags;

        result = mdb_txn_begin (mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }

        result = mdb_dbi_flags (txn, mHfVersions, &flags);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to retrieve HfVersions flags: ", result).c_str()));
        }

        /*!
         * if the flags are what we expect, this table has already been migrated
         */
        if (flags & MDB_INTEGERKEY) {
            txn.abort ();
            break;
        }

        /*!
         * the hf_versions table name is the same but the old version and new version
         * have incompatible DB flags. Create a new table with the right flags.
         */
        oHfV = mHfVersions;
        LMDBDBOpen (txn,
                    "HfVersionR",
                    MDB_INTEGERKEY | MDB_CREATE,
                    mHfVersions,
                    "Failed to open db handle for HfVersionR");

        MDB_cursor *cOld, *cCur;
        i = 0;
        z = mHeight;

        while (1) {
            if (!(i % 2000)) {
                if (i) {
                    std::cout << i << " / " << z << "  \r" << std::flush;
                    txn.commit();
                    result = mdb_txn_begin (mEnv, NULL, 0, txn);
                    if (result) {
                        throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
                    }
                }

                result = mdb_cursor_open (txn, mHfVersions, &cCur);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for SpentKeyR: ", result).c_str()));
                }

                result = mdb_cursor_open (txn, oHfV, &cOld);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for SpentKeys: ", result).c_str()));
                }

                if (!i) {
                    MDB_stat ms;
                    mdb_stat (txn, mHfVersions, &ms);
                    i = ms.ms_entries;
                }
            }

            result = mdb_cursor_get (cOld, &k, &v, MDB_NEXT);
            if (result == MDB_NOTFOUND) {
                txn.commit ();
                break;
            } else if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from HfVersions: ", result).c_str()));
            }

            result = mdb_cursor_put (cCur, &k, &v, MDB_APPEND);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to put a record into HfVersionR: ", result).c_str()));
            }

            result = mdb_cursor_del (cOld, 0);
            if (result) {
                throw(DB_ERROR(LMDBError("Failed to delete a record from HfVersions: ", result).c_str()));
            }

            i++;
        }

        result = mdb_txn_begin (mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }

        /*!
         * delete the old table
         */
        result = mdb_drop(txn, oHfV, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete old HfVersions table: ", result).c_str()));
        }

        RENAME_DB ("HfVersionR");
        mdb_dbi_close(mEnv, mHfVersions);
        LMDBDBOpen (txn,
                    "HfVersion",
                    MDB_INTEGERKEY,
                    mHfVersions,
                    "Failed to open db handle for HfVersions");

        txn.commit ();
    } while(0);

    do {
        /*!
         * Delete all other tables, we're just going to recreate them
         */
        MDB_dbi dbi;
        result = mdb_dbi_open (txn, "TxUnlocks", 0, &dbi);
        if (result == MDB_NOTFOUND) {
            txn.abort ();
            break;
        }
        txn.abort();

#define DELETE_DB(x) do {                                                             \
                result = mdb_txn_begin(mEnv, NULL, 0, txn);                                     \
                if (result) {                                                                   \
                    throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ",    \
                                              result).c_str()));                                \
                }                                                                               \
                result = mdb_dbi_open(txn, x, 0, &dbi);                                         \
                if (!result) {                                                                  \
                    result = mdb_drop(txn, dbi, 1);                                             \
                    if (result) {                                                               \
                        throw(DB_ERROR(LMDBError("Failed to delete " x ": ", result).c_str()));\
                    }                                                                           \
                    txn.commit();                                                               \
                }                                                                               \
            } while(0);

        DELETE_DB("TxHeights");
        DELETE_DB("OutputTxs");
        DELETE_DB("OutputIndices");
        DELETE_DB("OutputKeys");
        DELETE_DB("SpentKeys");
        DELETE_DB("OutputAmounts");
        DELETE_DB("TxOutputs");
        DELETE_DB("TxUnlocks");

        /*!
         * reopen new DBs with correct flags
         */
        result = mdb_txn_begin(mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }

        LMDBDBOpen(txn,
                   LMDB_OUTPUT_TXS,
                   MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
                   mOutputTxs,
                   "Failed to open db handle for mOutputTxs");
        mdb_set_dupsort(txn, mOutputTxs, compareUInt64);
        LMDBDBOpen(txn,
                   LMDB_TX_OUTPUTS,
                   MDB_INTEGERKEY | MDB_CREATE,
                   mTxOutputs,
                   "Failed to open db handle for mTxOutputs");
        LMDBDBOpen(txn,
                   LMDB_SPENT_KEYS,
                   MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
                   mSpentKeys,
                   "Failed to open db handle for mSpentKeys");
        mdb_set_dupsort(txn, mSpentKeys, compareHash32);
        LMDBDBOpen(txn,
                   LMDB_OUTPUT_AMOUNTS,
                   MDB_INTEGERKEY | MDB_DUPSORT | MDB_DUPFIXED | MDB_CREATE,
                   mOutputAmounts,
                   "Failed to open db handle for mOutputAmounts");
        mdb_set_dupsort(txn, mOutputAmounts, compareUInt64);
        txn.commit();
    } while(0);

    do {
        unsigned int flags;

        result = mdb_txn_begin(mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }

        result = mdb_dbi_flags(txn, mTxs, &flags);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to retrieve txs flags: ", result).c_str()));
        }

        /*!
         * if the flags are what we expect, this table has already been migrated
         */
        if (flags & MDB_INTEGERKEY) {
            txn.abort();
            break;
        }

        MDB_dbi oTxs;
        CryptoNote::blobData bD;
        CryptoNote::Block bT;
        MDB_val hK;

        oTxs = mTxs;
        mdb_set_compare(txn, oTxs, compareHash32);
        LMDBDBOpen(txn,
                   "TxR",
                   MDB_INTEGERKEY | MDB_CREATE,
                   mTxs,
                   "Failed to open db handle for TxR");

        txn.commit();

        MDB_cursor *cBlocks, *cTxs, *cProps, *cCur;
        i = 0;
        z = mHeight;

        hK.mv_size = sizeof (Crypto::Hash);
        setBatchTransactions(true);
        batchStart(1000);
        txn.mTxn = mWriteTxn->mTxn;
        mHeight = 0;

        while(1) {
            if (!(i % 1000)) {
                if (i) {
                    std::cout << i << " / " << z << "  \r" << std::flush;
                    MDBValSet(pK, "TxBlk");
                    MDBValSet(pV, mHeight);
                    result = mdb_cursor_put(cProps, &pK, &pV, 0);
                    if (result) {
                        throw(DB_ERROR(LMDBError("Failed to update TxBlk property: ", result).c_str()));
                    }
                    txn.commit();

                    result = mdb_txn_begin(mEnv, NULL, 0, txn);
                    if (result) {
                        throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
                    }

                    mWriteTxn->mTxn = txn.mTxn;
                    mWriteBatchTxn->mTxn = txn.mTxn;
                    memset(&mWCursors, 0, sizeof (mWCursors));
                }

                result = mdb_cursor_open(txn, mBlocks, &cBlocks);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for blocks: ", result).c_str()));
                }

                result = mdb_cursor_open(txn, mProperties, &cProps);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for properties: ", result).c_str()));
                }

                result = mdb_cursor_open(txn, oTxs, &cTxs);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to open a cursor for txs: ", result).c_str()));
                }

                if (!i) {
                    MDB_stat ms;
                    mdb_stat(txn, mTxs, &ms);
                    i = ms.ms_entries;
                    if (i) {
                        MDBValSet(pK, "TxBlk");
                        result = mdb_cursor_get(cProps, &pK, &k, MDB_SET);
                        if (result) {
                            throw(DB_ERROR(LMDBError("Failed to get a record from properties: ", result).c_str()));
                        }
                        mHeight = *(uint64_t *)k.mv_data;
                    }
                }
                if (i) {
                    result = mdb_cursor_get(cBlocks, &k, &v, MDB_SET);
                    if (result) {
                        throw(DB_ERROR(LMDBError("Failed to get a record from blocks: ", result).c_str()));
                    }
                }


            }

            result = mdb_cursor_get(cBlocks, &k, &v, MDB_NEXT);
            if (result == MDB_NOTFOUND) {
                MDBValSet(pK, "TxBlk");
                result = mdb_cursor_get(cProps, &pK, &v, MDB_SET);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to get a record from props: ", result).c_str()));
                }

                result = mdb_cursor_del(cProps, 0);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to delete a record from props: ", result).c_str()));
                }
                batchStop();
                break;
            } else if (result) {
                throw(DB_ERROR(LMDBError("Failed to get a record from blocks: ", result).c_str()));
            }

            bD.assign(reinterpret_cast<char *>(v.mv_data), v.mv_size);
            if (!parseAndValidateBlockFromBlob(bD, bT)) {
                throw(DB_ERROR("Failed to parse block from blob retrieved from the db"));
            }

            addTransaction(NULL_HASH, bT.baseTransaction);
            for (unsigned int j = 0; j < bT.transactionHashes.size(); j++) {
                CryptoNote::Transaction tx;
                hK.mv_data = &bT.transactionHashes[j];
                result = mdb_cursor_get(cTxs, &hK, &v, MDB_SET);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to get record from txs: ", result).c_str()));
                }
                bD.assign(reinterpret_cast<char *>(v.mv_data), v.mv_size);
                if (!parseAndValidateTxFromBlob(bD, tx)) {
                    throw(DB_ERROR("Failed to parse tx from blob retrieved from the db"));
                }

                addTransaction(NULL_HASH, tx, &bT.transactionHashes[j]);
                result = mdb_cursor_del(cTxs, 0);
                if (result) {
                    throw(DB_ERROR(LMDBError("Failed to get record from txs: ", result).c_str()));
                }
            }

            i++;
            mHeight = i;
        }

        result = mdb_txn_begin(mEnv, NULL, 0, txn);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
        }

        result = mdb_drop(txn, oTxs, 1);
        if (result) {
            throw(DB_ERROR(LMDBError("Failed to delete txs from the db: ", result).c_str()));
        }

        RENAME_DB("TxR");

        mdb_dbi_close(mEnv, mTxs);

        LMDBDBOpen(txn, "Txs", MDB_INTEGERKEY, mTxs, "Failed to open db handle for Txs");

        txn.commit();
    } while(0);

    uint32_t version = 1;
    v.mv_data = (void *)&version;
    v.mv_size = sizeof (version);
    MDBValCopy<const char *> vK("Version");

    result = mdb_txn_begin(mEnv, NULL, 0, txn);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to create a transaction for the db: ", result).c_str()));
    }

    result = mdb_put(txn, mProperties,&vK, &v, 0);
    if (result) {
        throw(DB_ERROR(LMDBError("Failed to update version for the db: ", result).c_str()));
    }

    txn.commit();
}

void BlockchainLMDB::migrate(const uint32_t oldVersion)
{
    switch(oldVersion) {
    case 0:
        migrate0To1 ();
    default:
        ;
    }
}
} // namespace CryptoNote
