// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Forknote developers
// Copyright (c) 2017-2018, The Karbo developers
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

#include <BlockchainExplorer/BlockchainExplorerData.h>
#include <crypto/hash.h>
#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <CryptoNoteCore/Difficulty.h>
#include <CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h>
#include <Serialization/BlockchainExplorerDataSerialization.h>
#include <Serialization/SerializationOverloads.h>

namespace CryptoNote {

#define CORE_RPC_STATUS_OK "OK"
#define CORE_RPC_STATUS_BUSY "BUSY"

struct EMPTY_STRUCT
{
    void serialize(ISerializer &s)
    {
    }
};

struct STATUS_STRUCT
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(status)
    }

    std::string status;
};

struct COMMAND_RPC_GET_HEIGHT
{
    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
            KV_MEMBER(status)
        }

        uint64_t height;
        std::string status;
    };

    typedef EMPTY_STRUCT request;
};

struct COMMAND_RPC_GET_BLOCKS_FAST
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            serializeAsBinary(block_ids, "block_ids", s);
        }

        /*!
            First 10 blocks id goes sequential, next goes in pow(2,n) offset,
            like 2, 4, 8, 16, 32, 64 and so on, and the last one is always
            genesis block
        */
        std::vector<Crypto::Hash> block_ids;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(blocks)
            KV_MEMBER(start_height)
            KV_MEMBER(current_height)
            KV_MEMBER(status)
        }

        std::vector<block_complete_entry> blocks;
        uint64_t start_height;
        uint64_t current_height;
        std::string status;
    };
};

struct COMMAND_RPC_GET_TRANSACTIONS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(txs_hashes)
        }

        std::vector<std::string> txs_hashes;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(txs_as_hex)
            KV_MEMBER(missed_tx)
            KV_MEMBER(status)
        }

        std::vector<std::string> txs_as_hex; // transactions blobs as hex
        std::vector<std::string> missed_tx;  // not found transactions
        std::string status;
    };
};

struct COMMAND_RPC_GET_POOL_CHANGES
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tailBlockId)
            serializeAsBinary(knownTxsIds, "knownTxsIds", s);
        }

        Crypto::Hash tailBlockId;
        std::vector<Crypto::Hash> knownTxsIds;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(isTailBlockActual)
            KV_MEMBER(addedTxs)
            serializeAsBinary(deletedTxsIds, "deletedTxsIds", s);
            KV_MEMBER(status)
        }

        bool isTailBlockActual;
        std::vector<BinaryArray> addedTxs; // added transactions blobs
        std::vector<Crypto::Hash> deletedTxsIds; // IDs of not found transactions
        std::string status;
    };
};

struct COMMAND_RPC_GET_POOL_CHANGES_LITE
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tailBlockId)
            serializeAsBinary(knownTxsIds, "knownTxsIds", s);
        }

        Crypto::Hash tailBlockId;
        std::vector<Crypto::Hash> knownTxsIds;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(isTailBlockActual)
            KV_MEMBER(addedTxs)
            serializeAsBinary(deletedTxsIds, "deletedTxsIds", s);
            KV_MEMBER(status)
        }

        bool isTailBlockActual;
        std::vector<TransactionPrefixInfo> addedTxs; // added transactions blobs
        std::vector<Crypto::Hash> deletedTxsIds; // IDs of not found transactions
        std::string status;
    };
};

struct COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(txid)
        }

        Crypto::Hash txid;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(o_indexes)
            KV_MEMBER(status)
        }

        std::vector<uint64_t> o_indexes;
        std::string status;
    };
};

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(amounts)
        KV_MEMBER(outs_count)
    }

    std::vector<uint64_t> amounts;
    uint64_t outs_count;
};

