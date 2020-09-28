

#pragma once

#include <exception>
#include <iostream>
#include <list>
#include <string>

#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#include <Common/CommandLine.h>

#include <crypto/hash.h>

#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/Hardfork.h>
#include <CryptoNoteCore/LMDB/BinaryArrayDataType.h>
#include <CryptoNoteCore/LMDB/Structures.h>

#include <Serialization/BinaryInputStreamSerializer.h>
#include <Serialization/BinaryOutputStreamSerializer.h>

/*! @file
 * CryptoNote Blockchain Database Interface
 *
 * The DB interface is a store for the canonical block chain.
 * It serves as a persistent storage for the blockchain.
 *
 * For the sake of efficiency, a concrete implementation may also
 * store some blockchain data outside of the blocks, such as spent
 * transfer key images, unspent transaction outputs, etc.
 *
 * Examples are as follows:
 *
 * Transactions are duplicated so that we don't have to fetch a whole block
 * in order to fetch a transaction from that block.
 *
 * Spent key images are duplicated outside of the blocks so it is quick
 * to verify an output hasn't already been spent
 *
 * Unspent transaction outputs are duplicated to quickly gather random
 * outputs to use for mixins
 *
 * Indices and Identifiers:
 * The word "index" is used ambiguously throughout this code. It is
 * particularly confusing when talking about the output or transaction
 * tables since their indexing can refer to themselves or each other.
 * I have attempted to clarify these usages here:
 *
 * Blocks, transactions, and outputs are all identified by a hash.
 * For storage efficiency, a 64-bit integer ID is used instead of the hash
 * inside the DB. Tables exist to map between hash and ID. A block ID is
 * also referred to as its "height". Transactions and outputs generally are
 * not referred to by ID outside of this module, but the tx ID is returned
 * by tx_exists() and used by get_tx_amount_output_indices(). Like their
 * corresponding hashes, IDs are globally unique.
 *
 * The remaining uses of the word "index" refer to local offsets, and are
 * not globally unique. An "amount output index" N refers to the Nth output
 * of a specific amount. An "output local index" N refers to the Nth output
 * of a specific tx.
 *
 * Exceptions:
 *   DB_ERROR -- generic
 *   DB_OPEN_FAILURE
 *   DB_CREATE_FAILURE
 *   DB_SYNC_FAILURE
 *   BLOCK_DNE
 *   BLOCK_PARENT_DNE
 *   BLOCK_EXISTS
 *   BLOCK_INVALID -- considering making this multiple errors
 *   TX_DNE
 *   TX_EXISTS
 *   OUTPUT_DNE
 *   OUTPUT_EXISTS
 *   KEY_IMAGE_EXISTS
 */

namespace CryptoNote {

#define DBF_SAFE       1
#define DBF_FAST       2
#define DBF_FASTEST    4
#define DBF_RDONLY     8
#define DBF_SALVAGE 0x10

/*!
 * Exception Definitions
 */

/*!
 * @brief A base class for BlockchainDB exceptions
 */
class DB_EXCEPTION : public std::exception
{
private:
    std::string m;

protected:
    DB_EXCEPTION(const char *s) : m(s) {}

public:
    virtual ~DB_EXCEPTION() {}

