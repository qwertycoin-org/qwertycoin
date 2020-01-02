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

#include <queue>
#include <P2p/IP2pNodeInternal.h>
#include <P2p/LevinProtocol.h>
#include <P2p/P2pContextOwner.h>
#include <P2p/P2pInterfaces.h>

namespace CryptoNote {

class P2pContext;

class P2pConnectionProxy : public IP2pConnection
{
public:
    P2pConnectionProxy(P2pContextOwner &&ctx, IP2pNodeInternal &node);
    ~P2pConnectionProxy() override;

    bool processIncomingHandshake();

    void read(P2pMessage &message) override;
    void write(const P2pMessage &message) override;
    void ban() override;
    void stop() override;

private:
    void writeHandshake(const P2pMessage &message);
    void handleHandshakeRequest(const LevinProtocol::Command &cmd);
    void handleHandshakeResponse(const LevinProtocol::Command &cmd, P2pMessage &message);
    void handleTimedSync(const LevinProtocol::Command &cmd);

private:
    std::queue<P2pMessage> m_readQueue;
    P2pContextOwner m_contextOwner;
    P2pContext &m_context;
    IP2pNodeInternal &m_node;
};

} // namespace CryptoNote
