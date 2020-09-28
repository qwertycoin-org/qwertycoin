

#pragma once

namespace CryptoNote {
#pragma pack(push, 1)
struct OutputDataT
{
    /*!
     * the output's public key (for spend verification)
     */
    Crypto::PublicKey   publicKey;

    /*!
     * the output's unlock time (or height)
     */
    uint64_t            unlockTime;

    /*!
     * the height of the block which created the output
     */
    uint64_t            height;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct TxDataT
{
    uint64_t    txId;
    uint64_t    unlockTime;
    uint64_t    blockId;
};
#pragma pack(pop)

struct TxPoolTxMetaT
{
    Crypto::Hash    maxUsedBlockId;
    Crypto::Hash    lastFailedId;
    uint64_t        blobSize;
    uint64_t        fee;
    uint64_t        maxUsedBlockHeight;
    uint64_t        lastFailedHeight;
    uint64_t        receiveTime;
    uint64_t        lastRelayedTime;
    /*!
     * 112 bytes
     */
    uint8_t keptByBlock;
    uint8_t relayed;
    uint8_t doNotRelay;
    uint8_t doubleSpendSeen: 1;

    /*!
     * till 192 bytes
     */
    uint8_t padding[76];
};
} // namespace CryptoNote

typedef std::pair<Crypto::Hash, uint64_t> txOutIndex;
