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

#include <Lmdb/LMDBExceptions.h>
#include <Lmdb/Structures.h>

#include <Logging/LoggerRef.h>

#include <Serialization/BinaryInputStreamSerializer.h>
#include <Serialization/BinaryOutputStreamSerializer.h>

/**
 * @file QwertyNote Blockchain Database Interface
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
#define DBF_SAFE 1
#define DBF_FAST 2
#define DBF_FASTEST 4
#define DBF_RDONLY 8
#define DBF_SALVAGE 0x10

/**
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
        /**
         * @brief add the block and metadata to the db
         *
         * The subclass implementing this will add the specified block and
         * block metadata to its backing store.  This does not include its
         * transactions, those are added in a separate step.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         *
         * @param block the block to be added
         * @param blockSize the size of the block (transactions and all)
         * @param cumulativeDifficulty the accumulated difficulty after this block
         * @param coinsGenerated the number of coins generated total after this block
         * @param blockHash the hash of the block
         */
        virtual void addBlock(const CryptoNote::Block &block, const size_t &blockSize,
                              const CryptoNote::difficulty_type &cumulativeDifficulty,
                              const uint64_t &coinsGenerated, const Crypto::Hash &blockHash) = 0;

        /**
         * @brief remove data about the top block
         *
         * The subclass implementing this will remove the block data from the top
         * block in the chain. The data to be removed is that which was added in
         * BlockchainDB::addBlock(CryptoNote::const Block& block,
         *                        const size_t& blockSize,
         *                        const CryptoNote::difficulty_type& cumulativeDifficulty,
         *                        const uint64_t& coinsGenerated,
         *                        const Crypto::Hash& blockHash)
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         */
        virtual void removeBlock() = 0;

        /**
         * @brief Store the transaction and its metadata
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
         * @param sBlockHash
         * @param sTransaction
         * @param sTxHash
         * @return
         */
        virtual uint64_t addTransactionData(const Crypto::Hash &sBlockHash,
                                            const CryptoNote::Transaction &sTransaction,
                                            const Crypto::Hash &sTxHash) = 0;

        /**
         * @brief Remove data about a transaction
         *
         * The subclass implementing this will remove the transaction data
         * for the passed transaction.  The data to be removed was added in
         * add_transaction_data().  Additionally, current subclasses have behavior
         * which requires the transaction itself as a parameter here.  Future
         * implementations should note that this parameter is subject to be removed
         * at a later time.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         * @param sTxHash
         * @param tx
         */
        virtual void removeTransactionData(const Crypto::Hash &sTxHash,
                                           const CryptoNote::Transaction &tx) = 0;

        /**
         * @brief Store an output
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
         * reverse the process is the tx_out.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         *
         * @param sTxHash 		Hash of the transaction the output was created by
         * @param sTxOutput		The output
         * @param uIndex		Index of the output in its transaction
         * @param uUnlockTime	Unlock time/height of the output
         *
         * @return				Amount output index
         */
        virtual uint64_t addOutput(const Crypto::Hash &sTxHash,
                                   const CryptoNote::TransactionOutput &sTxOutput,
                                   const uint64_t &uIndex, const uint64_t &uUnlockTime) = 0;

        /**
         * @brief Store amount output indices for a tx's outputs
         *
         * The subclass implementing this will add the amount output indices to its
         * backing store in a suitable manner. The tx_id will be the same one that
         * was returned from #add_output().
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         * @param uTxId			ID of the transaction containing these outputs
         * @param vAmountOutputIndices The amount output indices of the transaction
         */
        virtual void
        addTransactionAmountOutputIndices(const uint64_t uTxId,
                                          const std::vector<uint64_t> &vAmountOutputIndices) = 0;

        /**
         *@brief Store a spent key
         *
         * The subclass implementing this will store the spent key image.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         *
         * @param sSpentKeyImage	The spent key image to store
         */
        virtual void addSpentKey(const Crypto::KeyImage &sSpentKeyImage) = 0;

        /**
         *@brief Remove a spent key
         *
         * The subclass implementing this will remove the spent key image.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         *
         * @param sSpentKeyImage	The spent key image to remove
         */
        virtual void removeSpentKey(const Crypto::KeyImage &sSpentKeyImage) = 0;

        /**
         * @brief The private version of popBlock, for undoing if an add_block fails
         *
         * This function simply calls popBlock(CryptoNote::Block &sBlock,
                                               std::vector<CryptoNote::Transaction> &sTransactions);
         * with dummy parameters, as the returns-by-reference can be discarded.
         */
        void popBlock();

        /**
         * @brief helper function to remove transaction from the blockchain
         *
         * This function encapsulates aspects of removing a transaction.
         *
         * @param tx_hash the hash of the transaction to be removed
         */
        void removeTransaction(const Crypto::Hash &sTxHash);

        uint64_t mNumCalls = 0;
        uint64_t mTimeBlockHash = 0;
        uint64_t mTimeAddBlock = 0;
        uint64_t mTimeAddTransaction = 0;

    protected:
        /**
         * @brief helper function for add_transactions, to add each individual transaction
         *
         * This function is called by add_transactions() for each transaction to be
         * added.
         *
         * @param sBlockHash        Hash of the block which has the transaction
         * @param sTransactions     The transaction to add
         * @param sTxHashPtr        The hash of the transaction, if already calculated
         */
        void addTransaction(const Crypto::Hash &sBlockHash,
                            const CryptoNote::Transaction &sTransactions,
                            const Crypto::Hash *sTxHashPtr = NULL);

        mutable uint64_t gTimeTxExists = 0;
        uint64_t gTimeCommit = 0;
        bool gAutoRemoveLogs = true;

    public:
        friend class BlockchainLMDB;

        BlockchainDB() : pOpen(false), pIsResizing(false) {}

        /**
         * @brief An empty destructor.
         */
        virtual ~BlockchainDB() {};

        /**
         * @brief Reset profiling stats
         */
        virtual void resetStats() = 0;

        /**
         * @brief Show profiling stats
         *
         * This function prints current performance/profiling data to whichever
         * log file(s) are set up (possibly including stdout or stderr)
         */
        virtual void showStats() = 0;

        /**
         * @brief open a db, or create it if necessary.
         *
         * This function opens an existing database or creates it if it
         * does not exist.
         *
         * The subclass implementing this will handle all file opening/creation,
         * and is responsible for maintaining its state.
         *
         * The parameter <cFileName> may not refer to a file name, necessarily, but
         * could be an IP:PORT for a database which needs it, and so on.  Calling it
         * <cFileName> is convenient and should be descriptive enough, however.
         *
         * For now, iDBFlags are
         * specific to the subclass being instantiated.  This is subject to change,
         * and the iDBFlags parameter may be deprecated.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         *
         * @param cFileName a string referring to the BlockchainDB to open
         * @param iDBFlags flags relevant to how to open/use the BlockchainDB
         */
        virtual void open(const std::string &cFileName, const int iDBFlags = 0) = 0;

        /**
         * @brief Gets the current open/ready state of the BlockchainDB
         *
         * @return true if open/ready, otherwise false
         */
        bool isOpen() const;

        /**
         * @brief Gets the current resizing/ready state of the BlockchainDB
         *
         * @return true if resizing/ready, otherwise false
         */
        bool isResizing() const;

        /**
         * @brief Close the BlockchainDB
         *
         * At minimum, this call ensures that further use of the BlockchainDB
         * instance will not have effect.  In any case where it is necessary
         * to do so, a subclass implementing this will sync with disk.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         */
        virtual void close() = 0;

        /**
         * @brief Sync the BlockchainDB with disk
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

        /**
         * @brief Toggle safe syncs for the DB
         *
         * Used to switch DBF_SAFE on or off after starting up with DBF_FAST.
         */
        virtual void safeSyncMode(const bool bOnOff) = 0;

        /**
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

        /**
         * @brief Gets the name of the folder the BlockchainDB's file(s) should be in
         *
         * The subclass implementation should return the name of the folder in which
         * it stores files, or an empty string if there is none.
         *
         * @return The name of the folder with the BlockchainDB's files, if any.
         */
        virtual std::string getDBName() const = 0;

        /**
         * @brief Acquires the BlockchainDB lock
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
         * @return True, unless at a future time false makes sense (timeout, etc)
         */
        virtual bool lock() = 0;

        /**
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

        /**
         * @brief Tells the BlockchainDB to start a new "batch" of blocks
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
         * @param uBatchNumBlocks   Number of blocks to batch together
         * @param uBatchBytes       Number of bytes from the batch
         *
         * @return True if we started the batch, false if already started
         */
        virtual bool batchStart(uint64_t uBatchNumBlocks = 0, uint64_t uBatchBytes = 0) = 0;

        /**
         * @brief Ends a batch transaction
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

        /**
         * @brief Sets whether or not to batch transactions
         *
         * If the subclass implements batching, this function tells it to begin
         * batching automatically.
         *
         * If the subclass implements batching and has a batch in-progress, a
         * parameter of false should disable batching and call batch_stop() to
         * store the current batch.
         *
         * If any of this cannot be done, the subclass should throw the corresponding
         * subclass of DB_EXCEPTION
         *
         * @param Bool batch whether or not to use batch transactions.
         */
        virtual void setBatchTransactions(bool) = 0;

        virtual void blockTxnStart(bool bReadOnly = false) = 0;

        virtual void blockTxnStop() = 0;

        virtual void blockTxnAbort() = 0;

        /**
         * @brief Handles the addition of a new block to BlockchainDB
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
         * @param block                 The block to be added
         * @param uBlockSize            The size of the block (transactions and all)
         * @param uCumulativeDifficulty The accumulated difficulty after this block
         * @param uCoinsGenerated       The number of coins generated total after this block
         * @param transactions          The transactions in the block
         *
         * @return The height of the chain post-addition
         */
        virtual uint64_t addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                                  const CryptoNote::difficulty_type &uCumulativeDifficulty,
                                  const uint64_t &uCoinsGenerated,
                                  const std::vector<CryptoNote::Transaction> &transactions);

        /**
         * @brief Checks if a block exists
         *
         * @param sHash     The hash of the requested block
         * @param uHeight   If non NULL, returns the block's height if found
         *
         * @return True if the block exists, otherwise false
         */
        virtual bool blockExists(const Crypto::Hash &sHash, uint64_t *uHeight = NULL) const = 0;

        /**
         * @brief Fetches the block with the given hash
         *
         * The subclass should return the requested block.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param sHash The hash to look for
         *
         * @return The block requested
         */
        virtual CryptoNote::blobData getBlockBlob(const Crypto::Hash &sHash) const = 0;

        /**
         * @brief Fetches the block with the given hash
         *
         * Returns the requested block.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param sHash The hash to look for
         *
         * @return The block requested
         */
        virtual CryptoNote::Block getBlock(const Crypto::Hash &sHash) const;

        /**
         * @brief Gets the height of the block with a given hash
         *
         * The subclass should return the requested height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param sHash The hash to look for
         *
         * @return The height
         */
        virtual uint64_t getBlockHeight(const Crypto::Hash &sHash) const = 0;

        /**
         * @brief Fetch a block header
         *
         * The subclass should return the block header from the block with
         * the given hash.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param sHash The hash to look for
         *
         * @return The block header
         */
        virtual CryptoNote::BlockHeader getBlockHeader(const Crypto::Hash &sHash) const = 0;

        /**
         * @brief Fetch a block blob by height
         *
         * The subclass should return the block at the given height.
         *
         * If the block does not exist, that is to say if the blockchain is not
         * that high, then the subclass should throw BLOCK_DNE
         *
         * @param uHeight The height to look for
         * @return the block blob
         */
        virtual CryptoNote::blobData getBlockBlobFromHeight(const uint64_t &uHeight) const = 0;

        /**
         * @brief Fetch a block by height
         *
         * If the block does not exist, that is to say if the blockchain is not
         * that high, then the subclass should throw BLOCK_DNE
         *
         * @param uHeight The height to look for
         * @return The Block
         */
        virtual CryptoNote::Block getBlockFromHeight(const uint64_t &uHeight) const;

        /**
         * @brief fetch a block's timestamp
         *
         * The subclass should return the timestamp of the block with the
         * given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param uHeight The height requested
         *
         * @return The timestamp
         */
        virtual uint64_t getBlockTimestamp(const uint64_t &uHeight) const = 0;

        /**
         * @brief Fetch the top block's timestamp
         *
         * The subclass should return the timestamp of the block with the
         * given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @return The timestamp
         */
        virtual uint64_t getTopBlockTimestamp() const = 0;

        /**
         * @brief Fetch a block's size
         *
         * The subclass should return the size of the block with the
         * given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param uHeight The height requested
         *
         * @return The size
         */
        virtual size_t getBlockSize(const uint64_t &uHeight) const = 0;

        /**
         * @brief Fetch a block's cumulative difficulty
         *
         * The subclass should return the difficulty of the block with the
         * given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param uHeight
         * @return
         */
        virtual CryptoNote::difficulty_type
        getBlockCumulativeDifficulty(const uint64_t &uHeight) const = 0;

        /**
         * @brief Fetch a block's difficulty
         *
         * The subclass should return the difficulty of the block with the
         * given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param uHeight The requested height
         * @return The difficulty
         */
        virtual CryptoNote::difficulty_type getBlockDifficulty(const uint64_t &uHeight) const = 0;

        /**
         * @brief Fetch a block's already generated coins
         *
         * The subclass should return the total coins generated as of the block
         * with the given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param uHeight The height requested
         * @return The amount of already generated coins
         */
        virtual uint64_t getBlockAlreadyGeneratedCoins(const uint64_t &uHeight) const = 0;

        /**
         * @brief fetch a block's hash
         *
         * The subclass should return hash of the block with the
         * given height.
         *
         * If the block does not exist, the subclass should throw BLOCK_DNE
         *
         * @param uHeight The requested height
         *
         * @return The hash
         */
        virtual Crypto::Hash getBlockHashFromHeight(const uint64_t &uHeight) const = 0;

        /**
         * @brief Fetch a list of blocks
         *
         * The subclass should return a vector of blocks with heights starting at
         * h1 and ending at h2, inclusively.
         *
         * If the height range requested goes past the end of the blockchain,
         * the subclass should throw BLOCK_DNE.  (current implementations simply
         * don't catch this exception as thrown by methods called within)
         *
         * @param uStartHeight  The start height
         * @param uEndHeight    The end height
         *
         * @return A vector of blocks
         */
        virtual std::vector<CryptoNote::Block> getBlocksRange(const uint64_t &uStartHeight,
                                                              const uint64_t &uEndHeight) = 0;

        /**
         * @brief Fetch a list of block hashes
         *
         * The subclass should return a vector of block hashes from blocks with
         * heights starting at h1 and ending at h2, inclusively.
         *
         * If the height range requested goes past the end of the blockchain,
         * the subclass should throw BLOCK_DNE.  (current implementations simply
         * don't catch this exception as thrown by methods called within)
         *
         * @param uStartHeight  The start height
         * @param uEndHeight    The end height
         *
         * @return A vector of block hashes
         */
        virtual std::vector<Crypto::Hash> getHashesRange(const uint64_t &uStartHeight,
                                                         const uint64_t &uEndHeight) = 0;

        /**
         * @brief Fetch the top block's hash
         *
         * The subclass should return the hash of the most recent block
         *
         * @return The top block's hash
         */
        virtual Crypto::Hash getTopBlockHash() const = 0;

        /**
         * @brief Fetch the top block
         *
         * The subclass should return most recent block
         *
         * @return The top block
         */
        virtual CryptoNote::Block getTopBlock() const = 0;

        /**
         * @brief fetch the current blockchain height
         *
         * The subclass should return the current blockchain height
         *
         * @return the current blockchain height
         */
        virtual uint64_t height() const = 0;

        /**
         * <!--
         * TODO: Rewrite (if necessary) such that all calls to remove_* are
         *       done in concrete members of this base class.
         * -->
         *
         * @brief pops the top block off the blockchain
         *
         * The subclass should remove the most recent block from the blockchain,
         * along with all transactions, outputs, and other metadata created as
         * a result of its addition to the blockchain.  Most of this is handled
         * by the concrete members of the base class provided the subclass correctly
         * implements remove_* functions.
         *
         * The subclass should return by reference the popped block and
         * its associated transactions
         *
         * @param blk return-by-reference the block which was popped
         * @param txs return-by-reference the transactions from the popped block
         */
        virtual void popBlock(CryptoNote::Block &sBlock, std::vector<CryptoNote::Transaction> &vTransactions);

        /**
         * @brief Check if a transaction with a given hash exists
         *
         * The subclass should check if a transaction is stored which has the
         * given hash and return true if so, false otherwise.
         * @param sHash The Hash to check against
         * @param uTransactionId (optional) Returns the Transaction ID for the transaction hash
         *
         * @return true if the transaction exists, otherwise false
         */
        virtual bool transactionExists(const Crypto::Hash &sHash) const = 0;

        virtual bool transactionExists(const Crypto::Hash &sHash, uint64_t &uTransactionId) const = 0;

        /**
         * @brief Fetches the transaction with the given hash
         *
         * If the transaction does not exist, the subclass should throw TX_DNE.
         *
         * @param sHash The hash to look for
         * @return The transaction with the given hash
         */
        virtual CryptoNote::Transaction getTransaction(const Crypto::Hash &sHash) const;

        /**
         * @brief Fetches the transaction with the given hash
         *
         * If the transaction does not exist, the subclass should return false.
         *
         * @param sHash The hash to look for
         * @param sTransaction
         *
         * @return True iff the transaction was found
         */
        virtual bool getTransaction(const Crypto::Hash &sHash,
                                    CryptoNote::Transaction &sTransaction) const;

        /**
         * @brief Fetches the transaction blob with the given hash
         *
         * The subclass should return the transaction stored which has the given
         * hash.
         *
         * If the transaction does not exist, the subclass should return false.
         *
         * @param sHash The hash to look for
         * @param sTransactionBlob
         *
         * @return True iff the transaction was found
         */
        virtual bool getTransactionBlob(const Crypto::Hash &sHash,
                                        CryptoNote::blobData &sTransactionBlob) const = 0;

        /**
         * @brief Fetches the total number of transactions ever
         *
         * The subclass should return a count of all the transactions from
         * all blocks.
         *
         * @return the number of transactions in the blockchain
         * @return
         */
        virtual uint64_t getTransactionCount() const = 0;

        /**
         * @brief Fetches a list of transactions based on their hashes
         *
         * The subclass should attempt to fetch each transaction referred to by
         * the hashes passed.
         *
         * Currently, if any of the transactions is not in BlockchainDB, the call
         * to get_tx in the implementation will throw TX_DNE.
         *
         * <!-- TODO: decide if this behavior is correct for missing transactions -->
         *
         * @param vHashList A list of hashes
         *
         * @return The list of transactions
         */
        virtual std::vector<CryptoNote::Transaction>
        getTransactionList(const std::vector<Crypto::Hash> &vHashList) const = 0;

        /**
         * @brief Is BlockchainDB in read-only mode?
         * @return True if in read-only mode, otherwise false
         */
        virtual bool isReadOnly() const = 0;

        // TODO: this should perhaps be (or call) a series of functions which progressively update
        // through version updates
        /**
         * @brief Fix up anything that may be wrong due to past bugs
         */
        virtual void fixUp();

        bool pOpen; // Whether or not the BlockchainDB is open/ready for use
        bool pIsResizing; // Whether or not the BlockchainDB is resizing/ready for use
        mutable std::recursive_mutex pSyncronizationLock; // A lock, currently for when BlockchainLMDB
        // needs to resize the backing db file
    };

    BlockchainDB *newDB(const std::string &cDBType, Logging::ILogger &logger);
} // namespace CryptoNote