    const char *what() const throw()
    {
        return m.c_str();
    }
};

/*!
 * @brief A generic BlockchainDB exception
 */
class DB_ERROR : public DB_EXCEPTION
{
public:
    DB_ERROR() : DB_EXCEPTION("Generic DB Error") {}
    DB_ERROR(const char *s) : DB_EXCEPTION(s) {}
};

/*!
 * @brief thrown when there is an error starting a DB transaction
 */
class DB_ERROR_TXN_START : public DB_EXCEPTION
{
public:
    DB_ERROR_TXN_START() : DB_EXCEPTION("DB Error in starting txn") {}
    DB_ERROR_TXN_START(const char *s) : DB_EXCEPTION(s) {}
};

/*!
 * @brief thrown then opening the BlockchainDB fails
 */
class DB_OPEN_FAILURE : DB_EXCEPTION
{
public:
    DB_OPEN_FAILURE() : DB_EXCEPTION("Failed to open the db") {}
    DB_OPEN_FAILURE(const char *s) : DB_EXCEPTION(s) {}
};

/*!
 * @brief thrown when creating the BlockchainDB fails
 */
class DB_CREATE_FAILURE : public DB_EXCEPTION
{
public:
    DB_CREATE_FAILURE() : DB_EXCEPTION("Failed to create the db") { }
    DB_CREATE_FAILURE(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when synchronizing the BlockchainDB to disk fails
 */
class DB_SYNC_FAILURE : public DB_EXCEPTION
{
public:
    DB_SYNC_FAILURE() : DB_EXCEPTION("Failed to sync the db") { }
    DB_SYNC_FAILURE(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a requested block does not exist
 */
class BLOCK_DNE : public DB_EXCEPTION
{
public:
    BLOCK_DNE() : DB_EXCEPTION("The block requested does not exist") { }
    BLOCK_DNE(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a block's parent does not exist (and it needed to)
 */
class BLOCK_PARENT_DNE : public DB_EXCEPTION
{
public:
    BLOCK_PARENT_DNE() : DB_EXCEPTION("The parent of the block does not exist") { }
    BLOCK_PARENT_DNE(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a block exists, but shouldn't, namely when adding a block
 */
class BLOCK_EXISTS : public DB_EXCEPTION
{
public:
    BLOCK_EXISTS() : DB_EXCEPTION("The block to be added already exists!") { }
    BLOCK_EXISTS(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when something is wrong with the block to be added
 */
class BLOCK_INVALID : public DB_EXCEPTION
{
public:
    BLOCK_INVALID() : DB_EXCEPTION("The block to be added did not pass validation!") { }
    BLOCK_INVALID(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a requested transaction does not exist
 */
class TX_DNE : public DB_EXCEPTION
{
public:
    TX_DNE() : DB_EXCEPTION("The transaction requested does not exist") { }
    TX_DNE(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a transaction exists, but shouldn't, namely when adding a block
 */
class TX_EXISTS : public DB_EXCEPTION
{
public:
    TX_EXISTS() : DB_EXCEPTION("The transaction to be added already exists!") { }
    TX_EXISTS(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a requested output does not exist
 */
class OUTPUT_DNE : public DB_EXCEPTION
{
public:
    OUTPUT_DNE() : DB_EXCEPTION("The output requested does not exist!") { }
    OUTPUT_DNE(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when an output exists, but shouldn't, namely when adding a block
 */
class OUTPUT_EXISTS : public DB_EXCEPTION
{
public:
    OUTPUT_EXISTS() : DB_EXCEPTION("The output to be added already exists!") { }
    OUTPUT_EXISTS(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * @brief thrown when a spent key image exists, but shouldn't, namely when adding a block
 */
class KEY_IMAGE_EXISTS : public DB_EXCEPTION
{
public:
    KEY_IMAGE_EXISTS() : DB_EXCEPTION("The spent key image to be added already exists!") { }
    KEY_IMAGE_EXISTS(const char* s) : DB_EXCEPTION(s) { }
};

/*!
 * End of Exception Definitions
 */

/*!
 * @brief The BlockchainDB backing store interface declaration/contract
 *
 * This class provides a uniform interface for using BlockchainDB to store
 * a blockchain.  Any implementation of this class will also implement all
 * functions exposed here, so one can use this class without knowing what
 * implementation is being used.  Refer to each pure virtual function's
 * documentation here when implementing a BlockchainDB subclass.
 *
 * A subclass which encounters an issue should report that issue by throwing
 * a DB_EXCEPTION which adequately conveys the issue.
 */

class BlockchainDB
{

private:
    /*!
     * private virtual members
     */

    /*!
     * @brief add the block and metadata to the db
     *
     * The subclass implementing this will add the specified block and
     * block metadata to its backing store.  This does not include its
     * transactions, those are added in a separate step.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param block                 the block to be added
     * @param blockSize             the size of the block (transactions and all)
     * @param cumulativeDifficulty  the accumulated difficulty after this block
     * @param coinsGenerated        the number of coins generated total after this block
     * @param blockHash             the hash of the block
     */
    virtual void addBlock(const CryptoNote::Block &block,
                          const size_t &blockSize,
                          const uint64_t &cumulativeDifficulty,
                          const uint64_t &coinsGenerated,
                          const Crypto::Hash &blockHash) = 0;

    /*!
     * @brief remove data about the top block
     *
     * The subclass implementing this will remove the block data from the top
     * block in the chain.  The data to be removed is that which was added in
     * BlockchainDB::addBlock(const CryptoNote::Block &block,
     *                        const size_t &blockSize,
     *                        const uint64_t &cumulativeDifficulty,
     *                        const uint64_t &coinsGenerated,
     *                        const Crypto::Hash &blockHash)
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     */
    virtual void removeBlock() = 0;

    /*!
     * @brief store the transaction and its metadata
     *
     * The subclass implementing this will add the specified transaction data
     * to its backing store.  This includes only the transaction blob itself
     * and the other data passed here, not the separate outputs of the
     * transaction.
     *
     * It returns a tx ID, which is a mapping from the tx_hash. The tx ID
     * is used in #add_tx_amount_output_indices().
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param blockHash     the hash of the block containing the transaction
     * @param tx            the transaction to be added
     * @param txHash        the hash of the transaction
     *
     * @return the transaction ID
     */
    virtual uint64_t addTransactionData(const Crypto::Hash &blockHash,
                                        const CryptoNote::Transaction &tx,
                                        const Crypto::Hash &txHash) = 0;

    /*!
     * @brief remove data about a transaction
     *
     * The subclass implementing this will remove the transaction data
     * for the passed transaction.  The data to be removed was added in
     * addTransactionData().  Additionally, current subclasses have behavior
     * which requires the transaction itself as a parameter here.  Future
     * implementations should note that this parameter is subject to be removed
     * at a later time.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param txHash        the hash of the transaction to be removed
     * @param tx            the transaction
     */
    virtual void removeTransactionData(const Crypto::Hash &txHash,
                                       const CryptoNote::Transaction &tx) = 0;

    /*!
     * @brief store an output
     *
     * The subclass implementing this will add the output data passed to its
     * backing store in a suitable manner.  In addition, the subclass is responsible
     * for keeping track of the global output count in some manner, so that
     * outputs may be indexed by the order in which they were created.  In the
     * future, this tracking (of the number, at least) should be moved to
     * this class, as it is necessary and the same among all BlockchainDB.
     *
     * It returns an amount output index, which is the index of the output
     * for its specified amount.
     *
     * This data should be stored in such a manner that the only thing needed to
     * reverse the process is the txOut.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param txHash        hash of the transaction the output was created by
     * @param txOutput      the output
     * @param localIndex    index of the output in its transaction
     * @param unlockTime    unlock time/height of the output
     *
     * @return amount output index
     */
    virtual uint64_t addOutput(const Crypto::Hash &txHash,
                               const CryptoNote::TransactionOutput &txOutput,
                               const uint64_t &index,
                               const uint64_t &unlock_time) = 0;

    /*!
     * @brief store amount output indices for a tx's outputs
     *
     * The subclass implementing this will add the amount output indices to its
     * backing store in a suitable manner. The txId will be the same one that
     * was returned from addOutput().
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param txId                  ID of the transaction containing these outputs
     * @param amountOutputIndices   the amount output indices of the transaction
     */
    virtual void addTxAmountOutputIndices(const uint64_t txId,
                                          const std::vector<uint64_t> &amountOutputIndices) = 0;

    /*!
     * @brief store a spent key
     *
     * The subclass implementing this will store the spent key image.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param kImage the spent key image to store
     */
    virtual void addSpentKey(const Crypto::KeyImage &kImage) = 0;

    /*!
     * @brief remove a spent key
     *
     * The subclass implementing this will remove the key image.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param kImage the spent key image to remove
     */
    virtual void removeSpentKey(const Crypto::KeyImage &kImage) = 0;

    /*!
     * private concrete members
     */

    /*!
     * @brief private version of popBlock, for undoing if an addBlock fails
     *
     * This function simply calls popBlock(Block &blk,
     *                                     std::vector<CryptoNote::Transaction> &txs)
     * with dummy parameters, as the returns-by-reference can be discarded.
     */
    void popBlock();

    // helper function to remove transaction from blockchain

    /*!
     * a performance metric
     */
    uint64_t numCalls = 0;
    uint64_t timeBlockHash = 0;
    uint64_t timeAddBlock1 = 0;
    uint64_t timeAddTransaction = 0;

protected:
    /*!
     * @brief helper function for addTransactions, to add each individual transaction
     *
     * This function is called by addTransactions() for each transaction to be
     * added.
     *
     * @param blockHash         hash of the block which has the transaction
     * @param tx                the transaction to add
     * @param txHashPtr         the hash of the transaction, if already calculated
     */
    void addTransaction(const Crypto::Hash &blockHash,
                        const CryptoNote::Transaction &tx,
                        const Crypto::Hash *txHashPtr = NULL);

    mutable uint64_t timeTxExists = 0;  //!< a performance metric
    uint64_t timeCommit1 = 0;  //!< a performance metric
    bool mAutoRemoveLogs = true;  //!< whether or not to automatically remove old logs

    Hardfork* mHardfork;

public:
    virtual void doResize();
    friend class BlockchainLMDB;

    BlockchainDB() : mOpen(false) {}

    /*!
     * @brief An empty destructor
     */
    virtual ~BlockchainDB() {};

    /*!
     * @brief reset profiling stats
     */
    void resetStats();

    /*!
     * @brief show profiling stats
     *
     * This function prints current performance/profiling data to whichever
     * log file(s) are set up (possibly including stdout or stderr)
     */
    void showStats();

    /*!
     * @brief open a db, or create it if necessary.
     *
     * This function opens an existing database or creates it if it
     * does not exist.
     *
     * The subclass implementing this will handle all file opening/creation,
     * and is responsible for maintaining its state.
     *
     * The parameter <filename> may not refer to a file name, necessarily, but
     * could be an IP:PORT for a database which needs it, and so on.  Calling it
     * <filename> is convenient and should be descriptive enough, however.
     *
     * For now, dbFlags are
     * specific to the subclass being instantiated.  This is subject to change,
     * and the dbFlags parameter may be deprecated.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param filename      a string referring to the BlockchainDB to open
     * @param dbFlags      flags relevant to how to open/use the BlockchainDB
     */
    virtual void open(const std::string &filename, const int dbFlags = 0) = 0;

    /*!
     * @brief Gets the current open/ready state of the BlockchainDB
     *
     * @return true if open/ready, otherwise false
     */
    bool isOpen() const;

    /*!
     * @brief close the BlockchainDB
     *
     * At minimum, this call ensures that further use of the BlockchainDB
     * instance will not have effect.  In any case where it is necessary
     * to do so, a subclass implementing this will sync with disk.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     */
    virtual void close() = 0;

    /*!
     * @brief sync the BlockchainDB with disk
     *
     * This function should write any changes to whatever permanent backing
     * store the subclass uses.  Example: a BlockchainDB instance which
     * keeps the whole blockchain in RAM won't need to regularly access a
     * disk, but should write out its state when this is called.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     */
    virtual void sync() = 0;

    /*!
     * @brief toggle safe syncs for the DB
     *
     * Used to switch DBF_SAFE on or off after starting up with DBF_FAST.
     */
    virtual void safeSyncMode(const bool onOff) = 0;

    /*!
     * @brief Remove everything from the BlockchainDB
     *
     * This function should completely remove all data from a BlockchainDB.
     *
     * Use with caution!
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     */
    virtual void reset() = 0;

    /*!
     * @brief get all files used by the BlockchainDB (if any)
     *
     * This function is largely for ease of automation, namely for unit tests.
     *
     * The subclass implementation should return all filenames it uses.
     *
     * @return a list of filenames
     */
    virtual std::vector<std::string> getFilenames() const = 0;

    // return the name of the folder the db's file(s) should reside in
    /*!
     * @brief gets the name of the folder the BlockchainDB's file(s) should be in
     *
     * The subclass implementation should return the name of the folder in which
     * it stores files, or an empty string if there is none.
     *
     * @return the name of the folder with the BlockchainDB's files, if any.
     */
    virtual std::string getDBName() const = 0;

    // FIXME: these are just for functionality mocking, need to implement
    // RAII-friendly and multi-read one-write friendly locking mechanism
    //
    // acquire db lock
    /*!
     * @brief acquires the BlockchainDB lock
     *
     * This function is a stub until such a time as locking is implemented at
     * this level.
     *
     * The subclass implementation should return true unless implementing a
     * locking scheme of some sort, in which case it should return true upon
     * acquisition of the lock and block until then.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @return true, unless at a future time false makes sense (timeout, etc)
     */
    virtual bool lock() = 0;

    // release db lock
    /*!
     * @brief This function releases the BlockchainDB lock
     *
     * The subclass, should it have implemented lock(), will release any lock
     * held by the calling thread.  In the case of recursive locking, it should
     * release one instance of a lock.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     */
    virtual void unlock() = 0;

    /*!
     * @brief tells the BlockchainDB to start a new "batch" of blocks
     *
     * If the subclass implements a batching method of caching blocks in RAM to
     * be added to a backing store in groups, it should start a batch which will
     * end either when <batch_num_blocks> has been added or batch_stop() has
     * been called.  In either case, it should end the batch and write to its
     * backing store.
     *
     * If a batch is already in-progress, this function must return false.
     * If a batch was started by this call, it must return true.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param batchNumBlocks    number of blocks to batch together
     *
     * @return true if we started the batch, false if already started
     */
    virtual bool batchStart(uint64_t batchNumBlocks=0, uint64_t batchBytes=0) = 0;

    /*!
     * @brief ends a batch transaction
     *
     * If the subclass implements batching, this function should store the
     * batch it is currently on and mark it finished.
     *
     * If no batch is in-progress, this function should throw a DB_ERROR.
     * This exception may change in the future if it is deemed necessary to
     * have a more granular exception type for this scenario.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     */
    virtual void batchStop() = 0;

    /*!
     * @brief sets whether or not to batch transactions
     *
     * If the subclass implements batching, this function tells it to begin
     * batching automatically.
     *
     * If the subclass implements batching and has a batch in-progress, a
     * parameter of false should disable batching and call batchStop() to
     * store the current batch.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param bool  batch whether or not to use batch transactions.
     */
    virtual void setBatchTransactions(bool) = 0;

    virtual void blockTxnStart(bool readOnly=false) = 0;
    virtual void blockTxnStop() = 0;
    virtual void blockTxnAbort() = 0;

    virtual void setHardFork(Hardfork *hf);

    // adds a block with the given metadata to the top of the blockchain, returns the new height
    /*!
     * @brief handles the addition of a new block to BlockchainDB
     *
     * This function organizes block addition and calls various functions as
     * necessary.
     *
     * NOTE: subclass implementations of this (or the functions it calls) need
     * to handle undoing any partially-added blocks in the event of a failure.
     *
     * If any of this cannot be done, the subclass should throw the corresponding
     * subclass of DB_EXCEPTION
     *
     * @param block                 the block to be added
     * @param blockSize             the size of the block (transactions and all)
     * @param cumulativeDifficulty  the accumulated difficulty after this block
     * @param coinsGenerated        the number of coins generated total after this block
     * @param txs                   the transactions in the block
     *
     * @return the height of the chain post-addition
     */
    virtual uint64_t addBlock(const CryptoNote::Block &block,
                              const size_t &blockSize,
                              const uint64_t &cumulativeDifficulty,
                              const uint64_t &coinsGenerated,
                              const std::vector<CryptoNote::Transaction> &txs);

    /*!
     * @brief checks if a block exists
     *
     * @param h         the hash of the requested block
     * @param height    if non NULL, returns the block's height if found
     *
     * @return true of the block exists, otherwise false
     */
    virtual bool blockExists(const Crypto::Hash &h, uint64_t *height = NULL) const = 0;

    /*!
     * @brief fetches the block with the given hash
     *
     * The subclass should return the requested block.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param h     the hash to look for
     *
     * @return the block requested
     */
    virtual CryptoNote::blobData getBlockBlob(const Crypto::Hash &h) const = 0;

    /*!
     * @brief fetches the block with the given hash
     *
     * Returns the requested block.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param h     the hash to look for
     *
     * @return the block requested
     */
    virtual CryptoNote::Block getBlock(const Crypto::Hash &h) const;

    /*!
     * @brief gets the height of the block with a given hash
     *
     * The subclass should return the requested height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param h     the hash to look for
     *
     * @return the height
     */
    virtual uint64_t getBlockHeight(const Crypto::Hash &h) const = 0;

    /*!
     * @brief fetch a block header
     *
     * The subclass should return the block header from the block with
     * the given hash.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param h     the hash to look for
     *
     * @return the block header
     */
    virtual CryptoNote::BlockHeader getBlockHeader(const Crypto::Hash &h) const = 0;

    /*!
     * @brief fetch a block blob by height
     *
     * The subclass should return the block at the given height.
     *
     * If the block does not exist, that is to say if the blockchain is not
     * that height, then the subclass should throw BLOCK_DNE
     *
     * @param height    the height to look for
     *
     * @return the block blob
     */
    virtual CryptoNote::blobData getBlockBlobFromHeight(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a block by height
     *
     * If the block does not exist, that is to say if the blockchain is not
     * that high, then the subclass should throw BLOCK_DNE
     *
     * @param height    the height to look for
     *
     * @return the block
     */
    virtual CryptoNote::Block getBlockFromHeight(const uint64_t &height) const;

    /*!
     * @brief fetch a block's timestamp
     *
     * The subclass should return the timestamp of the block with the
     * given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the timestamp
     */
    virtual uint64_t getBlockTimestamp(const uint64_t &height) const = 0;

    /*!
     * @brief fetch the top block's timestamp
     *
     * The subclass should return the timestamp of the most recent block.
     *
     * @return the top block's timestamp
     */
    virtual uint64_t getTopBlockTimestamp() const = 0;

    /*!
     * @brief fetch a block's size
     *
     * The subclass should return the size of the block with the
     * given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the size
     */
    virtual size_t getBlockSize(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a block's cumulative difficulty
     *
     * The subclass should return the cumulative difficulty of the block with the
     * given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the cumulative difficulty
     */
    virtual uint64_t getBlockCumulativeDifficulty(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a block's difficulty
     *
     * The subclass should return the difficulty of the block with the
     * given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the difficulty
     */
    virtual uint64_t getBlockDifficulty(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a block's already generated coins
     *
     * The subclass should return the total coins generated as of the block
     * with the given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the already generated coins
     */
    virtual uint64_t getBlockAlreadyGeneratedCoins(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a block's already generated transactions
     *
     * The subclass should return the total transactions generated as of the block
     * with the given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the already generated transactions
     */
    // virtual uint64_t getBlockAlreadyGeneratedTransactions(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a block's hash
     *
     * The subclass should return hash of the block with the
     * given height.
     *
     * If the block does not exist, the subclass should throw BLOCK_DNE
     *
     * @param height    the height requested
     *
     * @return the hash
     */
    virtual Crypto::Hash getBlockHashFromHeight(const uint64_t &height) const = 0;

    /*!
     * @brief fetch a list of blocks
     *
     * The subclass should return a vector of blocks with heights starting at
     * h1 and ending at h2, inclusively.
     *
     * If the height range requested goes past the end of the blockchain,
     * the subclass should throw BLOCK_DNE.  (current implementations simply
     * don't catch this exception as thrown by methods called within)
     *
     * @param h1    the start height
     * @param h2    the end height
     *
     * @return a vector of blocks
     */
    virtual std::vector<CryptoNote::Block> getBlocksRange(const uint64_t &h1, const uint64_t &h2) const = 0;

    /*!
     * @brief fetch a list of block hashes
     *
     * The subclass should return a vector of block hashes from blocks with
     * heights starting at h1 and ending at h2, inclusively.
     *
     * If the height range requested goes past the end of the blockchain,
     * the subclass should throw BLOCK_DNE.  (current implementations simply
     * don't catch this exception as thrown by methods called within)
     *
     * @param h1    the start height
     * @param h2    the end height
     *
     * @return a vector of block hashes
     */
    virtual std::vector<Crypto::Hash> getHashesRange(const uint64_t &h1, const uint64_t &h2) const = 0;

    /*!
     * @brief fetch the top block's hash
     *
     * The subclass should return the hash of the most recent block
     *
     * @return the top block's hash
     */
    virtual Crypto::Hash getTopBlockHash() const = 0;

    /*!
     * @brief fetch the top block
     *
     * The subclass should return most recent block
     *
     * @return the top block
     */
    virtual CryptoNote::Block getTopBlock() const = 0;

    /*!
     * @brief fetch the current blockchain height
     *
     * The subclass should return the current blockchain height
     *
     * @return the current blockchain height
     */
    virtual uint64_t height() const = 0;

    /*!
     * TODO: Rewrite (if necessary) such that all calls to remove* are
     *       done in concrete members of this base class.
     *
     * @brief pops the top block off the blockchain
     *
     * The subclass should remove the most recent block from the blockchain,
     * along with all transactions, outputs, and other metadata created as
     * a result of its addition to the blockchain.  Most of this is handled
     * by the concrete members of the base class provided the subclass correctly
     * implements remove* functions.
     *
     * The subclass should return by reference the popped block and
     * its associated transactions
     *
     * @param block       return-by-reference the block which was popped
     * @param txs       return-by-reference the transactions from the popped block
     */
    virtual void popBlock(CryptoNote::Block &blk,
                          std::vector<CryptoNote::Transaction> &txs);

    /*!
     * @brief check if a transaction with a given hash exists
     *
     * The subclass should check if a transaction is stored which has the
     * given hash and return true if so, false otherwise.
     *
     * @param h         the hash to check against
     * @param txId     (optional) returns the txId for the tx hash
     *
     * @return true if the transaction exists, otherwise false
     */
    virtual bool txExists(const Crypto::Hash &h) const = 0;
    virtual bool txExists(const Crypto::Hash &h, uint64_t &txId) const = 0;

    // return unlock time of tx with hash <h>
    /*!
     * @brief fetch a transaction's unlock time/height
     *
     * The subclass should return the stored unlock time for the transaction
     * with the given hash.
     *
     * If no such transaction exists, the subclass should throw TX_DNE.
     *
     * @param h         the hash of the requested transaction
     *
     * @return the unlock time/height
     */
    virtual uint64_t getTxUnlockTime(const Crypto::Hash &h) const = 0;

    // return tx with hash <h>
    // throw if no such tx exists
    /*!
     * @brief fetches the transaction with the given hash
     *
     * If the transaction does not exist, the subclass should throw TX_DNE.
     *
     * @param h         the hash to look for
     *
     * @return the transaction with the given hash
     */
    virtual CryptoNote::Transaction getTx(const Crypto::Hash &h) const;

    /*!
     * @brief fetches the transaction with the given hash
     *
     * If the transaction does not exist, the subclass should return false.
     *
     * @param h         the hash to look for
     *
     * @return true iff the transaction was found
     */
    virtual bool getTx(const Crypto::Hash &h, CryptoNote::Transaction &tx) const;

    /*!
     * @brief fetches the transaction blob with the given hash
     *
     * The subclass should return the transaction stored which has the given
     * hash.
     *
     * If the transaction does not exist, the subclass should return false.
     *
     * @param h         the hash to look for
     *
     * @return true if the transaction was found
     */
    virtual bool getTxBlob(const Crypto::Hash &h, CryptoNote::blobData &tx) const = 0;

    /*!
     * @brief fetches the total number of transactions ever
     *
     * The subclass should return a count of all the transactions from
     * all blocks.
     *
     * @return the number of transactions in the blockchain
     */
    virtual uint64_t getTxCount() const = 0;

    /*!
     * @brief fetches a list of transactions based on their hashes
     *
     * The subclass should attempt to fetch each transaction referred to by
     * the hashes passed.
     *
     * Currently, if any of the transactions is not in BlockchainDB, the call
     * to getTx in the implementation will throw TX_DNE.
     *
     * TODO: decide if this behavior is correct for missing transactions
     *
     * @param hList         a list of hashes
     *
     * @return the list of transactions
     */
    virtual std::vector<CryptoNote::Transaction> getTxList(const std::vector<Crypto::Hash> &hList) const = 0;

    // returns height of block that contains transaction with hash <h>
    /*!
     * @brief fetches the height of a transaction's block
     *
     * The subclass should attempt to return the height of the block containing
     * the transaction with the given hash.
     *
     * If the transaction cannot be found, the subclass should throw TX_DNE.
     *
     * @param h         the hash of the transaction
     *
     * @return the height of the transaction's block
     */
    virtual uint64_t getTxBlockHeight(const Crypto::Hash &h) const = 0;

    // returns the total number of outputs of amount <amount>
    /*!
     * @brief fetches the number of outputs of a given amount
     *
     * The subclass should return a count of outputs of the given amount,
     * or zero if there are none.
     *
     * TODO: should outputs spent with a low mixin (especially 0) be
     *       excluded from the count?
     *
     * @param amount        the output amount being looked up
     *
     * @return the number of outputs of the given amount
     */
    virtual uint64_t getNumOutputs(const uint64_t &amount) const = 0;

    /*!
     * @brief return index of the first element (should be hidden, but isn't)
     *
     * @return the index
     */
    virtual uint64_t getIndexingBase() const
    {
        return 0;
    }

    /*!
     * @brief get some of an output's data
     *
     * The subclass should return the public key, unlock time, and block height
     * for the output with the given amount and index, collected in a struct.
     *
     * If the output cannot be found, the subclass should throw OUTPUT_DNE.
     *
     * If any of these parts cannot be found, but some are, the subclass
     * should throw DB_ERROR with a message stating as much.
     *
     * @param amount        the output amount
     * @param index         the output's index (indexed by amount)
     *
     * @return the requested output data
     */
    virtual OutputDataT getOutputKey(const uint64_t &amount, const uint32_t &index) = 0;

    /*!
     * @brief get some of an output's data
     *
     * The subclass should return the public key, unlock time, and block height
     * for the output with the given global index, collected in a struct.
     *
     * If the output cannot be found, the subclass should throw OUTPUT_DNE.
     *
     * If any of these parts cannot be found, but some are, the subclass
     * should throw DB_ERROR with a message stating as much.
     *
     * @param globalIndex   the output's index (global)
     *
     * @return the requested output data
     */
    virtual OutputDataT getOutputKey(const uint32_t &globalIndex) const = 0;

    /*!
     * @brief gets an output's tx hash and index
     *
     * The subclass should return the hash of the transaction which created the
     * output with the global index given, as well as its index in that transaction.
     *
     * @param index     an output's global index
     *
     * @return the tx hash and output index
     */
    virtual txOutIndex getOutputTxAndIndexFromGlobal(const uint64_t &index) const = 0;

    /*!
     * @brief gets an output's tx hash and index
     *
     * The subclass should return the hash of the transaction which created the
     * output with the amount and index given, as well as its index in that
     * transaction.
     *
     * @param amount    an output amount
     * @param index     an output's amount-specific index
     *
     * @return the tx hash and output index
     */
    virtual txOutIndex getOutputTxAndIndex(const uint64_t &amount, const uint32_t &index) const = 0;

    /*!
     * @brief gets some outputs' tx hashes and indices
     *
     * This function is a mirror of
     * getOutputTxAndIndex(const uint64_t &amount, const uint64_t &index),
     * but for a list of outputs rather than just one.
     *
     * @param amount        an output amount
     * @param offsets       a list of amount-specific output indices
     * @param indices       return-by-reference a list of tx hashes and output indices (as pairs)
     */
    virtual void getOutputTxAndIndex(const uint64_t &amount,
                                     const std::vector<uint32_t> &offsets,
                                     std::vector<txOutIndex> &indices) const = 0;

    /*!
     * @brief gets outputs' data
     *
     * This function is a mirror of
     * getOutputData(const uint64_t &amount, const uint64_t &index)
     * but for a list of outputs rather than just one.
     *
     * @param amount    an output amount
     * @param offsets   a list of amount-specific output indices
     * @param outputs   return-by-reference a list of outputs' metadata
     */
    virtual void getOutputKey(const uint64_t &amount,
                              const std::vector<uint32_t> &offsets,
                              std::vector<OutputDataT> &outputs,
                              bool allowPartial = false) = 0;

    /*!
     * FIXME: Need to check with git blame and ask what this does to
     *        document it
     */
    virtual bool canThreadBulkIndices() const = 0;

    /*!
     * @brief gets output indices (amount-specific) for a transaction's outputs
     *
     * The subclass should fetch the amount-specific output indices for each
     * output in the transaction with the given ID.
     *
     * If the transaction does not exist, the subclass should throw TX_DNE.
     *
     * If an output cannot be found, the subclass should throw OUTPUT_DNE.
     *
     * @param txId      a transaction ID
     *
     * @return a list of amount-specific output indices
     */
    virtual std::vector<uint64_t> getTxAmountOutputIndices(const uint64_t txId) const = 0;

    /*!
     * @brief check if a key image is stored as spent
     *
     * @param img       the key image to check for
     *
     * @return true if the image is present, otherwise false
     */
    virtual bool hasKeyImage(const Crypto::KeyImage &img) const = 0;

    /*!
     * @brief add a txPool transaction
     *
     * @param details the details of the transaction to add
     */
    virtual void addTxPoolTx(const CryptoNote::Transaction &tx, const TxPoolTxMetaT &details) = 0;

    /*!
     * @brief update a txpool transaction's metadata
     *
     * @param txId      the txid of the transaction to update
     * @param details   the details of the transaction to update
     */
    virtual void updateTxPoolTx(const Crypto::Hash &txId, const TxPoolTxMetaT &details) = 0;

    /*!
     * @brief get the number of transactions in the txPool
     */
    virtual uint64_t getTxPoolTxCount() const = 0;

    /*!
     * @brief check whether a txId is in the txPool
     */
    virtual bool txPoolHasTx(const Crypto::Hash &txId) const = 0;

    /*!
     * @brief remove a txPool transaction
     *
     * @param txId      the transaction id of the transaction to remove
     */
    virtual void removeTxPoolTx(const Crypto::Hash &txId) = 0;

    /*!
     * @brief get a txPool transaction's metadata
     *
     * @param txId      the transaction id of the transaction to lookup
     * @param meta      the metadata to return
     *
     * @return true if the tx meta was found, false otherwise
     */
    virtual bool getTxPoolTxMeta(const Crypto::Hash &txId, TxPoolTxMetaT &meta) const = 0;

    /*!
     * @brief get a txPool transaction's blob
     *
     * @param txId      the transaction id of the transaction to lookup
     * @param bD        the blob to return
     *
     * @return true if the txId was in the txPool, false otherwise
     */
    virtual bool getTxPoolTxBlob(const Crypto::Hash &txId, CryptoNote::blobData &bD) const = 0;

    /*!
     * @brief get a txPool transaction's blob
     *
     * @param txId      the transaction id of the transaction to lookup
     *
     * @return the blob for that transaction
     */
    virtual CryptoNote::blobData getTxPoolTxBlob(const Crypto::Hash &txId) const = 0;

    /*!
     * @brief runs a function over all txPool transactions
     *
     * The subclass should run the passed function for each txPool tx it has
     * stored, passing the tx id and metadata as its parameters.
     *
     * If any call to the function returns false, the subclass should return
     * false.  Otherwise, the subclass returns true.
     *
     * @param std::function fn  the function to run
     *
     * @return false if the function returns false for any transaction, otherwise true
     */
    virtual bool forAllTxPoolTxes(std::function<bool(const Crypto::Hash &,
                                                     const TxPoolTxMetaT &,
                                                     const CryptoNote::blobData *)>,
                                  bool includeBlob = false,
                                  bool includeUnrelayedTxes = true) const = 0;

    /*!
     * @brief runs a function over all key images stored
     *
     * The subclass should run the passed function for each key image it has
     * stored, passing the key image as its parameter.
     *
     * If any call to the function returns false, the subclass should return
     * false.  Otherwise, the subclass returns true.
     *
     * @param std::function fn  the function to run
     *
     * @return false if the function returns false for any key image, otherwise true
     */
    virtual bool forAllKeyImages(std::function<bool(const Crypto::KeyImage &)>) const = 0;

    /*!
     * @brief runs a function over a range of blocks
     *
     * The subclass should run the passed function for each block in the
     * specified range, passing (blockHeight, blockHash, block) as its parameters.
     *
     * If any call to the function returns false, the subclass should return
     * false.  Otherwise, the subclass returns true.
     *
     * The subclass should throw DB_ERROR if any of the expected values are
     * not found.  Current implementations simply return false.
     *
     * @param h1                the start height
     * @param h2                the end height
     * @param std::function fn  the function to run
     *
     * @return false if the function returns false for any block, otherwise true
     */
    virtual bool forBlocksRange(const uint64_t &h1,
                                const uint64_t &h2,
    std::function<bool(uint64_t,
    const Crypto::Hash &,
    const CryptoNote::Block &)>) const = 0;

    /*!
     * @brief runs a function over all transactions stored
     *
     * The subclass should run the passed function for each transaction it has
     * stored, passing (transactionHash, transaction) as its parameters.
     *
     * If any call to the function returns false, the subclass should return
     * false.  Otherwise, the subclass returns true.
     *
     * The subclass should throw DB_ERROR if any of the expected values are
     * not found.  Current implementations simply return false.
     *
     * @param std::function fn  the function to run
     *
     * @return false if the function returns false for any transaction, otherwise true
     */
    virtual bool forAllTransactions(std::function<bool(const Crypto::Hash &,
                                                       const CryptoNote::Transaction &)>) const = 0;

    /*!
     * @brief runs a function over all outputs stored
     *
     * The subclass should run the passed function for each output it has
     * stored, passing (amount, transaction_hash, tx_local_output_index)
     * as its parameters.
     *
     * If any call to the function returns false, the subclass should return
     * false.  Otherwise, the subclass returns true.
     *
     * The subclass should throw DB_ERROR if any of the expected values are
     * not found.  Current implementations simply return false.
     *
     * @param std::function f   the function to run
     *
     * @return false if the function returns false for any output, otherwise true
     */
    virtual bool forAllOutputs(std::function<bool(uint64_t amount,
    const Crypto::Hash &txHash,
            uint64_t height,
    size_t txIdx)> f) const = 0;
    virtual bool forAllOutputs(uint64_t amount,
            const std::function<bool(uint64_t height)> &f) const = 0;

    //
    // Hard fork related storage
    //

    /*!
     * @brief sets which hardfork version a height is on
     *
     * @param height        the height
     * @param version       the version
     */
    virtual void setHardForkVersion(uint64_t height, uint8_t version) = 0;

    /*!
     * @brief checks which hardfork version a height is on
     *
     * @param height    the height
     *
     * @return the version
     */
    virtual uint8_t getHardForkVersion(uint64_t height) const = 0;

    /*!
     * @brief verify hard fork info in database
     */
    virtual void checkHardForkInfo() = 0;

    /*!
     * @brief delete hard fork info from database
     */
    virtual void dropHardForkInfo() = 0;

    /*!
     * @brief is BlockchainDB in read-only mode?
     *
     * @return true if in read-only mode, otherwise false
     */
    virtual bool isReadOnly() const = 0;

    // TODO: this should perhaps be (or call) a series of functions which
    //       progressively update through version updates
    /*!
     * @brief fix up anything that may be wrong due to past bugs
     */
    virtual void fixup();

    /*!
     * @brief set whether or not to automatically remove logs
     *
     * This function is only relevant for one implementation (BlockchainBDB), but
     * is here to keep BlockchainDB users implementation-agnostic.
     *
     * @param autoRemove whether or not to auto-remove logs
     */
    void setAutoRemoveLogs(bool autoRemove)
    {
        mAutoRemoveLogs = autoRemove;
    }


    bool mOpen; //!< Whether or not the BlockchainDB is open/ready for use
    mutable std::recursive_mutex mSynchronizationLock;  //!< A lock, currently for when BlockchainLMDB needs to resize the backing db file
    /*!
     * @brief helper function to remove transaction from the blockchain
     *
     * This function encapsulates aspects of removing a transaction.
     *
     * @param txHash the hash of the transaction to be removed
     */
    void removeTransaction(const Crypto::Hash &txHash);
}; // class BlockchainDB

BlockchainDB *newDB(const std::string &DBType);
} // namespace CryptoNote
