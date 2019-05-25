// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
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

#include <map>
#include <vector>
#include <Serialization/ISerializer.h>
#include <Transfers/CommonTypes.h>
#include <IStreamSerializable.h>

namespace CryptoNote {

class SynchronizationState : public IStreamSerializable
{
public:
    struct CheckResult
    {
        bool detachRequired;
        uint32_t detachHeight;
        bool hasNewBlocks;
        uint32_t newBlockHeight;
    };

    typedef std::vector<Crypto::Hash> ShortHistory;

    explicit SynchronizationState(const Crypto::Hash &genesisBlockHash)
    {
        m_blockchain.push_back(genesisBlockHash);
    }

    ShortHistory getShortHistory(uint32_t localHeight) const;
    CheckResult checkInterval(const BlockchainInterval &interval) const;

    void detach(uint32_t height);
    void addBlocks(const Crypto::Hash *blockHashes, uint32_t height, uint32_t count);
    uint32_t getHeight() const;
    const std::vector<Crypto::Hash> &getKnownBlockHashes() const;

    // IStreamSerializable
    void save(std::ostream &os) override;
    void load(std::istream &in) override;

    // serialization
    CryptoNote::ISerializer &serialize(CryptoNote::ISerializer &s, const std::string &name);

private:
    std::vector<Crypto::Hash> m_blockchain;
};

} // namespace CryptoNote