#pragma pack(push, 1)
struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_out_entry
{
    uint64_t global_amount_index;
    Crypto::PublicKey out_key;
};
#pragma pack(pop)

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(amount)
        serializeAsBinary(outs, "outs", s);
    }

    uint64_t amount;
    std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_out_entry> outs;
};

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(outs)
        KV_MEMBER(status)
    }

    std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount> outs;
    std::string status;
};

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS
{
    typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request request;
    typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response response;
    typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_out_entry out_entry;
    typedef COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount outs_for_amount;
};

struct COMMAND_RPC_SEND_RAW_TX
{
    struct request
    {
        request() = default;
        explicit request(const Transaction &);

        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_as_hex)
        }

        std::string tx_as_hex;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
        }

        std::string status;
    };
};

struct COMMAND_RPC_START_MINING
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(miner_address)
            KV_MEMBER(threads_count)
        }

        std::string miner_address;
        uint64_t threads_count;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
        }

        std::string status;
    };
};

struct COMMAND_RPC_GET_INFO
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(version)
            KV_MEMBER(height)
            KV_MEMBER(top_block_hash)
            KV_MEMBER(difficulty)
            KV_MEMBER(min_tx_fee)
            KV_MEMBER(readable_tx_fee)
            KV_MEMBER(tx_count)
            KV_MEMBER(tx_pool_size)
            KV_MEMBER(alt_blocks_count)
            KV_MEMBER(outgoing_connections_count)
            KV_MEMBER(incoming_connections_count)
            KV_MEMBER(rpc_connections_count)
            KV_MEMBER(white_peerlist_size)
            KV_MEMBER(grey_peerlist_size)
            KV_MEMBER(last_known_block_index)
            KV_MEMBER(start_time)
            KV_MEMBER(fee_address)
            KV_MEMBER(block_major_version)
            KV_MEMBER(already_generated_coins)
            KV_MEMBER(contact)
            KV_MEMBER(block_minor_version)
            KV_MEMBER(last_block_reward)
            KV_MEMBER(last_block_timestamp)
            KV_MEMBER(last_block_difficulty)
        }

        std::string status;
        std::string version;
        uint64_t height;
        std::string top_block_hash;
        uint64_t difficulty;
        uint64_t min_tx_fee;
        std::string readable_tx_fee;
        uint64_t tx_count;
        uint64_t tx_pool_size;
        uint64_t alt_blocks_count;
        uint64_t outgoing_connections_count;
        uint64_t incoming_connections_count;
        uint64_t rpc_connections_count;
        uint64_t white_peerlist_size;
        uint64_t grey_peerlist_size;
        uint32_t last_known_block_index;
        uint64_t start_time;
        std::string fee_address;
        uint8_t block_major_version;
        std::string already_generated_coins;
        std::string contact;
        uint8_t block_minor_version;
        uint64_t last_block_reward;
        uint64_t last_block_timestamp;
        uint64_t last_block_difficulty;
    };
};

struct COMMAND_RPC_STOP_MINING
{
    typedef EMPTY_STRUCT request;
    typedef STATUS_STRUCT response;
};

struct COMMAND_RPC_STOP_DAEMON
{
    typedef EMPTY_STRUCT request;
    typedef STATUS_STRUCT response;
};

struct COMMAND_RPC_GET_PEER_LIST
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(peers)
            KV_MEMBER(status)
        }

        std::vector<std::string> peers;
        std::string status;
    };
};

struct COMMAND_RPC_GET_FEE_ADDRESS
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(fee_address)
            KV_MEMBER(status)
        }

        std::string fee_address;
        std::string status;
    };
};

struct COMMAND_RPC_GETBLOCKCOUNT
{
    typedef std::vector<std::string> request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(count)
            KV_MEMBER(status)
        }

        uint64_t count;
        std::string status;
    };
};

struct COMMAND_RPC_GETBLOCKHASH
{
    typedef std::vector<uint64_t> request;
    typedef std::string response;
};

