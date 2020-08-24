// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016, The Monero Project
// Copyright (c) 2016-2018, Karbo developers
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
#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h>
#include <crypto/hash.h>
#include <Global/CryptoNoteConfig.h>
#include <Rpc/CoreRpcServerCommandsDefinitions.h>
#include <Wallet/WalletRpcServerErrorCodes.h>

#define WALLET_RPC_STATUS_OK "OK"
#define WALLET_RPC_STATUS_BUSY "BUSY"

namespace Tools {

namespace wallet_rpc {

using CryptoNote::ISerializer;

// command: get_balance
struct COMMAND_RPC_GET_BALANCE
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(locked_amount)
            KV_MEMBER(available_balance)
        }

        uint64_t locked_amount;
        uint64_t available_balance;
    };
};

// command: transfer
struct transfer_destination
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(amount)
        KV_MEMBER(address)
        KV_MEMBER(message)
    }

    uint64_t amount;
    std::string address;
    std::string message;
};

struct TransferMessage
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(address)
        KV_MEMBER(message)
    }

    std::string address;
    std::string message;
};

struct COMMAND_RPC_TRANSFER
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(destinations)
            KV_MEMBER(fee)
            KV_MEMBER(mixin)
            KV_MEMBER(unlock_time)
            KV_MEMBER(payment_id)
            KV_MEMBER(messages)
            KV_MEMBER(ttl)
        }

        std::list<transfer_destination> destinations;
        uint64_t fee = CryptoNote::parameters::MINIMUM_FEE_V2;
        uint64_t mixin = 0;
        uint64_t unlock_time = 0;
        std::string payment_id;
        std::list<TransferMessage> messages;
        uint64_t ttl = 0;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_hash)
            KV_MEMBER(tx_key)
        }

        std::string tx_hash;
        std::string tx_key;
    };
};

// command: store
struct COMMAND_RPC_STORE
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(stored)
        }

        bool stored;
    };
};

// command: stop_wallet
struct COMMAND_RPC_STOP
{
    typedef CryptoNote::EMPTY_STRUCT request;
    typedef CryptoNote::EMPTY_STRUCT response;
};

// command: get_payments
struct payment_details
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(tx_hash)
        KV_MEMBER(amount)
        KV_MEMBER(block_height)
        KV_MEMBER(unlock_time)
    }

    std::string tx_hash;
    uint64_t amount;
    uint64_t block_height;
    uint64_t unlock_time;
};

struct transaction_messages
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(tx_hash)
        KV_MEMBER(tx_id)
        KV_MEMBER(block_height)
        KV_MEMBER(timestamp)
        KV_MEMBER(messages)
    }

    std::string tx_hash;
    uint64_t tx_id;
    uint32_t block_height;
    uint64_t timestamp;
    std::list<std::string> messages;
};

struct COMMAND_RPC_GET_MESSAGES
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(first_tx_id)
            KV_MEMBER(tx_limit)
        }

        uint64_t first_tx_id = 0;
        uint32_t tx_limit = std::numeric_limits<uint32_t>::max();
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(total_tx_count)
            KV_MEMBER(tx_messages)
        }

        uint64_t total_tx_count;
        std::list<transaction_messages> tx_messages;
    };
};

struct COMMAND_RPC_GET_PAYMENTS
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
            KV_MEMBER(payments)
        }

        std::list<payment_details> payments;
    };
};

// command: get_transfers
struct Transfer
{
    void serialize(ISerializer &s)
    {
        KV_MEMBER(time)
        KV_MEMBER(output)
        KV_MEMBER(transactionHash)
        KV_MEMBER(amount)
        KV_MEMBER(fee)
        KV_MEMBER(paymentId)
        KV_MEMBER(address)
        KV_MEMBER(blockIndex)
        KV_MEMBER(unlockTime)
        KV_MEMBER(confirmations)
        KV_MEMBER(txKey)
    }

