// Copyright (c) 2014-2022, The Monero Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once


#define TX_EXTRA_PADDING_MAX_COUNT          255
#define TX_EXTRA_NONCE_MAX_COUNT            255

#define TX_EXTRA_TAG_PADDING                0x00
#define TX_EXTRA_TAG_PUBKEY                 0x01
#define TX_EXTRA_NONCE                      0x02
#define TX_EXTRA_MERGE_MINING_TAG           0x03
#define TX_EXTRA_TAG_ADDITIONAL_PUBKEYS     0x04
#define TX_EXTRA_TAG_EPOSE_V2               0x05
#define TX_EXTRA_TAG_HEIGHT_WINDOW          0x06
#define TX_EXTRA_TAG_VM_STATE_ROOT          0x07
#define TX_EXTRA_TAG_VM_CALL                0x08
#define TX_EXTRA_TAG_CHAINLOCK              0x09
#define TX_EXTRA_MYSTERIOUS_MINERGATE_TAG   0xDE

#define TX_EXTRA_NONCE_PAYMENT_ID           0x00
#define TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID 0x01

namespace cryptonote
{
  struct tx_extra_padding
  {
    size_t size;

    // load
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<false>& ar)
    {
      // size - 1 - because of variant tag
      for (size = 1; size <= TX_EXTRA_PADDING_MAX_COUNT; ++size)
      {
        if (ar.eof())
          break;

        uint8_t zero;
        if (!::do_serialize(ar, zero))
          return false;

        if (0 != zero)
          return false;
      }

      return size <= TX_EXTRA_PADDING_MAX_COUNT;
    }

