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

#include <BlockchainExplorer/BlockchainExplorerData.h>

namespace CryptoNote {

class IBlockchainObserver
{
    typedef std::pair<Crypto::Hash, TransactionRemoveReason> RemovedTransactionDetails;

public:
    virtual ~IBlockchainObserver() = default;

    virtual void blockchainSynchronized(const BlockDetails &topBlock) {}

    virtual void blockchainUpdated(const std::vector<BlockDetails> &newBlocks,
                                   const std::vector<BlockDetails> &orphanedBlocks) {}

    virtual void poolUpdated(const std::vector<TransactionDetails> &newTransactions,
                             const std::vector<RemovedTransactionDetails> &removedTransactions) {}
};

} // namespace CryptoNote