struct COMMAND_RPC_GETBLOCKTEMPLATE
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(reserve_size)
            KV_MEMBER(wallet_address)
        }

        uint64_t reserve_size; // max 255 bytes
        std::string wallet_address;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(difficulty)
            KV_MEMBER(height)
            KV_MEMBER(reserved_offset)
            KV_MEMBER(blocktemplate_blob)
            KV_MEMBER(blockhashing_blob)
            KV_MEMBER(status)
        }

        uint64_t difficulty;
        uint32_t height;
        uint64_t reserved_offset;
        std::string blocktemplate_blob;
        std::string blockhashing_blob;
        std::string status;
    };
};

struct COMMAND_RPC_GET_CURRENCY_ID
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(currency_id_blob)
        }

        std::string currency_id_blob;
    };
};

struct COMMAND_RPC_SUBMITBLOCK
{
    typedef std::vector<std::string> request;
    typedef STATUS_STRUCT response;
};

struct block_header_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(major_version)
        KV_MEMBER(minor_version)
        KV_MEMBER(timestamp)
        KV_MEMBER(prev_hash)
        KV_MEMBER(nonce)
        KV_MEMBER(orphan_status)
        KV_MEMBER(height)
        KV_MEMBER(depth)
        KV_MEMBER(hash)
        KV_MEMBER(difficulty)
        KV_MEMBER(reward)
    }

    uint8_t major_version;
    uint8_t minor_version;
    uint64_t timestamp;
    std::string prev_hash;
    uint32_t nonce;
    bool orphan_status;
    uint64_t height;
    uint64_t depth;
    std::string hash;
    difficulty_type difficulty;
    uint64_t reward;
};

struct BLOCK_HEADER_RESPONSE
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(block_header)
        KV_MEMBER(status)
    }

    std::string status;
    block_header_response block_header;
};

struct f_transaction_short_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(hash)
        KV_MEMBER(fee)
        KV_MEMBER(amount_out)
        KV_MEMBER(size)
    }

    std::string hash;
    uint64_t fee;
    uint64_t amount_out;
    uint64_t size;
};

struct f_transaction_details_extra_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(padding)
        KV_MEMBER(publicKey)
        KV_MEMBER(nonce)
        KV_MEMBER(raw)
    }

    std::vector<size_t> padding;
    Crypto::PublicKey publicKey;
    std::vector<std::string> nonce;
    std::vector<uint8_t> raw;
};

struct f_transaction_details_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(hash)
        KV_MEMBER(size)
        KV_MEMBER(paymentId)
        KV_MEMBER(mixin)
        KV_MEMBER(fee)
        KV_MEMBER(amount_out)
        KV_MEMBER(confirmations)
        KV_MEMBER(extra)
    }

    std::string hash;
    size_t size;
    std::string paymentId;
    uint64_t mixin;
    uint64_t fee;
    uint64_t amount_out;
    uint32_t confirmations = 0;
    f_transaction_details_extra_response extra;
};

struct f_mempool_transaction_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(hash)
        KV_MEMBER(fee)
        KV_MEMBER(amount_out)
        KV_MEMBER(size)
        KV_MEMBER(receiveTime)
        KV_MEMBER(keptByBlock)
        KV_MEMBER(max_used_block_height)
        KV_MEMBER(max_used_block_id)
        KV_MEMBER(last_failed_height)
        KV_MEMBER(last_failed_id)
    }

    std::string hash;
    uint64_t fee;
    uint64_t amount_out;
    uint64_t size;
    uint64_t receiveTime;
    bool keptByBlock;
    uint32_t max_used_block_height;
    std::string max_used_block_id;
    uint32_t last_failed_height;
    std::string last_failed_id;
};

struct f_block_short_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(timestamp)
        KV_MEMBER(height)
        KV_MEMBER(hash)
        KV_MEMBER(cumul_size)
        KV_MEMBER(tx_count)
        KV_MEMBER(difficulty)
        KV_MEMBER(min_tx_fee)
    }

    uint64_t timestamp;
    uint32_t height;
    std::string hash;
    uint64_t tx_count;
    uint64_t cumul_size;
    difficulty_type difficulty;
    uint64_t min_tx_fee;
};

