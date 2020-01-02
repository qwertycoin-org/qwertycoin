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

#include <vector>
#include <CryptoNote.h>

namespace CryptoNote {

class NewBlockMessage
{
public:
    explicit NewBlockMessage(const Crypto::Hash &hash);
    NewBlockMessage() = default;

    void get(Crypto::Hash &hash) const;

private:
    Crypto::Hash blockHash;
};

class NewAlternativeBlockMessage
{
public:
    explicit NewAlternativeBlockMessage(const Crypto::Hash &hash);
    NewAlternativeBlockMessage() = default;

    void get(Crypto::Hash &hash) const;

private:
    Crypto::Hash blockHash;
};

class ChainSwitchMessage
{
public:
    explicit ChainSwitchMessage(std::vector<Crypto::Hash> &&hashes);
    ChainSwitchMessage(const ChainSwitchMessage &other);

    void get(std::vector<Crypto::Hash> &hashes) const;

private:
    std::vector<Crypto::Hash> blocksFromCommonRoot;
};

class BlockchainMessage
{
public:
    enum class MessageType
    {
        NEW_BLOCK_MESSAGE,
        NEW_ALTERNATIVE_BLOCK_MESSAGE,
        CHAIN_SWITCH_MESSAGE
    };

    BlockchainMessage(const BlockchainMessage &other);
    explicit BlockchainMessage(NewBlockMessage &&message);
    explicit BlockchainMessage(NewAlternativeBlockMessage &&message);
    explicit BlockchainMessage(ChainSwitchMessage &&message);
    ~BlockchainMessage();

    MessageType getType() const;

    bool getNewBlockHash(Crypto::Hash &hash) const;
    bool getNewAlternativeBlockHash(Crypto::Hash &hash) const;
    bool getChainSwitch(std::vector<Crypto::Hash> &hashes) const;

private:
    const MessageType type;

    union
    {
        NewBlockMessage newBlockMessage;
        NewAlternativeBlockMessage newAlternativeBlockMessage;
        ChainSwitchMessage* chainSwitchMessage;
    };
};

} // namespace CryptoNote
