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

#include <cstddef>
#include <cstdint>
#include <utility>

namespace System {

class Dispatcher;
class Ipv4Address;

class TcpConnection
{
public:
    TcpConnection();
    TcpConnection(const TcpConnection &) = delete;
    TcpConnection(TcpConnection &&other) noexcept;
    ~TcpConnection();

    std::size_t read(uint8_t *data, std::size_t size);
    std::size_t write(const uint8_t *data, std::size_t size);
    std::pair<Ipv4Address, uint16_t> getPeerAddressAndPort() const;

    TcpConnection &operator=(const TcpConnection &) = delete;
    TcpConnection &operator=(TcpConnection &&other) noexcept(false);

private:
    TcpConnection(Dispatcher &dispatcher, int socket);

private:
    Dispatcher *dispatcher;
    int connection;
    void *readContext;

    void *writeContext;
    friend class TcpConnector;

    friend class TcpListener;
};

} // namespace System