struct f_block_details_response
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(major_version)
        KV_MEMBER(minor_version)
        KV_MEMBER(timestamp)
        KV_MEMBER(prev_hash)
        KV_MEMBER(nonce)
        KV_MEMBER(orphan_status)
        KV_MEMBER(height)
        KV_MEMBER(depth)
        KV_MEMBER(hash)
        KV_MEMBER(difficulty)
        KV_MEMBER(cumulativeDifficulty)
        KV_MEMBER(reward)
        KV_MEMBER(blockSize)
        KV_MEMBER(sizeMedian)
        KV_MEMBER(effectiveSizeMedian)
        KV_MEMBER(transactionsCumulativeSize)
        KV_MEMBER(alreadyGeneratedCoins)
        KV_MEMBER(alreadyGeneratedTransactions)
        KV_MEMBER(baseReward)
        KV_MEMBER(penalty)
        KV_MEMBER(totalFeeAmount)
        KV_MEMBER(transactions)
    }

    uint8_t major_version;
    uint8_t minor_version;
    uint64_t timestamp;
    std::string prev_hash;
    uint32_t nonce;
    bool orphan_status;
    uint64_t height;
    uint64_t depth;
    std::string hash;
    difficulty_type difficulty;
    difficulty_type cumulativeDifficulty;
    uint64_t reward;
    uint64_t blockSize;
    size_t sizeMedian;
    uint64_t effectiveSizeMedian;
    uint64_t transactionsCumulativeSize;
    std::string alreadyGeneratedCoins;
    uint64_t alreadyGeneratedTransactions;
    uint64_t baseReward;
    double penalty;
    uint64_t totalFeeAmount;
    std::vector<f_transaction_short_response> transactions;
};

struct COMMAND_RPC_GET_LAST_BLOCK_HEADER
{
    typedef EMPTY_STRUCT request;
    typedef BLOCK_HEADER_RESPONSE response;
};

struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH
{
    typedef BLOCK_HEADER_RESPONSE response;

    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(hash)
        }

        std::string hash;
    };
};

struct COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT
{
    typedef BLOCK_HEADER_RESPONSE response;

    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
        }

        uint64_t height;
    };
};

struct F_COMMAND_RPC_GET_BLOCKS_LIST
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
        }

        uint64_t height;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(blocks)
            KV_MEMBER(status)
        }

        std::vector<f_block_short_response> blocks; // transactions blobs as hex
        std::string status;
    };
};

struct F_COMMAND_RPC_GET_BLOCK_DETAILS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(hash)
        }

        std::string hash;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(block)
            KV_MEMBER(status)
        }

        f_block_details_response block;
        std::string status;
    };
};

struct K_COMMAND_RPC_GET_TRANSACTIONS_BY_PAYMENT_ID
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(payment_id)
        }

        std::string payment_id;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(transactions)
            KV_MEMBER(status)
        }

        std::vector<f_transaction_short_response> transactions;
        std::string status;
    };
};

struct F_COMMAND_RPC_GET_TRANSACTION_DETAILS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(hash)
        }

        std::string hash;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx)
            KV_MEMBER(txDetails)
            KV_MEMBER(block)
            KV_MEMBER(status)
        }

        Transaction tx;
        f_transaction_details_response txDetails;
        f_block_short_response block;
        std::string status;
    };
};

struct F_COMMAND_RPC_GET_POOL
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(transactions)
            KV_MEMBER(status)
        }

        std::vector<f_transaction_short_response> transactions;
        std::string status;
    };
};

struct COMMAND_RPC_GET_MEMPOOL
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(mempool)
            KV_MEMBER(status)
        }

        std::vector<f_mempool_transaction_response> mempool;
        std::string status;
    };
};