    // store
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<true>& ar)
    {
      if(TX_EXTRA_PADDING_MAX_COUNT < size)
        return false;

      // i = 1 - because of variant tag
      for (size_t i = 1; i < size; ++i)
      {
        uint8_t zero = 0;
        if (!::do_serialize(ar, zero))
          return false;
      }
      return true;
    }
  };

  struct tx_extra_pub_key
  {
    crypto::public_key pub_key;

    BEGIN_SERIALIZE()
      FIELD(pub_key)
    END_SERIALIZE()
  };

  struct tx_extra_nonce
  {
    std::string nonce;

    BEGIN_SERIALIZE()
      FIELD(nonce)
      if(TX_EXTRA_NONCE_MAX_COUNT < nonce.size()) return false;
    END_SERIALIZE()
  };

  struct tx_extra_merge_mining_tag
  {
    struct serialize_helper
    {
      tx_extra_merge_mining_tag& mm_tag;

      serialize_helper(tx_extra_merge_mining_tag& mm_tag_) : mm_tag(mm_tag_)
      {
      }

      BEGIN_SERIALIZE()
        VARINT_FIELD_N("depth", mm_tag.depth)
        FIELD_N("merkle_root", mm_tag.merkle_root)
      END_SERIALIZE()
    };

    size_t depth;
    crypto::hash merkle_root;

    // load
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<false>& ar)
    {
      std::string field;
      if(!::do_serialize(ar, field))
        return false;

      binary_archive<false> iar{epee::strspan<std::uint8_t>(field)};
      serialize_helper helper(*this);
      return ::serialization::serialize(iar, helper);
    }

    // store
    template <template <bool> class Archive>
    bool member_do_serialize(Archive<true>& ar)
    {
      std::ostringstream oss;
      binary_archive<true> oar(oss);
      serialize_helper helper(*this);
      if(!::do_serialize(oar, helper))
        return false;

      std::string field = oss.str();
      return ::serialization::serialize(ar, field);
    }
  };

  // per-output additional tx pubkey for multi-destination transfers involving at least one subaddress
  struct tx_extra_additional_pub_keys
  {
    std::vector<crypto::public_key> data;

    BEGIN_SERIALIZE()
      FIELD(data)
    END_SERIALIZE()
  };

  // Dedicated, length-delimited EPoSE v2 carrier. The payload is the complete
  // QEP2 envelope. Consensus applies the tighter EPoSE limits before any
  // record-specific cryptography; this generic type only preserves framing.
  struct tx_extra_epose_v2
  {
    std::string data;

    BEGIN_SERIALIZE()
      FIELD(data)
    END_SERIALIZE()
  };

  struct tx_extra_height_window
  {
    uint64_t from;
    uint64_t until;

    BEGIN_SERIALIZE()
      VARINT_FIELD(from)
      VARINT_FIELD(until)
    END_SERIALIZE()
  };

  struct tx_extra_vm_state_root
  {
    crypto::hash root;

    BEGIN_SERIALIZE()
      FIELD(root)
    END_SERIALIZE()
  };

  struct tx_extra_vm_mint_destination
  {
    crypto::public_key key;
    crypto::view_tag view_tag;
    crypto::public_key tx_key;

    BEGIN_SERIALIZE()
      FIELD(key)
      FIELD(view_tag)
      FIELD(tx_key)
    END_SERIALIZE()
  };

  struct tx_extra_vm_signature
  {
    crypto::signature value;

    BEGIN_SERIALIZE()
      FIELD(value)
    END_SERIALIZE()
  };

  struct tx_extra_vm_call
  {
    enum : uint32_t { kind_deploy = 0, kind_invoke = 1 };

    uint32_t version = 1;
    uint32_t kind;
    crypto::hash contract_id;
    uint32_t method;
    std::string code;
    std::string args;
    uint64_t gas_limit;
    uint64_t gas_price;
    uint64_t deposit;
    std::vector<tx_extra_vm_mint_destination> mint;
    std::vector<crypto::public_key> signers;
    std::vector<crypto::hash> signer_contracts;
    std::vector<uint32_t> signer_methods;
    std::vector<crypto::hash> signer_key_ids;
    std::vector<uint8_t> signer_nested;
    std::vector<tx_extra_vm_signature> signatures;

    BEGIN_SERIALIZE()
      VARINT_FIELD(version)
      VARINT_FIELD(kind)
      FIELD(contract_id)
      VARINT_FIELD(method)
      FIELD(code)
      FIELD(args)
      VARINT_FIELD(gas_limit)
      VARINT_FIELD(gas_price)
      VARINT_FIELD(deposit)
      FIELD(mint)
      FIELD(signers)
      FIELD(signer_contracts)
      FIELD(signer_methods)
      FIELD(signer_key_ids)
      FIELD(signer_nested)
      FIELD(signatures)
    END_SERIALIZE()
  };

  struct tx_extra_chainlock
  {
    std::string blob;

    BEGIN_SERIALIZE()
      FIELD(blob)
    END_SERIALIZE()
  };

  enum class contract_extra_scope
  {
    coinbase,
    transaction,
    either
  };

  struct tx_extra_mysterious_minergate
  {
    std::string data;

    BEGIN_SERIALIZE()
      FIELD(data)
    END_SERIALIZE()
  };

  // tx_extra_field format, except tx_extra_padding and tx_extra_pub_key:
  //   varint tag;
  //   varint size;
  //   varint data[];
  typedef boost::variant<tx_extra_padding, tx_extra_pub_key, tx_extra_nonce, tx_extra_merge_mining_tag, tx_extra_additional_pub_keys, tx_extra_epose_v2, tx_extra_height_window, tx_extra_vm_state_root, tx_extra_vm_call, tx_extra_chainlock, tx_extra_mysterious_minergate> tx_extra_field;
}

VARIANT_TAG(binary_archive, cryptonote::tx_extra_padding, TX_EXTRA_TAG_PADDING);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_pub_key, TX_EXTRA_TAG_PUBKEY);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_nonce, TX_EXTRA_NONCE);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_merge_mining_tag, TX_EXTRA_MERGE_MINING_TAG);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_additional_pub_keys, TX_EXTRA_TAG_ADDITIONAL_PUBKEYS);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_epose_v2, TX_EXTRA_TAG_EPOSE_V2);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_height_window, TX_EXTRA_TAG_HEIGHT_WINDOW);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_vm_state_root, TX_EXTRA_TAG_VM_STATE_ROOT);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_vm_call, TX_EXTRA_TAG_VM_CALL);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_chainlock, TX_EXTRA_TAG_CHAINLOCK);
VARIANT_TAG(binary_archive, cryptonote::tx_extra_mysterious_minergate, TX_EXTRA_MYSTERIOUS_MINERGATE_TAG);
