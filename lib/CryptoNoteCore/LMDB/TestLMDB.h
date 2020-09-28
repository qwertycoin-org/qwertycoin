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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <map>
#include <string>
#include <vector>

#include <CryptoNoteCore/LMDB/BlockchainDB.h>
#include <CryptoNoteCore/LMDB/Structures.h>

namespace CryptoNote {
class BaseTestDB: public CryptoNote::BlockchainDB
{
public:
    BaseTestDB() {}
    virtual void open(const std::string &filename, const int dbFlags = 0) override {}
    virtual void close() override {}
    virtual void sync() override {}
    virtual void safeSyncMode(const bool onOff) override {}
    virtual void reset() override {}
    virtual std::vector<std::string> getFilenames() const override { return std::vector<std::string>(); }
    virtual bool removeDataFile(const std::string &folder) const { return true; }
    virtual std::string getDbName() const override { return std::string(); }
    virtual bool lock() override { return true; }
    virtual void unlock() override { }
    virtual bool batchStart(uint64_t batchNumBlocks = 0, uint64_t batchBytes = 0) override { return true; }
    virtual void batchStop() override {}
    virtual void setBatchTransactions(bool) override {}
    virtual void blockWTxnStart() {}
    virtual void blockWTxnStop() {}
    virtual void blockWTxnAbort() {}
    virtual bool blockRTxnStart() const { return true; }
    virtual void blockRTxnStop() const {}
    virtual void blockRTxnAbort() const {}

    virtual bool blockExists(const Crypto::Hash &h, uint64_t *height) const override { return false; }

    virtual CryptoNote::blobData getBlockBlob(const Crypto::Hash &h) const override
    {
        return CryptoNote::blobData();
    }

    virtual bool getTxBlob(const Crypto::Hash &h, CryptoNote::blobData &tx) const override { return false; }

    virtual uint64_t getBlockHeight(const Crypto::Hash &h) const override { return 0; }

    virtual CryptoNote::BlockHeader getBlockHeader(const Crypto::Hash &h) const override
    {
        return CryptoNote::BlockHeader();
    }

    virtual uint64_t getBlockTimestamp(const uint64_t &height) const override { return 0; }

    virtual uint64_t getTopBlockTimestamp() const override { return 0; }

    virtual size_t getBlockSize(const uint64_t &height) const override { return 128; }

    virtual std::vector<uint64_t> getBlockSizes(uint64_t startHeight,
                                                size_t count) const
    {
        return {};
    }

    virtual uint64_t getBlockCumulativeDifficulty(const uint64_t &height) const override
    {
        return 10;
    }

    virtual uint64_t getBlockDifficulty(const uint64_t &height) const override { return 0; }

    virtual uint64_t getBlockAlreadyGeneratedCoins(const uint64_t &height) const override { return 10000000000; }

    virtual Crypto::Hash getBlockHashFromHeight(const uint64_t &height) const override { return Crypto::Hash(); }

    virtual std::vector<CryptoNote::Block> getBlocksRange(const uint64_t &h1,
                                                          const uint64_t &h2) const override
    {
        return std::vector<CryptoNote::Block>();
    }

    virtual std::vector<Crypto::Hash> getHashesRange(const uint64_t &h1,
                                                     const uint64_t &h2) const override
    {
        return std::vector<Crypto::Hash>();
    }

    virtual Crypto::Hash topBlockHash(uint64_t *blockHeight = NULL) const
    {
        if (blockHeight) {
            *blockHeight = 0;
        }

        return Crypto::Hash();
    }

    virtual CryptoNote::Block getTopBlock() const override { return CryptoNote::Block(); }

    virtual uint64_t height() const override { return 1; }

    virtual bool txExists(const Crypto::Hash &h) const override { return false; }

    virtual bool txExists(const Crypto::Hash &h, uint64_t &txIndex) const override { return false; }

    virtual uint64_t getTxUnlockTime(const Crypto::Hash &h) const override { return 0; }

    virtual CryptoNote::Transaction getTx(const Crypto::Hash &h) const override
    {
        return CryptoNote::Transaction();
    }

    virtual bool getTx(const Crypto::Hash &h,
                       CryptoNote::Transaction &tx) const override
    {
        return false;
    }

    virtual uint64_t getTxCount() const override { return 0; }

