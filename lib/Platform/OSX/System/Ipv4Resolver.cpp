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

#include <cassert>
#include <netdb.h>
#include <random>
#include <stdexcept>
#include <System/Dispatcher.h>
#include <System/ErrorMessage.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include "Ipv4Resolver.h"

namespace System {

Ipv4Resolver::Ipv4Resolver()
    : dispatcher(nullptr)
{
}

Ipv4Resolver::Ipv4Resolver(Dispatcher &dispatcher)
    : dispatcher(&dispatcher)
{
}

Ipv4Resolver::Ipv4Resolver(Ipv4Resolver &&other) noexcept
    : dispatcher(other.dispatcher)
{
    if (dispatcher != nullptr) {
        other.dispatcher = nullptr;
    }
}

Ipv4Resolver &Ipv4Resolver::operator=(Ipv4Resolver &&other) noexcept
{
    dispatcher = other.dispatcher;
    if (dispatcher != nullptr) {
        other.dispatcher = nullptr;
    }

    return *this;
}

Ipv4Address Ipv4Resolver::resolve(const std::string &host)
{
    assert(dispatcher != nullptr);
    if (dispatcher->interrupted()) {
        throw InterruptedException();
    }

    addrinfo hints = { 0, AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, nullptr, nullptr, nullptr };
    addrinfo *addressInfos;
    int result = getaddrinfo(host.c_str(), nullptr, &hints, &addressInfos);
    if (result != 0) {
        throw std::runtime_error(
            "Ipv4Resolver::resolve, getaddrinfo failed, " + errorMessage(result)
        );
    }

    std::size_t count = 0;
    for (addrinfo *addressInfo = addressInfos;
         addressInfo != nullptr;
         addressInfo = addressInfo->ai_next) {
        ++count;
    }

    std::mt19937 generator{ std::random_device()() };
    std::size_t index = std::uniform_int_distribution<std::size_t>(0, count - 1)(generator);
    addrinfo *addressInfo = addressInfos;
    for (std::size_t i = 0; i < index; ++i) {
        addressInfo = addressInfo->ai_next;
    }

    Ipv4Address address(ntohl(
        reinterpret_cast<sockaddr_in *>(addressInfo->ai_addr)->sin_addr.s_addr
    ));
    freeaddrinfo(addressInfo);
    return address;
}

} // namespace System
