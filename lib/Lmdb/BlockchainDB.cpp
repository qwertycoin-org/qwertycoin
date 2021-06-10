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

#include <boost/range/adaptor/reversed.hpp>

#include <Common/StringTools.h>

#include <CryptoNoteCore/CryptoNoteTools.h>

#include <Lmdb/BlockchainDB.h>
#include <Lmdb/DBLMDB.h>

namespace CryptoNote {
    BlockchainDB *newDB(const std::string &cDBType, Logging::ILogger &logger)
    {
        if (cDBType == "lmdb") {
            return new BlockchainLMDB(true, logger);
        }
        else {
            return NULL;
        }
    }

    void BlockchainDB::popBlock()
    {
        CryptoNote::Block sBlock;
        std::vector<CryptoNote::Transaction> vTransactions;

        popBlock(sBlock, vTransactions);
    }

    void BlockchainDB::addTransaction(const Crypto::Hash &sBlockHash,
                                      const CryptoNote::Transaction &sTransaction,
                                      const Crypto::Hash *sTxHashPtr)
    {
        bool bMinerTx = false;
        Crypto::Hash sTxHash;

        if (!sTxHashPtr) {
            // should only need to compute hash for miner Transactions
            sTxHash = getObjectHash(sTransaction);
        }
        else {
            sTxHash = *sTxHashPtr;
        }

        std::vector<uint64_t> vAmountOutputIndices;

        for (uint16_t o = 0; o < sTransaction.outputs.size(); o++) {
            const auto &out = sTransaction.outputs[o];
            if (out.target.type() == typeid(KeyOutput)) {
                vAmountOutputIndices.push_back(addOutput(sTxHash, out, o, sTransaction.unlockTime));
            }
            else if (out.target.type() == typeid(MultisignatureOutput)) {
                vAmountOutputIndices.push_back(addOutput(sTxHash, out, o, sTransaction.unlockTime));
            }
        }

        for (auto &in : sTransaction.inputs) {
            if (in.type() == typeid(KeyInput)) {
                addSpentKey(::boost::get<KeyInput>(in).keyImage);
            }
            else if (in.type() == typeid(MultisignatureInput)) {
                // ::boost::get<MultisignatureInput>(in).isUsed = true;
            }
            else if (in.type() == typeid(CryptoNote::BaseInput)) {
                bMinerTx = true;
                /* in_gen */
            }
        }

        uint64_t uTxId = addTransactionData(sBlockHash, sTransaction, sTxHash);
        addTransactionAmountOutputIndices(uTxId, vAmountOutputIndices);
    }

    uint64_t BlockchainDB::addBlock(const CryptoNote::Block &block, const size_t &uBlockSize,
                                    const CryptoNote::difficulty_type &uCumulativeDifficulty,
                                    const uint64_t &uCoinsGenerated,
                                    const std::vector<CryptoNote::Transaction> &transactions)
    {
        blockTxnStart(false);

        Crypto::Hash sBlockHash = get_block_hash(block);
        uint64_t uPrevheight = height();

        addTransaction(sBlockHash, block.baseTransaction);
        int iTxI = 0;
        Crypto::Hash sHash = NULL_HASH;
        for (const auto &transaction : transactions) {
            sHash = block.transactionHashes[iTxI];
            const Crypto::Hash sTxHash = sHash;
            addTransaction(sBlockHash, transaction, &sTxHash);
            ++iTxI;
        }

        addBlock(block, uBlockSize, uCumulativeDifficulty, uCoinsGenerated, sBlockHash);

        blockTxnStop();

        ++mNumCalls;

        return uPrevheight;
    }

    void BlockchainDB::popBlock(CryptoNote::Block &sBlock,
                                std::vector<CryptoNote::Transaction> &vTransactions)
    {
        sBlock = getTopBlock();
        std::cout << "BlockchainDB::" << __func__ << ". Before removeBlock." << std::endl;

        for (const auto &sHash : boost::adaptors::reverse(sBlock.transactionHashes)) {
            CryptoNote::Transaction sTransaction;
            if (!getTransaction(sHash, sTransaction)) {
                throw DB_ERROR("Failed to get transaction from the db");
            }

            vTransactions.push_back(std::move(sTransaction));
            removeTransaction(sHash);
        }

        std::cout << "BlockchainDB::" << __func__ << ". Before for." << std::endl;

        // TODO: Add baseTx removing
        removeTransaction(getObjectHash(sBlock.baseTransaction));
        removeBlock();
    }

