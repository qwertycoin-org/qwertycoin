

#include <atomic>

#include <boost/thread/tss.hpp>

#include <lmdb/lmdb.h>

#include <CryptoNoteCore/LMDB/BlockchainDB.h>
#include <CryptoNoteCore/LMDB/BinaryArrayDataType.h>
#include <CryptoNoteCore/LMDB/Structures.h>

#include <Global/LMDBConfig.h>

#define ENABLE_AUTO_RESIZE 1

namespace CryptoNote {
typedef struct mdbTxnCursors
{
    MDB_cursor *mTxcBlocks;
    MDB_cursor *mTxcBlockHeights;
    MDB_cursor *mTxcBlockInfo;

    MDB_cursor *mTxcOutputTxs;
    MDB_cursor *mTxcOutputAmounts;

    MDB_cursor *mTxcTxs;
    MDB_cursor *mTxcTxIndices;
    MDB_cursor *mTxcTxOutputs;

    MDB_cursor *mTxcSpentKeys;

    MDB_cursor *mTxcTxPoolMeta;
    MDB_cursor *mTxcTxPoolBlob;

    MDB_cursor *mTxcHfVersions;
} mdbTxnCursors;

#define mCurBlocks          mCursors->mTxcBlocks
#define mCurBlockHeights    mCursors->mTxcBlockHeights
#define mCurBlockInfo       mCursors->mTxcBlockInfo
#define mCurOutputTxs       mCursors->mTxcOutputTxs
#define mCurOutputAmounts   mCursors->mTxcOutputAmounts
#define mCurTxs             mCursors->mTxcTxs
#define mCurTxIndices       mCursors->mTxcTxIndices
#define mCurTxOutputs       mCursors->mTxcTxOutputs
#define mCurSpentKeys       mCursors->mTxcSpentKeys
#define mCurTxPoolMeta      mCursors->mTxcTxPoolMeta
#define mCurTxPoolBlob      mCursors->mTxcTxPoolBlob
#define mCurHfVersions      mCursors->mTxcHfVersions

typedef struct mdbRflags
{
    bool mRfTxn;
    bool mRfBlocks;
    bool mRfBlockHeights;
    bool mRfBlockInfo;
    bool mRfOutputTxs;
    bool mRfOutputAmounts;
    bool mRfTxs;
    bool mRfTxIndices;
    bool mRfTxOutputs;
    bool mRfSpentKeys;
    bool mRfTxPoolMeta;
    bool mRfTxPoolBlob;
    bool mRfHfVersions;
} mdbRflags;

typedef struct mdbThreadInfo
{
    /*!
     * per-thread read txn
     */
    MDB_txn *mTiRtxn;

    /*!
     * per-thread read cursors
     */
    mdbTxnCursors mTiCursors;

    /*!
     * per-thread read state
     */
    mdbRflags mTiFlags;

    ~mdbThreadInfo();
} mdbThreadInfo;

struct mdbTxnSafe
{
    mdbTxnSafe(const bool check=true);
    ~mdbTxnSafe();

    void commit(std::string message = "");

    /*!
     * This should only be needed for batch transaction which must be ensured to
     * be aborted before mdb_env_close, not after. So we can't rely on
     * BlockchainLMDB destructor to call mdb_txn_safe destructor, as that's too late
     * to properly abort, since mdb_env_close would have been called earlier.
     */
    void abort();
    void uncheck();

    operator MDB_txn*()
    {
        return mTxn;
    }

    operator MDB_txn**()
    {
        return &mTxn;
    }

    static void preventNewTxns();
    static void waitNoActiveTxns();
    static void allowNewTxns();

    bool mBatchTxn = false;
    bool mCheck;

    mdbThreadInfo* mTInfo;
    MDB_txn* mTxn;

    uint64_t numActiveTx() const;
    static std::atomic<uint64_t> numActiveTxns;

