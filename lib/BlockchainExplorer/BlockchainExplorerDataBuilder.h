// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2023, The Qwertycoin Group.
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
#include <CryptoNoteCore/ICore.h>
#include <CryptoNoteProtocol/ICryptoNoteProtocolQuery.h>

namespace CryptoNote {

class BlockchainExplorerDataBuilder
{
public:
    BlockchainExplorerDataBuilder(ICore &core, ICryptoNoteProtocolQuery &protocol);
    BlockchainExplorerDataBuilder(const BlockchainExplorerDataBuilder &) = delete;
    BlockchainExplorerDataBuilder(BlockchainExplorerDataBuilder &&) = delete;

    bool fillBlockDetails(const Block &block, BlockDetails &blockDetails);
    bool fillTransactionDetails(const Transaction &tx,
                                TransactionDetails &txRpcInfo,
                                uint64_t timestamp = 0);

    static bool getPaymentId(const Transaction &transaction, Crypto::Hash &paymentId);

    BlockchainExplorerDataBuilder &operator=(const BlockchainExplorerDataBuilder &) = delete;
    BlockchainExplorerDataBuilder &operator=(BlockchainExplorerDataBuilder &&) = delete;

private:
    static bool getMixin(const Transaction &transaction, uint64_t &mixin);
    static bool fillTxExtra(const std::vector<uint8_t> &rawExtra,
                            TransactionExtraDetails &extraDetails);
    static size_t median(std::vector<size_t> &v);

    ICore &m_core;
    ICryptoNoteProtocolQuery &m_protocol; // Not used!
};

} // namespace CryptoNote
