// Copyright 2019-2021 (c) The Qwertycoin Group.
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

#include <boost/utility/value_init.hpp>
#include <sstream>
#include <string>
#include <vector>
#include <CryptoTypes.h>
#include <Global/CryptoNoteConfig.h>
#include <version.h>

namespace Qwertycoin {

const Crypto::Hash NULL_HASH = boost::value_initialized<Crypto::Hash>();
const Crypto::PublicKey NULL_PUBLIC_KEY = boost::value_initialized<Crypto::PublicKey>();
const Crypto::SecretKey NULL_SECRET_KEY = boost::value_initialized<Crypto::SecretKey>();

/*!
    You can change things in this file, but you probably shouldn't.
    Leastways, without knowing what you're doing.
*/
namespace Constants {

/*!
    Amounts we make outputs into (Not mandatory, but a good idea)
*/
const std::vector <uint64_t> PRETTY_AMOUNTS {
    1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 20, 30, 40, 50, 60, 70, 80, 90,
    100, 200, 300, 400, 500, 600, 700, 800, 900,
    1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
    10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000,
    100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000, 900000,
    1000000, 2000000, 3000000, 4000000, 5000000, 6000000, 7000000, 8000000, 9000000,
    10000000, 20000000, 30000000, 40000000, 50000000, 60000000, 70000000, 80000000, 90000000,
    100000000, 200000000, 300000000, 400000000, 500000000, 600000000, 700000000, 800000000, 900000000,
    1000000000, 2000000000, 3000000000, 4000000000, 5000000000, 6000000000, 7000000000, 8000000000, 9000000000,
    10000000000, 20000000000, 30000000000, 40000000000, 50000000000, 60000000000, 70000000000,
    80000000000, 90000000000, 100000000000, 200000000000, 300000000000, 400000000000, 500000000000, 600000000000,
    700000000000, 800000000000, 900000000000, 1000000000000, 2000000000000, 3000000000000, 4000000000000,
    5000000000000, 6000000000000, 7000000000000, 8000000000000, 9000000000000, 10000000000000, 20000000000000,
    30000000000000, 40000000000000, 50000000000000, 60000000000000, 70000000000000, 80000000000000, 90000000000000,
    100000000000000, 200000000000000, 300000000000000, 400000000000000, 500000000000000, 600000000000000,
    700000000000000, 800000000000000, 900000000000000, 1000000000000000, 2000000000000000, 3000000000000000,
    4000000000000000, 5000000000000000, 6000000000000000, 7000000000000000, 8000000000000000, 9000000000000000,
    10000000000000000, 20000000000000000, 30000000000000000, 40000000000000000, 50000000000000000,
    60000000000000000, 70000000000000000, 80000000000000000, 90000000000000000, 100000000000000000, 200000000000000000,
    300000000000000000, 400000000000000000, 500000000000000000, 600000000000000000, 700000000000000000, 800000000000000000,
    900000000000000000, 1000000000000000000, 2000000000000000000, 3000000000000000000, 4000000000000000000,
    5000000000000000000, 6000000000000000000, 7000000000000000000, 8000000000000000000, 9000000000000000000,
    10000000000000000000ull
};

const std::string windowsAsciiArt =
        "\n                                                              \n"
        "                         _                   _                  \n"
        "                        | |                 (_)                 \n"
        "  __ ___      _____ _ __| |_ _   _  ___ ___  _ _ __             \n"
        " / _` \\ \\ /\\ / / _ \\ '__| __| | | |/ __/ _ \\| | '_  \\     \n"
        "| (_| |\\ V  V /  __/ |  | |_| |_| | (_| (_) | | | | |          \n"
        " \\__, | \\_/\\_/ \\___|_|   \\__|\\__, |\\___\\___/|_|_| |_|   \n"
        "    | |                       __/ |                             \n"
        "    |_|                      |___/                              \n"
        "                                                                \n";

const std::string nonWindowsAsciiArt =
        "\n                                                                                 \n"
        " ██████╗ ██╗    ██╗███████╗██████╗ ████████╗██╗   ██╗ ██████╗ ██████╗ ██╗███╗   ██╗\n"
        "██╔═══██╗██║    ██║██╔════╝██╔══██╗╚══██╔══╝╚██╗ ██╔╝██╔════╝██╔═══██╗██║████╗  ██║\n"
        "██║   ██║██║ █╗ ██║█████╗  ██████╔╝   ██║    ╚████╔╝ ██║     ██║   ██║██║██╔██╗ ██║\n"
        "██║▄▄ ██║██║███╗██║██╔══╝  ██╔══██╗   ██║     ╚██╔╝  ██║     ██║   ██║██║██║╚██╗██║\n"
        "╚██████╔╝╚███╔███╔╝███████╗██║  ██║   ██║      ██║   ╚██████╗╚██████╔╝██║██║ ╚████║\n"
        " ╚══▀▀═╝  ╚══╝╚══╝ ╚══════╝╚═╝  ╚═╝   ╚═╝      ╚═╝    ╚═════╝ ╚═════╝ ╚═╝╚═╝  ╚═══╝\n"
        "                                                                                   \n";

/*!
    Windows has some characters it won't display in a terminal. If your ascii
    art works fine on Windows and Linux terminals, just replace 'asciiArt' with
    the art itself, and remove these two #ifdefs and above ascii arts.
*/
#ifdef _WIN32
    const std::string asciiArt = windowsAsciiArt;
#else
    const std::string asciiArt = nonWindowsAsciiArt;
#endif

} // namespace Constants

/*!
    Make sure everything in here is const - or it won't compile!
*/
namespace WalletConfig {

/*!
    The prefix your coins address starts with
*/
const uint64_t addressPrefix = 0x14820c;

/*!
    Your coins 'Ticker', e.g. Monero = XMR, Bitcoin = BTC
*/
const std::string ticker = "QWC";

/*!
    The name of your deamon
*/
const std::string daemonName = "qwertycoind";

/*!
    The name to call this wallet
*/
const std::string walletName = "simplewallet";

/*!
    The name of service/walletd, the programmatic rpc interface to a wallet
*/
const std::string walletdName = "walletd";

/*!
    The full name of your crypto
*/
const std::string coinName = std::string(CryptoNote::CRYPTONOTE_NAME);

/*!
    Where can your users contact you for support? E.g. discord
*/
const std::string contactLink = "http://chat.qwertycoin.org";

} // namespace WalletConfig

inline std::string getProjectCLIHeader()
{
    std::stringstream programHeader;

    programHeader
        << std::endl
        << Constants::asciiArt << std::endl
        << " " << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << std::endl
        << " This software is distributed under the General Public License v3.0" << std::endl
        << std::endl
        << " " << PROJECT_COPYRIGHT << std::endl
        << std::endl
        << " Additional Copyright(s) may apply, please see the included LICENSE file for more information."
        << std::endl
        << " If you did not receive a copy of the LICENSE, please visit:" << std::endl
        << " " << CryptoNote::LICENSE_URL << std::endl
        << std::endl;

    return programHeader.str();
}

} // namespace Qwertycoin