    /*!
     * could use a mutex here, but this should be sufficient.
     */
    static std::atomic_flag creationGate;
};

/*!
 * If m_batch_active is set, a batch transaction exists beyond this class, such
 * as a batch import with verification enabled, or possibly (later) a batch
 * network sync.
 *
 * For some of the lookup methods, such as get_block_timestamp(), tx_exists(),
 * and get_tx(), when m_batch_active is set, the lookup uses the batch
 * transaction. This isn't only because the transaction is available, but it's
 * necessary so that lookups include the database updates only present in the
 * current batch write.
 *
 * A regular network sync without batch writes is expected to open a new read
 * transaction, as those lookups are part of the validation done prior to the
 * write for block and tx data, so no write transaction is open at the time.
 */
class BlockchainLMDB : public BlockchainDB
{
public:
    BlockchainLMDB(bool batchTransactions = false);
    ~BlockchainLMDB();

    virtual void open(const std::string &filename, const int mdbFlags = 0);
    virtual void close();
    virtual void sync();
    virtual void safeSyncMode(const bool onOff);
    virtual void reset();
    virtual std::vector<std::string> getFilenames() const;
    virtual std::string getDBName() const;
    virtual bool lock();
    virtual void unlock();
    virtual bool blockExists(const Crypto::Hash &h, uint64_t *height = NULL) const;
    virtual uint64_t getBlockHeight(const Crypto::Hash &h) const;
    virtual CryptoNote::BlockHeader getBlockHeader(const Crypto::Hash &h) const;
    virtual CryptoNote::blobData getBlockBlob(const Crypto::Hash &h) const;
    virtual CryptoNote::blobData getBlockBlobFromHeight(const uint64_t &height) const;
    virtual uint64_t getBlockTimestamp(const uint64_t &height) const;
    virtual uint64_t getTopBlockTimestamp() const;
    virtual size_t getBlockSize(const uint64_t &height) const;
    virtual uint64_t getBlockCumulativeDifficulty(const uint64_t &height) const;
    virtual uint64_t getBlockDifficulty(const uint64_t &height) const;
    virtual uint64_t getBlockAlreadyGeneratedCoins(const uint64_t &height) const;
    // virtual uint64_t getBlockAlreadyGeneratedTransactions(const uint64_t &height) const;
    virtual Crypto::Hash getBlockHashFromHeight(const uint64_t &height) const;
    virtual std::vector<CryptoNote::Block> getBlocksRange(const uint64_t &h1, const uint64_t &h2) const;
    virtual std::vector<Crypto::Hash> getHashesRange(const uint64_t &h1, const uint64_t &h2) const;
    virtual Crypto::Hash getTopBlockHash() const;
    virtual CryptoNote::Block getTopBlock() const;
    virtual uint64_t height() const;
    virtual bool txExists(const Crypto::Hash &h) const;
    virtual bool txExists(const Crypto::Hash &h, uint64_t &txIndex) const;
    virtual uint64_t getTxUnlockTime(const Crypto::Hash &h) const;
    virtual bool getTxBlob(const Crypto::Hash &h, CryptoNote::blobData &tx) const;
    virtual uint64_t getTxCount() const;
    virtual std::vector<CryptoNote::Transaction> getTxList(const std::vector<Crypto::Hash> &hList) const;
    virtual uint64_t getTxBlockHeight(const Crypto::Hash &h) const;
    virtual uint64_t getNumOutputs(const uint64_t& amount) const;
    virtual OutputDataT getOutputKey(const uint64_t &amount, const uint32_t &index);
    virtual OutputDataT getOutputKey(const uint32_t &globalIndex) const;
    virtual void getOutputKey(const uint64_t &amount,
                              const std::vector<uint32_t> &offsets,
                              std::vector<OutputDataT> &outputs,
                              bool allowPartial = false);
    virtual txOutIndex getOutputTxAndIndexFromGlobal(const uint64_t &index) const;
    virtual void getOutputTxAndIndexFromGlobal(const std::vector<uint64_t> &globalIndices,
                                               std::vector<txOutIndex> &txOutIndices) const;
    virtual txOutIndex getOutputTxAndIndex(const uint64_t &amount, const uint32_t &index) const;
    virtual void getOutputTxAndIndex(const uint64_t &amount,
                                     const std::vector<uint32_t> &offsets,
                                     std::vector<txOutIndex> &indices) const;
    virtual std::vector<uint64_t> getTxAmountOutputIndices(const uint64_t txId) const;
    virtual bool hasKeyImage(const Crypto::KeyImage &kImg) const;
    virtual void addTxPoolTx(const CryptoNote::Transaction &tx, const TxPoolTxMetaT &meta);
    virtual void updateTxPoolTx(const Crypto::Hash &txId, const TxPoolTxMetaT &meta);
    virtual uint64_t getTxPoolTxCount() const;
    virtual bool txPoolHasTx(const Crypto::Hash &txId) const;
    virtual void removeTxPoolTx(const Crypto::Hash &txId);
    virtual bool getTxPoolTxMeta(const Crypto::Hash &txId, TxPoolTxMetaT &meta) const;
    virtual bool getTxPoolTxBlob(const Crypto::Hash &txId, CryptoNote::blobData &bD) const;
    virtual CryptoNote::blobData getTxPoolTxBlob(const Crypto::Hash &txId) const;
    virtual bool forAllTxPoolTxes(std::function<bool(const Crypto::Hash &,
                                                     const TxPoolTxMetaT &,
                                                     const CryptoNote::blobData *)> f,
                                  bool includeBlob = false,
                                  bool includeUnrelayedTxes = true) const;
    virtual bool forAllKeyImages(std::function<bool(const Crypto::KeyImage &)>) const;
    virtual bool forBlocksRange(const uint64_t &h1,
                                const uint64_t &h2,
    std::function<bool(uint64_t,
    const Crypto::Hash &,
    const CryptoNote::Block &)>) const;
    virtual bool forAllTransactions(std::function<bool(const Crypto::Hash &,
                                                       const CryptoNote::Transaction&)>) const;
    virtual bool forAllOutputs(std::function<bool(uint64_t amount,
    const Crypto::Hash &txHash,
            uint64_t height,
    size_t txIdx)> f) const;
    virtual bool forAllOutputs(uint64_t, const std::function<bool(uint64_t height)> &f) const;
    virtual uint64_t addBlock(const CryptoNote::Block &block,
                              const size_t &blockSize,
                              const uint64_t &cumulativeDifficulty,
                              const uint64_t &coinsGenerated,
                              const std::vector<CryptoNote::Transaction> &txs);
    virtual void setBatchTransactions(bool batchTransactions);
    virtual bool batchStart(uint64_t batchNumBlocks=0, uint64_t batchBytes=0);
    virtual void batchCommit();
    virtual void batchStop();
    virtual void batchAbort();
    virtual void blockTxnStart(bool readonly);
    virtual void blockTxnStop();
    virtual void blockTxnAbort();
    virtual bool blockRTxnStart(MDB_txn **mTxn, mdbTxnCursors **mCur) const;
    virtual void blockRTxnStop() const;
    virtual void popBlock(CryptoNote::Block &block, std::vector<CryptoNote::Transaction> &txs);
    virtual bool canThreadBulkIndices() const
    {
        return true;
    }

