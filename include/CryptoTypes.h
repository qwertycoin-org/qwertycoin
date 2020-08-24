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

#include <cstdint>
#include <cstring>

namespace Crypto {

struct Hash
{
    uint8_t data[32];
};

// This structure can be used to store Hashes in ordered containers
// like std::set<Hash, HashCompare>
struct HashCompare
{
   bool operator() (const Hash& lh, const Hash& rh) const {
      return memcmp(lh.data, rh.data, 32) > 0;
   }
};

struct PublicKey
{
    uint8_t data[32];
};

struct SecretKey
{
    uint8_t data[32];
};

struct KeyDerivation
{
    uint8_t data[32];
};

struct KeyImage
{
    uint8_t data[32];
};

struct Signature
{
    uint8_t data[64];
};

} // namespace CryptoNote
