// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
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

#include <BlockchainExplorer/IBlockchainObserver.h>

namespace CryptoNote {

class IBlockchainExplorer
{
public:
    virtual ~IBlockchainExplorer() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;

    virtual bool addObserver(IBlockchainObserver *observer) = 0;
    virtual bool removeObserver(IBlockchainObserver *observer) = 0;

    virtual bool getBlocks(const std::vector<uint32_t> &blockHeights,
                           std::vector<std::vector<BlockDetails>> &blocks) = 0;
    virtual bool getBlocks(const std::vector<Crypto::Hash> &blockHashes,
                           std::vector<BlockDetails> &blocks) = 0;
    virtual bool getBlocks(uint64_t timestampBegin,
                           uint64_t timestampEnd,
                           uint32_t blocksNumberLimit,
                           std::vector<BlockDetails> &blocks,
                           uint32_t &blocksNumberWithinTimestamps) = 0;

    virtual bool getBlockchainTop(BlockDetails &topBlock) = 0;

    virtual bool getPoolState(const std::vector<Crypto::Hash> &knownPoolTransactionHashes,
                              Crypto::Hash knownBlockchainTop,
                              bool &isBlockchainActual,
                              std::vector<TransactionDetails> &newTransactions,
                              std::vector<Crypto::Hash> &removedTransactions) = 0;
    virtual bool getPoolTransactions(uint64_t timestampBegin,
                                     uint64_t timestampEnd,
                                     uint32_t transactionsNumberLimit,
                                     std::vector<TransactionDetails> &transactions,
                                     uint64_t &transactionsNumberWithinTimestamps) = 0;
    virtual bool getTransactions(const std::vector<Crypto::Hash> &transactionHashes,
                                 std::vector<TransactionDetails> &transactions) = 0;
    virtual bool getTransactionsByPaymentId(const Crypto::Hash &paymentId,
                                            std::vector<TransactionDetails> &transactions) = 0;

    virtual uint64_t getRewardBlocksWindow() = 0;
    virtual uint64_t getFullRewardMaxBlockSize(uint8_t majorVersion) = 0;

    virtual bool isSynchronized() = 0;
};

} // namespace CryptoNote
