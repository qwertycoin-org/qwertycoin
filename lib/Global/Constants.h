// Copyright 2019 (c) The Qwertycoin Group.
// Licensed under the GNU General Public License, Version 3.
// See the file LICENSE from this package for details.

#pragma once

#include <string>
#include <vector>
#include <CryptoTypes.h>

#define QWC_BEGIN_NAMESPACE namespace Qwertycoin {
#define QWC_END_NAMESPACE }

QWC_BEGIN_NAMESPACE

class Constants
{
public:
    explicit Constants() = default;
    virtual ~Constants() = default;

    static std::vector<uint64_t> prettyAmounts();

    static uint8_t txExtraPaymentIdIdentifier();
    static uint8_t txExtraPubkeyIdentifier();
    static uint8_t txExtraNonceIdentifier();

    static Crypto::Hash nullHash();
    static Crypto::PublicKey nullPublicKey();
    static Crypto::SecretKey nullSecretKey();

    // ASCII art
    static std::string asciiArt();
    static std::string unixAsciiArt();
    static std::string windowsAsciiArt();
};

QWC_END_NAMESPACE