    virtual std::vector<CryptoNote::Transaction> getTxList(const std::vector<Crypto::Hash> &hList) const override
    {
        return std::vector<CryptoNote::Transaction>();
    }

    virtual uint64_t getTxBlockHeight(const Crypto::Hash &h) const override { return 0; }

    virtual uint64_t getNumOutputs(const uint64_t &amount) const override { return 1; }

    virtual uint64_t getIndexingBase() const override { return 0; }

    virtual CryptoNote::txOutIndex getOutputTxAndIndexFromGlobal(const uint64_t &index) const override
    {
        return CryptoNote::txOutIndex();
    }

    virtual CryptoNote::txOutIndex getOutputTxAndIndex(const uint64_t &amount,
                                                       const uint64_t &index) const override
    {
        return CryptoNote::txOutIndex();
    }

    virtual void getOutputTxAndIndex(const uint64_t &amount,
                                     const std::vector<uint64_t> &offsets,
                                     std::vector<CryptoNote::txOutIndex> &indices) const override {}

    virtual bool canThreadBulkIndices() const override { return false; }

    virtual bool hasKeyImage(const Crypto::KeyImage &img) const override { return false; }

    virtual void removeBlock() override { }

    virtual uint64_t addOutput(const Crypto::Hash &txHash,
                               const CryptoNote::TransactionOutput &txOutput,
                               const uint64_t &localIndex,
                               const uint64_t unlockTime) override {return 0;}

    virtual void addTxAmountOutputIndices(const uint64_t txIndex,
                                          const std::vector<uint64_t> &amountOutputIndices) override {}

    virtual void addSpentKey(const Crypto::KeyImage &kImage) override {}

    virtual void removeSpentKey(const Crypto::KeyImage &kImage) override {}

    virtual bool forAllKeyImages(std::function<bool(const Crypto::KeyImage&)>) const override { return true; }

    virtual bool forBlocksRange(const uint64_t &,
                                const uint64_t &,
    std::function<bool(uint64_t,
    const Crypto::Hash &,
    const CryptoNote::Block &)>) const override { return true; }
    virtual bool forAllTransactions(std::function<bool(const Crypto::Hash&,
                                                       const CryptoNote::Transaction&)>) const override
    {
        return true;
    }
    virtual bool forAllOutputs(std::function<bool(uint64_t amount,
    const Crypto::Hash &txHash,
            uint64_t height,
    size_t txIdx)> f) const override { return true; }
    virtual bool forAllOutputs(uint64_t amount,
            const std::function<bool(uint64_t height)> &f) const override { return true; }
    virtual bool isReadOnly() const override { return false; }

    virtual void addTxPoolTx(const Crypto::Hash &txId,
                             const CryptoNote::BinaryArray &blob,
                             const CryptoNote::txpoolTxMetaT &details) {}

    virtual void updateTxPoolTx(const Crypto::Hash &txId, const CryptoNote::txpoolTxMetaT &details) override {}

    virtual uint64_t getTxPoolTxCount(bool includeUnrelayedTxes = true) const override { return 0; }

    virtual bool txPoolHasTx(const Crypto::Hash &txId) const override { return false; }

    virtual void removeTxPoolTx(const Crypto::Hash &txId) override {}

    virtual bool getTxPoolTxMeta(const Crypto::Hash &txId,
                                 CryptoNote::txpoolTxMetaT &meta) const override { return false; }

    virtual bool getTxPoolTxBlob(const Crypto::Hash &txId,
                                 CryptoNote::BinaryArray &bd) const { return false; }

    virtual uint64_t getDatabaseSize() const { return 0; }

    virtual CryptoNote::blobData getTxPoolTxBlob(const Crypto::Hash &txId) const override { return ""; }

    virtual bool forAllTxPoolTxes(std::function<bool(const Crypto::Hash &,
                                                     const CryptoNote::txpoolTxMetaT &,
                                                     const CryptoNote::BinaryArray *)>,
                                  bool includeBlob = false) const
    {
        return false;
    }

    virtual void addBlock(const CryptoNote::Block &blk,
                          size_t blockSize,
                          const uint64_t &cumulativeDifficulty,
                          const uint64_t &coinsGenerated,
                          const Crypto::Hash &blkHash) { }

    virtual CryptoNote::Block getBlockFromHeight(const uint64_t &height) const override
    {
        return CryptoNote::Block();
    }
};
}
