// Copyright 2019 (c) The Qwertycoin Group.
// Licensed under the GNU General Public License, Version 3.
// See the file LICENSE from this package for details.

#pragma once

#include <string>
#include <vector>
#include <boost/utility/value_init.hpp>
#include <CryptoTypes.h>

#define QWC_BEGIN_NAMESPACE namespace Qwertycoin {
#define QWC_END_NAMESPACE }

QWC_BEGIN_NAMESPACE

const Crypto::Hash NULL_HASH = boost::value_initialized<Crypto::Hash>();
const Crypto::PublicKey NULL_PUBLIC_KEY = boost::value_initialized<Crypto::PublicKey>();
const Crypto::SecretKey NULL_SECRET_KEY = boost::value_initialized<Crypto::SecretKey>();

class Constants
{
public:
    explicit Constants() = default;
    virtual ~Constants() = default;

    static std::vector<uint64_t> prettyAmounts();

    static uint8_t txExtraPaymentIdIdentifier();
    static uint8_t txExtraPubkeyIdentifier();
    static uint8_t txExtraNonceIdentifier();

    // ASCII art
    static std::string asciiArt();
    static std::string unixAsciiArt();
    static std::string windowsAsciiArt();

    static std::string daemonCliHeader();

    // Wallet config
    static uint64_t addressPrefix();
    static std::string ticker();
    static std::string walletCsvFilename();
    static std::string walletAddressBookFilename();
    static std::string daemonName();
    static std::string walletName();
    static std::string walletdName();
    static std::string coinName();
    static std::string contactLink();
    static uint8_t numDecimalPlaces();
    static uint16_t standardAddressLength();
    static uint16_t integratedAddressLength();
    static uint64_t defaultFee();
    static uint64_t minimumFee();
    static uint64_t minimumSend();
    static bool mixinZeroDisabled();
    static uint64_t mixinZeroDisabledHeight();
    static bool processCoinbaseTransactions();
    static size_t maxBodyResponseSize();
    static size_t blockStoreMemoryLimit();
};

QWC_END_NAMESPACE