struct COMMAND_RPC_QUERY_BLOCKS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            serializeAsBinary(block_ids, "block_ids", s);
            KV_MEMBER(timestamp)
        }

        /*!
            First 10 blocks id goes sequential, next goes in pow(2,n) offset,
            like 2, 4, 8, 16, 32, 64 and so on, and the last one is always
            genesis block
        */
        std::vector<Crypto::Hash> block_ids;
        uint64_t timestamp;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(start_height)
            KV_MEMBER(current_height)
            KV_MEMBER(full_offset)
            KV_MEMBER(items)
        }

        std::string status;
        uint64_t start_height;
        uint64_t current_height;
        uint64_t full_offset;
        std::vector<BlockFullInfo> items;
    };
};

struct COMMAND_RPC_QUERY_BLOCKS_LITE
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            serializeAsBinary(blockIds, "block_ids", s);
            KV_MEMBER(timestamp)
        }

        std::vector<Crypto::Hash> blockIds;
        uint64_t timestamp;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(startHeight)
            KV_MEMBER(currentHeight)
            KV_MEMBER(fullOffset)
            KV_MEMBER(items)
        }

        std::string status;
        uint64_t startHeight;
        uint64_t currentHeight;
        uint64_t fullOffset;
        std::vector<BlockShortInfo> items;
    };
};

struct COMMAND_RPC_QUERY_BLOCKS_DETAILED
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(blockIds)
            KV_MEMBER(timestamp)
        }

        std::vector<Crypto::Hash> blockIds;
        uint64_t timestamp;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(startHeight)
            KV_MEMBER(currentHeight)
            KV_MEMBER(fullOffset)
            KV_MEMBER(blocks)
        }

        std::string status;
        uint64_t startHeight;
        uint64_t currentHeight;
        uint64_t fullOffset;
        std::vector<BlockFullInfo> blocks;
    };
};

struct COMMAND_RPC_GEN_PAYMENT_ID
{
    typedef EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(payment_id)
        }

        std::string payment_id;
    };
};

struct K_COMMAND_RPC_CHECK_TX_KEY
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(txid)
            KV_MEMBER(txkey)
            KV_MEMBER(address)
        }

        std::string txid;
        std::string txkey;
        std::string address;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(amount)
            KV_MEMBER(outputs)
            KV_MEMBER(status)
        }

        uint64_t amount;
        std::vector<TransactionOutput> outputs;
        std::string status;
    };
};

struct K_COMMAND_RPC_CHECK_TX_WITH_PRIVATE_VIEW_KEY
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(txid)
            KV_MEMBER(view_key)
            KV_MEMBER(address)
        }

        std::string txid;
        std::string view_key;
        std::string address;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(amount)
            KV_MEMBER(outputs)
            KV_MEMBER(confirmations)
            KV_MEMBER(status)
        }

        uint64_t amount;
        std::vector<TransactionOutput> outputs;
        uint32_t confirmations = 0;
        std::string status;
    };
};

struct COMMAND_RPC_VALIDATE_ADDRESS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(address)
        }

        std::string address;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(isvalid)
            KV_MEMBER(address)
            KV_MEMBER(spendPublicKey)
            KV_MEMBER(viewPublicKey)
            KV_MEMBER(status)
        }

        bool isvalid;
        std::string address;
        std::string spendPublicKey;
        std::string viewPublicKey;
        std::string status;
    };
};

struct COMMAND_RPC_VERIFY_MESSAGE
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(message)
            KV_MEMBER(address)
            KV_MEMBER(signature)
        }

        std::string message;
        std::string address;
        std::string signature;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(sig_valid)
            KV_MEMBER(status)
        }

        bool sig_valid;
        std::string status;
    };
};

struct COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(blockHeights)
        }

        std::vector<uint32_t> blockHeights;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(blocks)
        }

        std::vector<BlockDetails2> blocks;
        std::string status;
    };
};

struct COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(blockHashes)
        }

        std::vector<Crypto::Hash> blockHashes;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(blocks)
        }

        std::vector<BlockDetails2> blocks;
        std::string status;
    };
};

