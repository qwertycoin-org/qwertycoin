// Copyright (c) 2014-2019, The Monero Project
//
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

#include <iostream>

#include <boost/range/adaptor/reversed.hpp>

#include <Common/StringTools.h>

#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/Currency.h>
#include <CryptoNoteCore/LMDB/BinaryArrayDataType.h>
#include <CryptoNoteCore/LMDB/BlockchainDB.h>
#include <CryptoNoteCore/LMDB/DatabaseLMDB.h>

using Common::podToHex;

static const char *DBTypes[]
        {
                "lmdb",
                NULL
        };

namespace CryptoNote {
BlockchainDB *newDB(const std::string &DBType)
{
    if (DBType == "lmdb") {
        return new BlockchainLMDB();
    } else {
        return NULL;
    }
}

void BlockchainDB::popBlock()
{
    CryptoNote::Block block;
    std::vector<CryptoNote::Transaction> txs;
    popBlock(block, txs);
}

void BlockchainDB::addTransaction(const Crypto::Hash &blockHash,
                                  const CryptoNote::Transaction &tx,
                                  const Crypto::Hash *txHashPtr)
{
    bool minerTx = false;
    Crypto::Hash txHash;
    if (!txHashPtr) {
        /*!
         * should only need to compute hash for miner Transactions
         */
        txHash = getObjectHash(tx);
    } else {
        txHash = *txHashPtr;
    }

    std::vector<uint64_t> amountOutputIndices;

    for (uint16_t o = 0; o < tx.outputs.size(); o++) {
        const auto &out = tx.outputs[o];
        if (out.target.type() == typeid (KeyOutput)) {
            amountOutputIndices.push_back(addOutput (txHash, out, o, tx.unlockTime));
        } else if (out.target.type() == typeid (MultisignatureOutput)) {
            amountOutputIndices.push_back (addOutput (txHash, out, o, tx.unlockTime));
        }
    }

    for (auto &in : tx.inputs) {
        if (in.type() == typeid (KeyInput)) {
            addSpentKey (::boost::get<KeyInput>(in).keyImage);
        } else if (in.type() == typeid (MultisignatureInput)) {
            // ::boost::get<MultisignatureInput>(in).isUsed = true;
        } else if (in.type() == typeid (CryptoNote::BaseInput)) {
            minerTx = true;
        }
    }

    uint64_t txId = addTransactionData (blockHash, tx, txHash);

    addTxAmountOutputIndices (txId, amountOutputIndices);
}

uint64_t BlockchainDB::addBlock(const CryptoNote::Block &block,
                                const size_t &blockSize,
                                const uint64_t &cumulativeDifficulty,
                                const uint64_t &coinsGenerated,
                                const std::vector<CryptoNote::Transaction> &txs)
{
    blockTxnStart(false);
    Crypto::Hash blkHash = get_block_hash (block);

    uint64_t prevHeight = height ();

    /*!
     * call out to add the transactions
     */
    addTransaction(blkHash, block.baseTransaction);
    int txI = 0;
    Crypto::Hash mHash = NULL_HASH;

    for (const auto &tx : txs) {
        mHash = block.transactionHashes[txI];
        const Crypto::Hash txHash = mHash;
        addTransaction (blkHash, tx, &txHash);
        ++txI;
    }

    /*!
     * call out to subclass implementation to add the block & metadata
     */
    addBlock(block, blockSize, cumulativeDifficulty, coinsGenerated, blkHash);

    mHardfork->add(block, prevHeight);

    blockTxnStop ();

    ++numCalls;

    return prevHeight;
}

void BlockchainDB::setHardFork(Hardfork *hf)
{
    mHardfork = hf;
}

void BlockchainDB::doResize()
{
    doResize();
}

void BlockchainDB::popBlock(CryptoNote::Block &blk, std::vector<CryptoNote::Transaction> &txs)
{
    blk = getTopBlock ();
    removeBlock ();

    for (const auto &h : boost::adaptors::reverse(blk.transactionHashes)) {
        txs.push_back(getTx (h));
        removeTransaction (h);
    }

    removeTransaction (getObjectHash (blk.baseTransaction));
}

bool BlockchainDB::isOpen() const
{
    return mOpen;
}

void BlockchainDB::removeTransaction(const Crypto::Hash &txHash)
{
    CryptoNote::Transaction tx = getTx (txHash);

    for (const CryptoNote::TransactionInput &txInput : tx.inputs) {
        if (txInput.type() == typeid (CryptoNote::KeyInput())) {
            removeSpentKey (boost::get<CryptoNote::KeyInput>(txInput).keyImage);
        }
    }

    /*!
     * need tx as tx.vout has the tx outputs, and the output amounts are needed
     */
    removeTransactionData (txHash, tx);
}

CryptoNote::Block BlockchainDB::getBlockFromHeight(const uint64_t &height) const
{
    CryptoNote::blobData bD = getBlockBlobFromHeight (height);
    CryptoNote::Block bT;
    if (!parseAndValidateBlockFromBlob(bD, bT)) {
        throw(DB_ERROR("Failed to parse block from blob retrieved from the db"));
    }

    return bT;
}

CryptoNote::Block BlockchainDB::getBlock(const Crypto::Hash &h) const
{
    CryptoNote::blobData bD = getBlockBlob (h);
    CryptoNote::Block bT;
    if (!parseAndValidateBlockFromBlob (bD, bT)) {
        throw(DB_ERROR("Failed to parse block from blob retrieved from the db"));
    }

    return bT;
}

bool BlockchainDB::getTx(const Crypto::Hash &h, CryptoNote::Transaction &tx) const
{
    CryptoNote::blobData bD;
    if (!getTxBlob (h, bD)) {
        return false;
    }

    if (!parseAndValidateTxFromBlob(bD, tx)) {
        throw(DB_ERROR("Failed to parse transaction from blob retrieved from the db"));
    }

    return true;
}

CryptoNote::Transaction BlockchainDB::getTx(const Crypto::Hash &h) const
{
    CryptoNote::Transaction tx;
    if (!getTx(h, tx)) {
        throw(TX_DNE(std::string("tx with hash ")
                             .append(Common::podToHex(h))
                             .append(" not found in db").c_str()));
    }

    return tx;
}

void BlockchainDB::resetStats()
{
    numCalls            = 0;
    timeBlockHash       = 0;
    timeTxExists        = 0;
    timeAddBlock1       = 0;
    timeAddTransaction  = 0;
    timeCommit1         = 0;
}

void BlockchainDB::showStats()
{
    std::cout   << std::endl
                << "*********************************" << std::endl
                << "numCalls" << numCalls << std::endl
                << "timeBlockHash: " << timeBlockHash << "ms" << std::endl
                << "timeTxExists: " << timeTxExists << "ms" << std::endl
                << "timeAddBlock1: " << timeAddBlock1 << "ms" << std::endl
                << "timeAddTransaction: " << timeAddTransaction << "ms" << std::endl
                << "timeCommit1: " << timeCommit1 << "ms" << std::endl
                << "*********************************" << std::endl;
}

void BlockchainDB::fixup()
{
    if (isReadOnly ()) {
        return;
    }

    setBatchTransactions (true);
    batchStart ();
    batchStop ();
}

} // namespace CryptoNote