    virtual void removeSpentKeys(std::vector<Crypto::KeyImage> &kImages);

    /*!
     * @brief return a histogram of outputs on the blockchain
     *
     * @param amounts       optional set of amounts to lookup
     * @param unlocked      whether to restrict count to unlocked outputs
     * @param recentCutoff  timestamp to determine which outputs are recent
     *
     * @return a set of amount/instances
     */
    std::map<uint64_t, std::tuple<uint64_t,
            uint64_t,
            uint64_t>> getOutputHistorgram(const std::vector<uint64_t> &amounts,
                                           bool unlocked,
                                           uint64_t recentCutoff) const;
    void doResize(uint64_t sizeIncrease=0);

private:
    virtual void addBlock(const CryptoNote::Block &block,
                          const size_t &blockSize,
                          const uint64_t &cumulativeDifficulty,
                          const uint64_t &coinsGenerated,
                          const Crypto::Hash &blockHash);
    virtual void removeBlock();
    virtual uint64_t addTransactionData(const Crypto::Hash &blockHash,
                                        const CryptoNote::Transaction &tx,
                                        const Crypto::Hash &txHash);
    virtual void removeTransactionData(const Crypto::Hash &txHash, const CryptoNote::Transaction &tx);
    virtual uint64_t addOutput(const Crypto::Hash &txHash,
                               const CryptoNote::TransactionOutput &txOutput,
                               const uint64_t &localIndex,
                               const uint64_t &unlockTime);
    virtual void addTxAmountOutputIndices(const uint64_t txId,
                                          const std::vector<uint64_t> &amountOutputIndices);
    void removeTxOutputs(const uint64_t txId, const CryptoNote::Transaction &tx);
    void removeOutput(const uint64_t amount, const uint64_t &outIndex);
    virtual void addSpentKey(const Crypto::KeyImage &kImage);
    virtual void removeSpentKey(const Crypto::KeyImage &kImage);
    uint64_t numOutputs() const;
    virtual void setHardForkVersion(uint64_t height, uint8_t version);
    virtual uint8_t getHardForkVersion(uint64_t height) const;
    virtual void checkHardForkInfo();
    virtual void dropHardForkInfo();