    uint64_t time;
    bool output;
    std::string transactionHash;
    uint64_t amount;
    uint64_t fee;
    std::string paymentId;
    std::string address;
    uint64_t blockIndex;
    uint64_t unlockTime;
    uint64_t confirmations;
    std::string txKey;
};

struct COMMAND_RPC_GET_TRANSFERS
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(transfers)
        }

        std::list<Transfer> transfers;
    };
};

// command: get_transaction
struct COMMAND_RPC_GET_TRANSACTION
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_hash)
        }

        std::string tx_hash;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(transaction_details)
            KV_MEMBER(destinations)
        }

        Transfer transaction_details;
        std::list<transfer_destination> destinations;
    };
};

struct COMMAND_RPC_GET_HEIGHT
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
        }

        uint64_t height;
    };
};

// command: reset
struct COMMAND_RPC_RESCAN
{
    typedef CryptoNote::EMPTY_STRUCT request;
    typedef CryptoNote::EMPTY_STRUCT response;
};

// command: purge
struct COMMAND_RPC_PURGE
{
    typedef CryptoNote::EMPTY_STRUCT request;
    typedef CryptoNote::EMPTY_STRUCT response;
};

// command: query_key
struct COMMAND_RPC_QUERY_KEY
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(key_type)
        }

        std::string key_type;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(key)
        }

        std::string key;
    };
};

// command: get_address
struct COMMAND_RPC_GET_ADDRESS
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(address)
        }

        std::string address;
    };
};

// command: paymentid
struct COMMAND_RPC_GEN_PAYMENT_ID
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(payment_id)
        }

        std::string payment_id;
    };
};

// command: get_tx_key
struct COMMAND_RPC_GET_TX_KEY
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_hash)
        }

        std::string tx_hash;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_key)
        }

        std::string tx_key;
    };
};

struct COMMAND_RPC_SIGN_MESSAGE
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(message);
        }

        std::string message;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(signature);
        }

        std::string signature;
    };
};

struct COMMAND_RPC_VERIFY_MESSAGE
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(message);
            KV_MEMBER(address);
            KV_MEMBER(signature);
        }

        std::string message;
        std::string address;
        std::string signature;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(good);
        }

        bool good;
    };
};

struct COMMAND_RPC_CHANGE_PASSWORD
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(old_password);
            KV_MEMBER(new_password);
        }

        std::string old_password;
        std::string new_password;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(password_changed);
        }

        bool password_changed;
    };
};

struct COMMAND_RPC_GET_OUTPUTS
{
    typedef CryptoNote::EMPTY_STRUCT request;

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(unlocked_outputs_count)
        }

        size_t unlocked_outputs_count;
    };
};

struct COMMAND_RPC_GET_TX_PROOF
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_hash);
            KV_MEMBER(dest_address);
            KV_MEMBER(tx_key);
        }

        std::string tx_hash;
        std::string dest_address;
        std::string tx_key;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(signature);
        }

        std::string signature;
    };
};

struct COMMAND_RPC_GET_BALANCE_PROOF
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(amount);
            KV_MEMBER(message);
        }

        uint64_t amount = 0;
        std::string message;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(signature);
        }

        std::string signature;
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

/* Fusion transactions */

struct COMMAND_RPC_ESTIMATE_FUSION
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(threshold)
        }

        uint64_t threshold;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(fusion_ready_count)
        }

        size_t fusion_ready_count;
    };
};

struct COMMAND_RPC_SEND_FUSION
{
    struct request
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(mixin)
            KV_MEMBER(threshold)
            KV_MEMBER(unlock_time)
        }

        uint64_t mixin = 0;
        uint64_t threshold;
        uint64_t unlock_time = 0;
    };

    struct response
    {
        void serialize(ISerializer &s)
        {
            KV_MEMBER(tx_hash)
        }

        std::string tx_hash;
    };
};

} // namespace wallet_rpc

} // namespace Tools
