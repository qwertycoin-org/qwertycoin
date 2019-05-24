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

#include <string>

namespace System {

class Dispatcher;
class Ipv4Address;

class Ipv4Resolver
{
public:
    Ipv4Resolver();
    explicit Ipv4Resolver(Dispatcher &dispatcher);
    Ipv4Resolver(const Ipv4Resolver &) = delete;
    Ipv4Resolver(Ipv4Resolver &&other) noexcept;
    ~Ipv4Resolver() = default;

    Ipv4Address resolve(const std::string &host);

    Ipv4Resolver &operator=(const Ipv4Resolver &) = delete;
    Ipv4Resolver &operator=(Ipv4Resolver &&other) noexcept;

private:
    Dispatcher *dispatcher;
};

} // namespace System