    /*!
     * @brief convert a tx output to a blob for storage
     *
     * @param output    the output to convert
     *
     * @return the resultant blob
     */
    CryptoNote::blobData outputToBlob(const CryptoNote::TransactionOutput &output) const;

    /*!
     * @brief convert a tx output blob to a tx output
     *
     * @param blob      the blob to convert
     *
     * @return the resultant tx output
     */
    CryptoNote::TransactionOutput outputFromBlob(const CryptoNote::blobData &blob);

    void checkOpen() const;
    virtual bool isReadOnly() const;
    /*!
     * fix up anything that may be wrong to past bugs
     */
    virtual void fixup();

    /*!
     * migrate from older DB version to current
     */
    void migrate(const uint32_t oldVersion);

    /*!
     * migrate from DB version 0 to 1
     */
    void migrate0To1();
    void cleanupBatch();

private:
    MDB_env *mEnv;

    MDB_dbi mBlocks;
    MDB_dbi mBlockHeights;
    MDB_dbi mBlockInfo;

    MDB_dbi mTxs;
    MDB_dbi mTxIndices;
    MDB_dbi mTxOutputs;

    MDB_dbi mOutputTxs;
    MDB_dbi mOutputAmounts;

    MDB_dbi mSpentKeys;

    MDB_dbi mTxPoolMeta;
    MDB_dbi mTxPoolBlob;

    MDB_dbi mHfStartingHeights;
    MDB_dbi mHfVersions;

    MDB_dbi mProperties;

    /*!
     * used in batch size estimation
     */
    mutable uint64_t mCumSize;
    mutable unsigned int mCumCount;
    std::string mFolder;

    /*!
     * may point to either a shot-lived txn or batch txn
     */
    mdbTxnSafe *mWriteTxn;

    /*!
     * persist batch txn outside of BlockchainLMDB
     */
    mdbTxnSafe *mWriteBatchTxn;
    boost::thread::id mWriter;

    /*!
     * support for batch transactions
     */
    bool mBatchTransactions;

    /*!
     * whether batch transaction is in progress
     */
    bool mBatchActive;

    mdbTxnCursors mWCursors;
    mutable boost::thread_specific_ptr<mdbThreadInfo> mTInfo;
};
} // namespace CryptoNote
