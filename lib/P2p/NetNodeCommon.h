// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2016-2018, The Karbowanec developers
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

#include <P2p/P2pProtocolTypes.h>
#include <CryptoNoteConfig.h>
#include <CryptoNote.h>

namespace CryptoNote {

struct CryptoNoteConnectionContext;

struct IP2pEndpoint
{
    virtual void relay_notify_to_all(int command,
                                     const BinaryArray &data_buff,
                                     const net_connection_id *excludeConnection) = 0;
    virtual bool invoke_notify_to_peer(int command,
                                       const BinaryArray &req_buff,
                                       const CryptoNote::CryptoNoteConnectionContext &context) = 0;
    virtual uint64_t get_connections_count() = 0;
    virtual bool ban_host(uint32_t address_ip, time_t seconds = CryptoNote::P2P_IP_BLOCKTIME) = 0;
    virtual bool unban_host(uint32_t address_ip) = 0;
    virtual void drop_connection(CryptoNoteConnectionContext& context, bool add_fail) = 0;
    virtual std::map<uint32_t, time_t> get_blocked_hosts() = 0;
    virtual void for_each_connection(
        std::function<void(CryptoNote::CryptoNoteConnectionContext &, PeerIdType)> f) = 0;
    virtual void externalRelayNotifyToAll(int command, const BinaryArray& data_buff) = 0;
};

struct p2p_endpoint_stub: public IP2pEndpoint
{
    void relay_notify_to_all(int command,
                             const BinaryArray &data_buff,
                             const net_connection_id *excludeConnection) override
    {
    }

    bool invoke_notify_to_peer(int command,
                               const BinaryArray &req_buff,
                               const CryptoNote::CryptoNoteConnectionContext &context) override
    {
        return true;
    }

    bool ban_host(const uint32_t address_ip, time_t seconds) override
    {
        return true;
    }

    bool unban_host(const uint32_t address_ip) override
    {
        return true;
    }

    void drop_connection(CryptoNoteConnectionContext &context, bool add_fail) override
    {
    }

    std::map<uint32_t, time_t> get_blocked_hosts() override
    {
        return std::map<uint32_t, time_t>();
    }

    void for_each_connection(
        std::function<void(CryptoNote::CryptoNoteConnectionContext &, PeerIdType)> f) override
    {
    }

    uint64_t get_connections_count() override
    {
        return 0;
    }

    void externalRelayNotifyToAll(int command, const BinaryArray &data_buff) override
    {
    }
};

} // namespace CryptoNote
