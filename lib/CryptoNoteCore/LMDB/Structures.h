#include <CryptoTypes.h>
#include <CryptoNote.h>

#pragma once

namespace CryptoNote {
#pragma pack(push, 1)

    struct FOutputData
    {
        uint64_t uHeight;
        uint64_t uUnlockTime;
        Crypto::PublicKey sPublicKey;
    };

#pragma pack(pop)

#pragma pack(push, 1)

    struct FTxData
    {
        uint64_t uBlockID;
        uint64_t uTxID;
        uint64_t uUnlockTime;
    };
#pragma pack(pop)

    struct FTxPoolMeta
    {
        uint8_t uPadding[76];

        uint8_t uDoNotRelay;
        uint8_t uDoubleSpendSeen: 1;
        uint8_t uKeptByBlock;

        uint64_t uBlobSize;
        uint64_t uFee;
        uint64_t uLastFailedHeight;
        uint64_t uLastRelayedTime;
        uint64_t uMaxUsedBlockHeight;
        uint64_t uReceiveTime;

        Crypto::Hash uLastFailedID;
        Crypto::Hash uMaxUsedBlockID;
    };

    enum EBlockchainDBSyncMode
    {
        DBDefaultSync,
        DBSync,
        DBAsync,
        DBNoSync
    };

    struct FBlockExtendedInfo
    {
        Block sBlock;
        uint64_t uHeight;
        size_t uBlockCumulativeSize;
        difficulty_type uCumulativeDifficulty;
        uint64_t uAlreadyGeneratedCoins;
    };
}

typedef std::pair<Crypto::Hash, uint64_t> txOutIndex;
