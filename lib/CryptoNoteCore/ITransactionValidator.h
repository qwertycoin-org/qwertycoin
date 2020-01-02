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

#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <Global/Constants.h>

using namespace Qwertycoin;

namespace CryptoNote {

struct BlockInfo
{
    BlockInfo()
        : height(0),
          id(NULL_HASH)
    {
    }

    void clear()
    {
        height = 0;
        id = NULL_HASH;
    }

    bool empty() const
    {
        return id == NULL_HASH;
    }

    uint32_t height;
    Crypto::Hash id;
};

class ITransactionValidator
{
public:
    virtual ~ITransactionValidator() = default;

    virtual bool checkTransactionInputs(const Transaction &tx, BlockInfo &maxUsedBlock) = 0;
    virtual bool checkTransactionInputs(
        const Transaction &tx,
        BlockInfo &maxUsedBlock,
        BlockInfo &lastFailed) = 0;
    virtual bool haveSpentKeyImages(const Transaction &tx) = 0;
    virtual bool checkTransactionSize(size_t blobSize) = 0;
};

} // namespace CryptoNote
