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

#include <boost/utility/value_init.hpp>
#include <CryptoNote.h>

namespace CryptoNote {

KeyPair generateKeyPair();

struct ParentBlockSerializer
{
    ParentBlockSerializer(
        ParentBlock &parentBlock,
        uint64_t &timestamp,
        uint32_t &nonce,
        bool hashingSerialization,
        bool headerOnly)
        : m_parentBlock(parentBlock),
          m_timestamp(timestamp),
          m_nonce(nonce),
          m_hashingSerialization(hashingSerialization),
          m_headerOnly(headerOnly)
    {
    }

    ParentBlock &m_parentBlock;
    uint64_t &m_timestamp;
    uint32_t &m_nonce;
    bool m_hashingSerialization;
    bool m_headerOnly;
};

inline ParentBlockSerializer makeParentBlockSerializer(
    const Block &b,
    bool hashingSerialization,
    bool headerOnly)
{
    auto &blockRef = const_cast<Block &>(b);
    return ParentBlockSerializer{
        blockRef.parentBlock,
        blockRef.timestamp,
        blockRef.nonce,
        hashingSerialization,
        headerOnly
    };
}

} // namespace CryptoNote
