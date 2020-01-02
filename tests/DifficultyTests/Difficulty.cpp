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

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

#include <Global/CryptoNoteConfig.h>
#include "CryptoNoteCore/Difficulty.h"
#include "CryptoNoteCore/Currency.h"
#include "Logging/ConsoleLogger.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "Wrong arguments" << std::endl;
        return 1;
    }

    Logging::ConsoleLogger logger;

    CryptoNote::CurrencyBuilder currencyBuilder(logger);
    currencyBuilder.difficultyTarget(120);
    currencyBuilder.difficultyWindow(720);
    currencyBuilder.difficultyCut(60);
    currencyBuilder.difficultyLag(15);
    CryptoNote::Currency currency = currencyBuilder.currency();

    std::fstream data(argv[1], std::fstream::in);
    data.exceptions(std::fstream::badbit);
    data.clear(data.rdstate());

    std::vector<uint64_t> cumulativeDifficulties;
    std::vector<uint64_t> timestamps;
    uint64_t cumulativeDifficulty = 0;
    uint64_t timestamp = 0;
    uint64_t difficulty = 0;
    size_t n = 0;
    while (data >> timestamp >> difficulty) {
        size_t begin;
        size_t end;
        if (n < currency.difficultyWindow() + currency.difficultyLag()) {
            begin = 0;
            end = std::min(n, currency.difficultyWindow());
        } else {
            end = n - currency.difficultyLag();
            begin = end - currency.difficultyWindow();
        }

        uint64_t res = currency.nextDifficultyV1(
            std::vector<uint64_t>{
                std::next(std::begin(timestamps), begin),
                std::next(std::begin(timestamps), begin + end)
            },
            std::vector<uint64_t>{
                std::next(std::begin(cumulativeDifficulties), begin),
                std::next(std::begin(cumulativeDifficulties), begin + end)
            }
        );
        if (res != difficulty) {
            std::cerr << "Wrong difficulty for block " << n << std::endl
                      << "Expected: " << difficulty << std::endl
                      << "Found: " << res << std::endl;
            return 1;
        }

        cumulativeDifficulty += difficulty;
        cumulativeDifficulties.push_back(cumulativeDifficulty);
        timestamps.push_back(timestamp);
        ++n;
    }

    if (!data.eof()) {
        data.clear(std::fstream::badbit);
    }

    return 0;
}
