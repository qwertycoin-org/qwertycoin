// Copyright 2019 (c) The Qwertycoin Group.
// Licensed under the GNU General Public License, Version 3.
// See the file LICENSE from this package for details.

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

/*!
    Indicates the following data is a payment ID
*/
const uint8_t TX_EXTRA_PAYMENT_ID_IDENTIFIER = 0x00;

/*!
    Indicates the following data is a transaction public key
*/
const uint8_t TX_EXTRA_PUBKEY_IDENTIFIER = 0x01;

/*!
    Indicates the following data is an extra nonce
*/
const uint8_t TX_EXTRA_NONCE_IDENTIFIER = 0x02;

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
    The filename to output the CSV to in save_csv
*/
const std::string csvFilename = "transactions.csv";

/*!
    The filename to read+write the address book to - consider starting with
    a leading '.' to make it hidden under mac+linux
*/
const std::string addressBookFilename = ".addressBook.json";

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
const std::string walletdName = "qwertycoin-service";

/*!
    The full name of your crypto
*/
const std::string coinName = std::string(CryptoNote::CRYPTONOTE_NAME);

/*!
    Where can your users contact you for support? E.g. discord
*/
const std::string contactLink = "http://chat.qwertycoin.org";

/*!
    The number of decimals your coin has
*/
const uint8_t numDecimalPlaces = CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT;

/*!
    The length of a standard address for your coin
*/
const uint16_t standardAddressLength = 98;

/*!
    The length of an integrated address for your coin - It's the same as
    a normal address, but there is a paymentID included in there - since
    payment ID's are 64 chars, and base58 encoding is done by encoding
    chunks of 8 chars at once into blocks of 11 chars, we can calculate
    this automatically
*/
const uint16_t integratedAddressLength = standardAddressLength + ((64 * 11) / 8);

/*!
    The default fee value to use with transactions (in ATOMIC units!)
*/
const uint64_t defaultFee = CryptoNote::parameters::MINIMUM_FEE;

/*!
    The minimum fee value to allow with transactions (in ATOMIC units!)
*/
const uint64_t minimumFee = CryptoNote::parameters::MINIMUM_FEE;

/*!
    The minimum amount allowed to be sent - usually 1 (in ATOMIC units!)
*/
const uint64_t minimumSend = 1;

/*!
    Is a mixin of zero disabled on your network?
*/
const bool mixinZeroDisabled = false;

/*!
    If a mixin of zero is disabled, at what height was it disabled?
    E.g. fork height, or 0, if never allowed. This is ignored if a
    mixin of zero is allowed
*/
const uint64_t mixinZeroDisabledHeight = CryptoNote::parameters::MIXIN_LIMITS_V2_HEIGHT;

/*!
    Should we process coinbase transactions? We can skip them to speed up
    syncing, as most people don't have solo mined transactions
*/
const bool processCoinbaseTransactions = true;

/*!
    Max size of a post body response - 10MB
    Will decrease the amount of blocks requested from the daemon if this
    is exceeded.
    Note - blockStoreMemoryLimit - maxBodyResponseSize should be greater
    than zero, or no data will get cached.
    Further note: Currently blocks request are not decreased if this is
    exceeded. Needs to be implemented in future?
*/
const size_t maxBodyResponseSize = 1024 * 1024 * 10;

/*!
    The amount of memory to use storing downloaded blocks - 50MB
*/
const size_t blockStoreMemoryLimit = 1024 * 1024 * 50;

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