    void BlockchainDB::removeTransaction(const Crypto::Hash &sTxHash)
    {
        std::cout << "BlockchainDB::" << __func__ << ". Before getTransaction." << std::endl;
        CryptoNote::Transaction sTransaction = getTransaction(sTxHash);
        std::cout << "BlockchainDB::" << __func__ << ". After removeTransaction." << std::endl;

        for (const CryptoNote::TransactionInput &sTxIn : sTransaction.inputs) {
            if (sTxIn.type() == typeid(CryptoNote::KeyInput)) {
                removeSpentKey(boost::get<CryptoNote::KeyInput>(sTxIn).keyImage);
            }
        }

        removeTransactionData(sTxHash, sTransaction);
    }

    bool BlockchainDB::isOpen() const
    {
        return pOpen;
    }

    bool BlockchainDB::isResizing() const
    {
        return pIsResizing;
    }

    CryptoNote::Block BlockchainDB::getBlockFromHeight(const uint64_t &uHeight) const
    {
        CryptoNote::blobData sBlobData = getBlockBlobFromHeight(uHeight);
        CryptoNote::Block sBlock;
        if (!parseAndValidateBlockFromBlob(sBlobData, sBlock)) {
            throw (DB_ERROR("Failed to parse block from blob retrieved from the db"));
        }

        return sBlock;
    }

    CryptoNote::Block BlockchainDB::getBlock(const Crypto::Hash &sHash) const
    {
        CryptoNote::blobData sBlobData = getBlockBlob(sHash);
        CryptoNote::Block sBlock;
        if (!parseAndValidateBlockFromBlob(sBlobData, sBlock)) {
            throw (DB_ERROR("Failed to parse block from blob retrieved from the db"));
        }

        return sBlock;
    }

    bool BlockchainDB::getTransaction(const Crypto::Hash &sHash,
                                      CryptoNote::Transaction &sTransaction) const
    {
        CryptoNote::blobData sBlobData;
        if (!getTransactionBlob(sHash, sBlobData)) {
            return false;
        }

        if (!parseAndValidateTransactionFromBlob(sBlobData, sTransaction)) {
            throw (DB_ERROR("Failed to parse transaction from blob retrieved from the db"));
        }

        return true;
    }

    CryptoNote::Transaction BlockchainDB::getTransaction(const Crypto::Hash &sHash) const
    {
        CryptoNote::Transaction sTransaction;
        if (!getTransaction(sHash, sTransaction)) {
            throw (TX_DNE(std::string("tx with hash ")
                                  .append(Common::podToHex(sHash))
                                  .append(" not found in db")
                                  .c_str()));
        }

        return sTransaction;
    }
/*

    void BlockchainDB::resetStats()
    {
        mNumCalls = 0;
        mTimeBlockHash = 0;
        gTimeTxExists = 0;
        mTimeAddBlock = 0;
        mTimeAddTransaction = 0;
        gTimeCommit = 0;
    }

    void BlockchainDB::showStats()
    {
        std::cout << std::endl
                  << "*********************************" << std::endl
                  << "mNumCalls: " << mNumCalls << std::endl
                  << "time_blk_hash: " << mTimeBlockHash << "ms" << std::endl
                  << "time_tx_exists: " << gTimeTxExists << "ms" << std::endl
                  << "time_add_block1: " << mTimeAddBlock << "ms" << std::endl
                  << "time_add_transaction: " << mTimeAddTransaction << "ms" << std::endl
                  << "time_commit1: " << gTimeCommit << "ms" << std::endl
                  << "*********************************" << std::endl;
    }
*/

    void BlockchainDB::fixUp()
    {
        if (isReadOnly()) {
            return;
        }

        setBatchTransactions(true);
        batchStart();
        batchStop();
    }
}