struct COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(blockHeight)
        }

        uint32_t blockHeight;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(block)
        }

        BlockDetails2 block;
        std::string status;
    };
};

struct COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(timestampBegin)
            KV_MEMBER(timestampEnd)
            KV_MEMBER(limit)
        }

        uint64_t timestampBegin;
        uint64_t timestampEnd;
        uint32_t limit;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(count)
            KV_MEMBER(blockHashes)
        }

        std::vector<Crypto::Hash> blockHashes;
        uint32_t count;
        std::string status;
    };
};

struct COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(paymentId)
        }

        Crypto::Hash paymentId;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(transactionHashes)
        }

        std::vector<Crypto::Hash> transactionHashes;
        std::string status;
    };
};

struct COMMAND_RPC_GET_TRANSACTIONS_DETAILS_BY_HASHES
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(transactionHashes)
        }

        std::vector<Crypto::Hash> transactionHashes;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(transactions)
        }

        std::vector<TransactionDetails2> transactions;
        std::string status;
    };
};

struct COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASH
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(hash)
        }

        Crypto::Hash hash;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(transaction)
        }

        TransactionDetails2 transaction;
        std::string status;
    };
};

struct difficulty_statistics
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(block_num)
        KV_MEMBER(avg_solve_time)
        KV_MEMBER(stddev_solve_time)
        KV_MEMBER(outliers_num)
        KV_MEMBER(avg_diff)
        KV_MEMBER(min_diff)
        KV_MEMBER(max_diff)
    }

    uint32_t block_num;
    uint64_t avg_solve_time;
    uint64_t stddev_solve_time;
    uint32_t outliers_num;
    difficulty_type avg_diff;
    difficulty_type min_diff;
    difficulty_type max_diff;
};

struct COMMAND_RPC_GET_DIFFICULTY_STAT
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
        }

        uint32_t height;

    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(status)
            KV_MEMBER(hour)
            KV_MEMBER(day)
            KV_MEMBER(week)
            KV_MEMBER(month)
            KV_MEMBER(halfyear)
            KV_MEMBER(year)
        }

        std::string status;
        difficulty_statistics hour;
        difficulty_statistics day;
        difficulty_statistics week;
        difficulty_statistics month;
        difficulty_statistics halfyear;
        difficulty_statistics year;
    };
};

struct reserve_proof_entry
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(txid)
        KV_MEMBER(index_in_tx)
        KV_MEMBER(shared_secret)
        KV_MEMBER(key_image)
        KV_MEMBER(shared_secret_sig)
        KV_MEMBER(key_image_sig)
    }

    Crypto::Hash txid;
    uint64_t index_in_tx;
    Crypto::PublicKey shared_secret;
    Crypto::KeyImage key_image;
    Crypto::Signature shared_secret_sig;
    Crypto::Signature key_image_sig;
};

struct reserve_proof
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(proofs)
        KV_MEMBER(signature)
    }

    std::vector<reserve_proof_entry> proofs;
    Crypto::Signature signature;
};

struct K_COMMAND_RPC_CHECK_TX_PROOF
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_id)
            KV_MEMBER(dest_address)
            KV_MEMBER(signature)
        }

        std::string tx_id;
        std::string dest_address;
        std::string signature;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(signature_valid)
            KV_MEMBER(received_amount)
            KV_MEMBER(outputs)
            KV_MEMBER(confirmations)
            KV_MEMBER(status)
        }

        bool signature_valid;
        uint64_t received_amount;
        std::vector<TransactionOutput> outputs;
        uint32_t confirmations = 0;
        std::string status;
    };
};

struct K_COMMAND_RPC_CHECK_RESERVE_PROOF
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(address)
            KV_MEMBER(message)
            KV_MEMBER(signature)
        }

        std::string address;
        std::string message;
        std::string signature;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(good)
            KV_MEMBER(total)
            KV_MEMBER(spent)
        }

        bool good;
        uint64_t total;
        uint64_t spent;
    };
};

} // namespace CryptoNote
