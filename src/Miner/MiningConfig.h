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
#include <string>

namespace CryptoNote {

struct MiningConfig
{
    MiningConfig();

    void parse(int argc, char **argv);
    void printHelp();

    std::string miningAddress;
    std::string daemonHost;
    uint16_t daemonPort;
    size_t threadCount;
    size_t scanPeriod;
    uint8_t logLevel;
    size_t blocksLimit;
    uint64_t firstBlockTimestamp;
    int64_t blockTimestampInterval;
    bool help;
};

} // namespace CryptoNote
