// Copyright (c) 2014-2022, The Monero Project
// Copyright (c) 2026, The Qwertycoin Project
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

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/uuid/nil_generator.hpp>

#include <algorithm>
#include <atomic>

#include "string_tools.h"
using namespace epee;

#include <unordered_set>
#include "cryptonote_core.h"
#include "common/util.h"
#include "common/updates.h"
#include "common/download.h"
#include "common/threadpool.h"
#include "common/command_line.h"
#include "cryptonote_basic/events.h"
#include "warnings.h"
#include "crypto/crypto.h"
#include "cryptonote_config.h"
#include "misc_language.h"
#include "file_io_utils.h"
#include <csignal>
#include "checkpoints/checkpoints.h"
#include "ringct/rctTypes.h"
#include "blockchain_db/blockchain_db.h"
#include "ringct/rctSigs.h"
#include "rpc/zmq_pub.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "net/http_client.h"
#include "net/net_parse_helpers.h"
#include "storages/http_abstract_invoke.h"
#include "common/notify.h"
#include "hardforks/hardforks.h"
#include "tx_verification_utils.h"
#include "version.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/attestation_pool.h"
#include "epose/envelope_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_epoch.h"

#include <boost/filesystem.hpp>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "cn"

DISABLE_VS_WARNINGS(4355)

#define MERROR_VER(x) MCERROR("verify", x)

// basically at least how many bytes the block itself serializes to without the miner tx
#define BLOCK_SIZE_SANITY_LEEWAY 100

namespace
{
  constexpr size_t EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES = 2 * 1024 * 1024;
  constexpr uint64_t EPOSE_V2_PROBE_TIMEOUT_MS = 5000;

  crypto::hash epose_v2_receipt_slot_hash(
      const uint64_t epoch,
      const uint64_t round,
      const crypto::public_key &subject,
      const crypto::public_key &verifier,
      const crypto::hash &anchor)
  {
    std::string blob("QWC_EPOSE_RECEIPT_SLOT_V2");
    const auto append = [&blob](const auto &value) {
      blob.append(reinterpret_cast<const char *>(&value), sizeof(value));
    };
    append(epoch);
    append(round);
    append(subject);
    append(verifier);
    append(anchor);
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  crypto::hash epose_v2_challenge_nonce(
      const crypto::hash &slot,
      const crypto::hash &snapshot_hash,
      const crypto::hash &endpoint_hash)
  {
    std::string blob("QWC_EPOSE_CHALLENGE_NONCE_V2");
    blob.append(reinterpret_cast<const char *>(&slot), sizeof(slot));
    blob.append(reinterpret_cast<const char *>(&snapshot_hash), sizeof(snapshot_hash));
    blob.append(reinterpret_cast<const char *>(&endpoint_hash), sizeof(endpoint_hash));
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  uint64_t epose_v2_steady_milliseconds()
  {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return elapsed < 0 ? 0 : static_cast<uint64_t>(elapsed);
  }

  bool parse_epose_discovery_url(
      const std::string &url, std::string &host, uint16_t &port)
  {
    host.clear();
    port = 0;
    epee::net_utils::http::url_content parsed{};
    if (!epee::net_utils::parse_url(url, parsed)
        || parsed.schema != "http" || parsed.host.empty()
        || parsed.port == 0 || parsed.port > std::numeric_limits<uint16_t>::max()
        || (!parsed.uri.empty() && parsed.uri != "/"))
      return false;
    host = parsed.host;
    port = static_cast<uint16_t>(parsed.port);
    return true;
  }

  bool fetch_epose_endpoint_descriptor(
      const std::string &url,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const crypto::hash &required_hash,
      qwertycoin::epose::endpoint_descriptor_v2 &descriptor)
  {
    descriptor = {};
    std::string host;
    uint16_t port = 0;
    if (!parse_epose_discovery_url(url, host, port))
      return false;
    epee::net_utils::http::http_simple_client client;
    client.set_server(host, std::to_string(port), boost::none);
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::request request{};
    request.descriptor_hash = epee::string_tools::pod_to_hex(required_hash);
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response response{};
    if (!epee::net_utils::invoke_http_json(
            "/get_epose_service_endpoint_v2", request, response, client,
            std::chrono::milliseconds(EPOSE_V2_PROBE_TIMEOUT_MS))
        || response.status != CORE_RPC_STATUS_OK || !response.ready
        || response.version > std::numeric_limits<uint8_t>::max()
        || response.transport > std::numeric_limits<uint8_t>::max()
        || response.port > std::numeric_limits<uint16_t>::max()
        || response.service_kind > std::numeric_limits<uint8_t>::max()
        || response.service_version > std::numeric_limits<uint8_t>::max())
      return false;
    descriptor.version = static_cast<uint8_t>(response.version);
    descriptor.transport = static_cast<qwertycoin::epose::endpoint_transport_v2>(response.transport);
    descriptor.host = response.host;
    descriptor.port = static_cast<uint16_t>(response.port);
    descriptor.service_kind = static_cast<uint8_t>(response.service_kind);
    descriptor.service_version = static_cast<uint8_t>(response.service_version);
    descriptor.sequence = response.sequence;
    descriptor.expiry_epoch = response.expiry_epoch;
    if (!epee::string_tools::hex_to_pod(
            response.service_public_key, descriptor.service_public_key)
        || !epee::string_tools::hex_to_pod(response.signature, descriptor.signature)
        || qwertycoin::epose::validate_endpoint_descriptor_v2(
               parameters.nettype, parameters.genesis_hash,
               parameters.parameter_set_hash, descriptor)
            != qwertycoin::epose::resource_status_v2::accepted
        || descriptor.host != host || descriptor.port != port)
    {
      descriptor = {};
      return false;
    }
    const crypto::hash descriptor_hash =
        qwertycoin::epose::hash_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, descriptor);
    crypto::hash advertised_hash{};
    if (!epee::string_tools::hex_to_pod(
            response.descriptor_hash, advertised_hash)
        || advertised_hash != descriptor_hash || descriptor_hash != required_hash)
    {
      descriptor = {};
      return false;
    }
    return true;
  }

  bool request_epose_service_response(
      const qwertycoin::epose::service_challenge_v2 &challenge,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      qwertycoin::epose::canonical_service_response_v2 &service_response)
  {
    service_response = {};
    epee::net_utils::http::http_simple_client client;
    client.set_server(
        endpoint.host, std::to_string(endpoint.port), boost::none);
    cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::request request{};
    request.version = challenge.version;
    request.service_kind = challenge.service_kind;
    request.epoch = challenge.epoch;
    request.round = challenge.round;
    request.snapshot_hash = epee::string_tools::pod_to_hex(challenge.snapshot_hash);
    request.anchor_hash = epee::string_tools::pod_to_hex(challenge.anchor_hash);
    request.subject_public_key = epee::string_tools::pod_to_hex(challenge.subject_public_key);
    request.verifier_public_key = epee::string_tools::pod_to_hex(challenge.verifier_public_key);
    request.endpoint_descriptor_hash = epee::string_tools::pod_to_hex(challenge.endpoint_descriptor_hash);
    request.nonce = epee::string_tools::pod_to_hex(challenge.nonce);
    request.requested_object_hash = epee::string_tools::pod_to_hex(challenge.requested_object_hash);
    cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::response response{};
    if (!epee::net_utils::invoke_http_json(
            "/epose_service_challenge_v2", request, response, client,
            std::chrono::milliseconds(EPOSE_V2_PROBE_TIMEOUT_MS))
        || response.status != CORE_RPC_STATUS_OK
        || response.block_blob.size() / 2
            > EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES
        || !epee::string_tools::parse_hexstr_to_binbuff(
            response.block_blob, service_response.block_blob)
        || !epee::string_tools::hex_to_pod(
            response.subject_signature, service_response.subject_signature))
    {
      service_response = {};
      return false;
    }
    return true;
  }
}

namespace cryptonote
{
  const command_line::arg_descriptor<bool, false> arg_testnet_on  = {
    "testnet"
  , "Run on testnet. The wallet must be launched with --testnet flag."
  , false
  };
  const command_line::arg_descriptor<bool, false> arg_stagenet_on  = {
    "stagenet"
  , "Run on stagenet. The wallet must be launched with --stagenet flag."
  , false
  };
  const command_line::arg_descriptor<bool> arg_regtest_on  = {
    "regtest"
  , "Run in a regression testing mode."
  , false
  };
  const command_line::arg_descriptor<bool> arg_keep_fakechain = {
    "keep-fakechain"
  , "Don't delete any existing database when in fakechain mode."
  , false
  };
  const command_line::arg_descriptor<difficulty_type> arg_fixed_difficulty  = {
    "fixed-difficulty"
  , "Fixed difficulty used for testing."
  , 0
  };
  const command_line::arg_descriptor<std::string, false, true, 3> arg_data_dir = {
    "data-dir"
  , "Specify data directory"
  , tools::get_default_data_dir()
  , {{ &arg_testnet_on, &arg_stagenet_on, &arg_regtest_on }}
  , [](std::array<bool, 3> nets, bool defaulted, std::string val)->std::string {
      if (nets[0])
        return (boost::filesystem::path(val) / "testnet").string();
      else if (nets[1])
        return (boost::filesystem::path(val) / "stagenet").string();
      else if (nets[2])
        return (boost::filesystem::path(val) / "fake").string();
      return val;
    }
  };
  const command_line::arg_descriptor<bool> arg_offline = {
    "offline"
  , "Do not listen for peers, nor connect to any"
  };
  const command_line::arg_descriptor<bool> arg_disable_dns_checkpoints = {
    "disable-dns-checkpoints"
  , "Do not retrieve checkpoints from DNS"
  };
  const command_line::arg_descriptor<size_t> arg_block_download_max_size  = {
    "block-download-max-size"
  , "Set maximum size of block download queue in bytes (0 for default)"
  , 0
  };
  const command_line::arg_descriptor<bool> arg_sync_pruned_blocks  = {
    "sync-pruned-blocks"
  , "Allow syncing from nodes with only pruned blocks"
  };
  const command_line::arg_descriptor<bool> arg_service_node = {
    "service-node"
  , "Run this daemon as a Qwertycoin EPoSE service node"
  , false
  };
  const command_line::arg_descriptor<std::string> arg_service_node_key = {
    "service-node-key"
  , "Path to the EPoSE service-node private key file"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_service_reward_address = {
    "service-reward-address"
  , "Primary Qwertycoin address receiving EPoSE service rewards"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_service_reward_view_key = {
    "service-reward-view-key"
  , "Private view key matching the EPoSE service reward address; it is disclosed in the on-chain registration for deterministic reward validation"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_service_node_advertise_address = {
    "service-node-advertise-address"
  , "Public host:port advertised by this EPoSE service node"
  , ""
  };
  const command_line::arg_descriptor<bool> arg_epose_v2_service = {
    "epose-v2-service"
  , "Run the genesis-bound QWC-HF17/EPoSE-v2 service producer"
  , false
  };
  const command_line::arg_descriptor<std::string> arg_epose_v2_keystore = {
    "epose-v2-keystore"
  , "Path to the genesis- and parameter-bound EPoSE-v2 operator/service keystore"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_epose_v2_reward_address = {
    "epose-v2-reward-address"
  , "Primary public Qwertycoin address receiving EPoSE-v2 service rewards"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_epose_v2_endpoint_host = {
    "epose-v2-endpoint-host"
  , "Canonical public IPv4, IPv6, or lowercase DNS host serving EPoSE-v2 probes"
  , ""
  };
  const command_line::arg_descriptor<uint16_t> arg_epose_v2_endpoint_port = {
    "epose-v2-endpoint-port"
  , "Public restricted-RPC port serving EPoSE-v2 probes"
  , 0
  };
  const command_line::arg_descriptor<std::vector<std::string>> arg_epose_v2_discovery_endpoint = {
    "epose-v2-discovery-endpoint"
  , "Public http://host:port endpoint used to discover signed EPoSE-v2 service descriptors (repeatable)"
  };

  static const command_line::arg_descriptor<bool> arg_test_drop_download = {
    "test-drop-download"
  , "For net tests: in download, discard ALL blocks instead checking/saving them (very fast)"
  };
  static const command_line::arg_descriptor<uint64_t> arg_test_drop_download_height = {
    "test-drop-download-height"
  , "Like test-drop-download but discards only after around certain height"
  , 0
  };
  static const command_line::arg_descriptor<int> arg_test_dbg_lock_sleep = {
    "test-dbg-lock-sleep"
  , "Sleep time in ms, defaults to 0 (off), used to debug before/after locking mutex. Values 100 to 1000 are good for tests."
  , 0
  };
  static const command_line::arg_descriptor<bool> arg_dns_checkpoints  = {
    "enforce-dns-checkpointing"
  , "checkpoints from DNS server will be enforced"
  , false
  };
  static const command_line::arg_descriptor<uint64_t> arg_fast_block_sync = {
    "fast-block-sync"
  , "Sync up most of the way by using embedded, known block hashes."
  , 1
  };
  static const command_line::arg_descriptor<uint64_t> arg_prep_blocks_threads = {
    "prep-blocks-threads"
  , "Max number of threads to use when preparing block hashes in groups."
  , 4
  };
  static const command_line::arg_descriptor<uint64_t> arg_show_time_stats  = {
    "show-time-stats"
  , "Show time-stats when processing blocks/txs and disk synchronization."
  , 0
  };
  static const command_line::arg_descriptor<size_t> arg_block_sync_size  = {
    "block-sync-size"
  , "How many blocks to sync at once during chain synchronization (0 = adaptive)."
  , 0
  };
  static const command_line::arg_descriptor<std::string> arg_check_updates = {
    "check-updates"
  , "Check for new versions of qwertycoin: [disabled|notify|download|update]"
  , "notify"
  };
  static const command_line::arg_descriptor<bool> arg_fluffy_blocks  = {
    "fluffy-blocks"
  , "Relay blocks as fluffy blocks (obsolete, now default)"
  , true
  };
  static const command_line::arg_descriptor<size_t> arg_max_txpool_weight  = {
    "max-txpool-weight"
  , "Set maximum txpool weight in bytes."
  , DEFAULT_TXPOOL_MAX_WEIGHT
  };
  static const command_line::arg_descriptor<std::string> arg_block_notify = {
    "block-notify"
  , "Run a program for each new block, '%s' will be replaced by the block hash"
  , ""
  };
  static const command_line::arg_descriptor<bool> arg_prune_blockchain  = {
    "prune-blockchain"
  , "Prune blockchain"
  , false
  };
  static const command_line::arg_descriptor<std::string> arg_reorg_notify = {
    "reorg-notify"
  , "Run a program for each reorg, '%s' will be replaced by the split height, "
    "'%h' will be replaced by the new blockchain height, '%n' will be "
    "replaced by the number of new blocks in the new chain, and '%d' will be "
    "replaced by the number of blocks discarded from the old chain"
  , ""
  };
  static const command_line::arg_descriptor<std::string> arg_block_rate_notify = {
    "block-rate-notify"
  , "Run a program when the block rate undergoes large fluctuations. This might "
    "be a sign of large amounts of hash rate going on and off the Qwertycoin network, "
    "and thus be of potential interest in predicting attacks. %t will be replaced "
    "by the number of minutes for the observation window, %b by the number of "
    "blocks observed within that window, and %e by the number of blocks that was "
    "expected in that window. It is suggested that this notification is used to "
    "automatically increase the number of confirmations required before a payment "
    "is acted upon."
  , ""
  };
  static const command_line::arg_descriptor<bool> arg_keep_alt_blocks  = {
    "keep-alt-blocks"
  , "Keep alternative blocks on restart"
  , false
  };

  //-----------------------------------------------------------------------------------------------
  core::core(i_cryptonote_protocol* pprotocol):
              m_mempool(m_blockchain_storage),
              m_blockchain_storage(m_mempool),
              m_miner(this, [this](const cryptonote::block &b, uint64_t height, const crypto::hash *seed_hash, unsigned int threads, crypto::hash &hash) {
                return cryptonote::get_block_longhash(&m_blockchain_storage, b, hash, height, seed_hash, threads);
              }),
              m_starter_message_showed(false),
              m_target_blockchain_height(0),
              m_checkpoints_path(""),
              m_last_dns_checkpoints_update(0),
              m_last_json_checkpoints_update(0),
              m_disable_dns_checkpoints(false),
              m_update_download(0),
              m_nettype(UNDEFINED),
              m_update_available(false)
  {
    m_checkpoints_updating.clear();
    set_cryptonote_protocol(pprotocol);
  }
  void core::set_cryptonote_protocol(i_cryptonote_protocol* pprotocol)
  {
    if(pprotocol)
      m_pprotocol = pprotocol;
    else
      m_pprotocol = &m_protocol_stub;
  }
  //-----------------------------------------------------------------------------------
  const checkpoints& core::get_checkpoints() const
  {
    return m_blockchain_storage.get_checkpoints();
  }
  void core::set_checkpoints(checkpoints&& chk_pts)
  {
    m_blockchain_storage.set_checkpoints(std::move(chk_pts));
  }
  //-----------------------------------------------------------------------------------
  void core::set_checkpoints_file_path(const std::string& path)
  {
    m_checkpoints_path = path;
  }
  //-----------------------------------------------------------------------------------
  void core::set_enforce_dns_checkpoints(bool enforce_dns)
  {
    m_blockchain_storage.set_enforce_dns_checkpoints(enforce_dns);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::update_checkpoints(const bool skip_dns /* = false */)
  {
    if (m_nettype != MAINNET || m_disable_dns_checkpoints) return true;

    if (m_checkpoints_updating.test_and_set()) return true;

    bool res = true;
    if (!skip_dns && time(NULL) - m_last_dns_checkpoints_update >= 3600)
    {
      res = m_blockchain_storage.update_checkpoints(m_checkpoints_path, true);
      m_last_dns_checkpoints_update = time(NULL);
      m_last_json_checkpoints_update = time(NULL);
    }
    else if (time(NULL) - m_last_json_checkpoints_update >= 600)
    {
      res = m_blockchain_storage.update_checkpoints(m_checkpoints_path, false);
      m_last_json_checkpoints_update = time(NULL);
    }

    m_checkpoints_updating.clear();

    // if anything fishy happened getting new checkpoints, bring down the house
    if (!res)
    {
      graceful_exit();
    }
    return res;
  }
  //-----------------------------------------------------------------------------------
  void core::stop()
  {
    m_epose_v2_producer_cancel.store(true, std::memory_order_relaxed);
    m_miner.stop();
    m_blockchain_storage.cancel();

    tools::download_async_handle handle;
    {
      boost::lock_guard<boost::mutex> lock(m_update_mutex);
      handle = m_update_download;
      m_update_download = 0;
    }
    if (handle)
      tools::download_cancel(handle);
  }
  //-----------------------------------------------------------------------------------
  void core::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_data_dir);

    command_line::add_arg(desc, arg_test_drop_download);
    command_line::add_arg(desc, arg_test_drop_download_height);

    command_line::add_arg(desc, arg_testnet_on);
    command_line::add_arg(desc, arg_stagenet_on);
    command_line::add_arg(desc, arg_regtest_on);
    command_line::add_arg(desc, arg_keep_fakechain);
    command_line::add_arg(desc, arg_fixed_difficulty);
    command_line::add_arg(desc, arg_dns_checkpoints);
    command_line::add_arg(desc, arg_prep_blocks_threads);
    command_line::add_arg(desc, arg_fast_block_sync);
    command_line::add_arg(desc, arg_show_time_stats);
    command_line::add_arg(desc, arg_block_sync_size);
    command_line::add_arg(desc, arg_check_updates);
    command_line::add_arg(desc, arg_fluffy_blocks);
    command_line::add_arg(desc, arg_test_dbg_lock_sleep);
    command_line::add_arg(desc, arg_offline);
    command_line::add_arg(desc, arg_disable_dns_checkpoints);
    command_line::add_arg(desc, arg_block_download_max_size);
    command_line::add_arg(desc, arg_sync_pruned_blocks);
    command_line::add_arg(desc, arg_service_node);
    command_line::add_arg(desc, arg_service_node_key);
    command_line::add_arg(desc, arg_service_reward_address);
    command_line::add_arg(desc, arg_service_reward_view_key);
    command_line::add_arg(desc, arg_service_node_advertise_address);
    command_line::add_arg(desc, arg_epose_v2_service);
    command_line::add_arg(desc, arg_epose_v2_keystore);
    command_line::add_arg(desc, arg_epose_v2_reward_address);
    command_line::add_arg(desc, arg_epose_v2_endpoint_host);
    command_line::add_arg(desc, arg_epose_v2_endpoint_port);
    command_line::add_arg(desc, arg_epose_v2_discovery_endpoint);
    command_line::add_arg(desc, arg_max_txpool_weight);
    command_line::add_arg(desc, arg_block_notify);
    command_line::add_arg(desc, arg_prune_blockchain);
    command_line::add_arg(desc, arg_reorg_notify);
    command_line::add_arg(desc, arg_block_rate_notify);
    command_line::add_arg(desc, arg_keep_alt_blocks);

    miner::init_options(desc);
    BlockchainDB::init_options(desc);
  }
  //-----------------------------------------------------------------------------------------------
  network_type core::get_network_type_from_args(const boost::program_options::variables_map& vm)
  {
    const bool testnet = command_line::get_arg(vm, arg_testnet_on);
    const bool stagenet = command_line::get_arg(vm, arg_stagenet_on);
    const bool regtest = command_line::get_arg(vm, arg_regtest_on);
    if (testnet + stagenet + regtest > 1)
      throw std::runtime_error("More than one network type argument was specified");
    return testnet ? TESTNET : stagenet ? STAGENET : regtest ? FAKECHAIN : MAINNET;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_command_line(const boost::program_options::variables_map& vm)
  {
    if (m_nettype != FAKECHAIN)
    {
      m_nettype = get_network_type_from_args(vm);
    }

    m_config_folder = command_line::get_arg(vm, arg_data_dir);

    auto data_dir = boost::filesystem::path(m_config_folder);

    if (m_nettype == MAINNET)
    {
      cryptonote::checkpoints checkpoints;
      if (!checkpoints.init_default_checkpoints(m_nettype))
      {
        throw std::runtime_error("Failed to initialize checkpoints");
      }
      set_checkpoints(std::move(checkpoints));

      boost::filesystem::path json(JSON_HASH_FILE_NAME);
      boost::filesystem::path checkpoint_json_hashfile_fullpath = data_dir / json;

      set_checkpoints_file_path(checkpoint_json_hashfile_fullpath.string());
    }


    set_enforce_dns_checkpoints(command_line::get_arg(vm, arg_dns_checkpoints));
    test_drop_download_height(command_line::get_arg(vm, arg_test_drop_download_height));
    m_offline = get_arg(vm, arg_offline);
    m_disable_dns_checkpoints = get_arg(vm, arg_disable_dns_checkpoints);

    if (!init_epose_service_node_config(vm))
      return false;

    if (!command_line::is_arg_defaulted(vm, arg_fluffy_blocks))
      MWARNING(arg_fluffy_blocks.name << " is obsolete, it is now default");

    if (command_line::get_arg(vm, arg_test_drop_download) == true)
      test_drop_download();

    epee::debug::g_test_dbg_lock_sleep() = command_line::get_arg(vm, arg_test_dbg_lock_sleep);

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::init_epose_service_node_config(const boost::program_options::variables_map& vm)
  {
    qwertycoin::epose::local_service_node_config config{};
    config.enabled = command_line::get_arg(vm, arg_service_node);
    m_epose_local_service_node_config = config;
    m_epose_v2_service_enabled = command_line::get_arg(vm, arg_epose_v2_service);
    m_epose_v2_keystore_path = command_line::get_arg(vm, arg_epose_v2_keystore);
    m_epose_v2_reward_address_string = command_line::get_arg(vm, arg_epose_v2_reward_address);
    m_epose_v2_endpoint_host = command_line::get_arg(vm, arg_epose_v2_endpoint_host);
    m_epose_v2_endpoint_port = command_line::get_arg(vm, arg_epose_v2_endpoint_port);
    m_epose_v2_discovery_endpoints = command_line::get_arg(vm, arg_epose_v2_discovery_endpoint);
    if (!config.enabled)
      return true;

    // The inherited HF17-v1 options disclose a reward view secret and create
    // legacy registration objects. They are intentionally not an input to the
    // genesis-native v2 protocol. Keep the switches recognizable so operators
    // receive a deterministic error, but never load keys or construct v1 state.
    MERROR("The legacy --service-node interface is retired for QWC-HF17/EPoSE-v2; use the future v2 lifecycle/admission producer once its launch gate is complete");
    return false;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::init_epose_v2_service_runtime()
  {
    m_epose_v2_service_ready = false;
    m_epose_v2_pending_epoch = std::numeric_limits<uint64_t>::max();
    m_epose_v2_pending_envelopes.clear();
    if (!m_epose_v2_service_enabled)
      return true;
    if (m_offline || m_epose_v2_keystore_path.empty()
        || m_epose_v2_reward_address_string.empty()
        || m_epose_v2_endpoint_host.empty()
        || m_epose_v2_endpoint_port == 0)
    {
      MERROR("EPoSE-v2 service mode requires online operation, a keystore, a public reward address and a public endpoint host/port");
      return false;
    }

    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
    {
      MERROR("EPoSE-v2 service mode requires an active compiled consensus profile");
      return false;
    }
    std::string error;
    if (!qwertycoin::epose::parse_reward_address(
            m_epose_v2_reward_address_string, m_nettype,
            m_epose_v2_reward_address, error))
    {
      MERROR("Invalid EPoSE-v2 reward address: " << error);
      return false;
    }
    const qwertycoin::epose::service_keystore_context_v2 context{
        m_nettype, parameters.genesis_hash, parameters.parameter_set_hash};
    const qwertycoin::epose::service_keystore_status_v2 status =
        qwertycoin::epose::load_or_create_service_keystore_v2(
            m_epose_v2_keystore_path, context, m_epose_v2_keystore, error);
    if (status != qwertycoin::epose::service_keystore_status_v2::loaded
        && status != qwertycoin::epose::service_keystore_status_v2::created)
    {
      MERROR("Failed to load EPoSE-v2 keystore: " << error);
      return false;
    }
    m_epose_v2_identity_id = qwertycoin::epose::derive_identity_id_v2(
        m_nettype, parameters.genesis_hash, parameters.parameter_set_hash,
        m_epose_v2_keystore.operator_public_key);
    uint64_t first_service_epoch = 0;
    if (!parameters.timing.first_service_epoch(first_service_epoch)
        || first_service_epoch > std::numeric_limits<uint64_t>::max() - 2)
      return false;
    qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
    if (!build_epose_v2_configured_endpoint(
            parameters, 0, first_service_epoch + 2, endpoint))
    {
      MERROR("Invalid EPoSE-v2 public service endpoint");
      return false;
    }
    remember_epose_v2_endpoint(parameters, endpoint);
    m_epose_v2_service_ready = true;
    MGINFO("EPoSE-v2 service authority ready: identity "
        << epee::string_tools::pod_to_hex(m_epose_v2_identity_id)
        << ", service key "
        << epee::string_tools::pod_to_hex(m_epose_v2_keystore.service_public_key));
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::build_epose_v2_configured_endpoint(
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const uint64_t sequence,
      const uint64_t expiry_epoch,
      qwertycoin::epose::endpoint_descriptor_v2 &endpoint) const
  {
    endpoint = {};
    boost::system::error_code address_error;
    const auto address = boost::asio::ip::make_address(
        m_epose_v2_endpoint_host, address_error);
    endpoint.service_public_key = m_epose_v2_keystore.service_public_key;
    endpoint.transport = address_error
        ? qwertycoin::epose::endpoint_transport_v2::dns
        : (address.is_v4()
            ? qwertycoin::epose::endpoint_transport_v2::tcp_ipv4
            : qwertycoin::epose::endpoint_transport_v2::tcp_ipv6);
    endpoint.host = m_epose_v2_endpoint_host;
    endpoint.port = m_epose_v2_endpoint_port;
    endpoint.service_kind = parameters.committee.service_kind;
    endpoint.service_version = qwertycoin::epose::EPOSE_PROTOCOL_VERSION_V2;
    endpoint.sequence = sequence;
    endpoint.expiry_epoch = expiry_epoch;
    if (!qwertycoin::epose::sign_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, endpoint,
            m_epose_v2_keystore.service_secret_key)
        || qwertycoin::epose::validate_endpoint_descriptor_v2(
               parameters.nettype, parameters.genesis_hash,
               parameters.parameter_set_hash, endpoint)
            != qwertycoin::epose::resource_status_v2::accepted)
    {
      endpoint = {};
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::remember_epose_v2_endpoint(
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint)
  {
    const uint64_t current_epoch =
        m_blockchain_storage.get_epose_current_epoch();
    const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
    const auto status = m_epose_v2_endpoint_cache.admit(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, current_epoch, endpoint);
    if (status != qwertycoin::epose::resource_status_v2::accepted
        && status != qwertycoin::epose::resource_status_v2::idempotent_duplicate)
      MWARNING("EPoSE-v2 local endpoint was not admitted to the bounded discovery cache");
    m_epose_v2_endpoint = endpoint;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::update_epose_v2_service_producer()
  {
    if (!m_epose_v2_service_ready || m_offline || !get_protocol()
        || !get_protocol()->is_synchronized())
      return true;

    const uint64_t chain_height = m_blockchain_storage.get_current_blockchain_height();
    if (chain_height == 0)
      return true;
    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
      return false;

    uint64_t target_epoch = m_blockchain_storage.get_epose_current_epoch() + 1;
    uint64_t cutoff = 0;
    if (!parameters.timing.enrollment_cutoff(target_epoch, cutoff))
      return false;
    if (chain_height > cutoff)
    {
      if (target_epoch == std::numeric_limits<uint64_t>::max())
        return false;
      ++target_epoch;
    }

    const auto descriptors =
        m_blockchain_storage.get_epose_identity_descriptors_v2(target_epoch);
    const auto existing = std::find_if(
        descriptors.begin(), descriptors.end(), [this](const auto &descriptor) {
          return descriptor.identity_id == m_epose_v2_identity_id;
        });
    if (existing != descriptors.end())
    {
      if (existing->service_public_key
          != m_epose_v2_keystore.service_public_key)
      {
        MERROR("EPoSE-v2 local identity is bound to an unexpected service key");
        return false;
      }
      if (existing->reward_address.m_view_public_key
              != m_epose_v2_reward_address.m_view_public_key
          || existing->reward_address.m_spend_public_key
              != m_epose_v2_reward_address.m_spend_public_key)
      {
        MERROR("EPoSE-v2 configured reward address does not match the canonical descriptor; a frozen reward is never redirected silently");
        return false;
      }
      qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
      if (!build_epose_v2_configured_endpoint(
              parameters, existing->sequence, existing->expiry_epoch,
              endpoint))
        return false;
      if (qwertycoin::epose::hash_endpoint_descriptor_v2(
              parameters.nettype, parameters.genesis_hash,
              parameters.parameter_set_hash, endpoint)
          != existing->endpoint_descriptor_hash)
      {
        MERROR("EPoSE-v2 configured endpoint does not match the active descriptor");
        return false;
      }
      remember_epose_v2_endpoint(parameters, endpoint);
      if (!relay_local_epose_v2_endpoint(parameters, endpoint))
        return false;
      if (m_blockchain_storage.has_epose_admission_v2(
              m_epose_v2_identity_id, target_epoch))
      {
        m_epose_v2_producer_cancel.store(true, std::memory_order_relaxed);
        m_epose_v2_pending_envelopes.clear();
        m_epose_v2_pending_epoch = std::numeric_limits<uint64_t>::max();
        return true;
      }
    }

    if (m_epose_v2_pending_epoch != target_epoch)
    {
      m_epose_v2_producer_cancel.store(true, std::memory_order_relaxed);
      if (m_epose_v2_producer_future.valid())
      {
        if (m_epose_v2_producer_future.wait_for(std::chrono::seconds(0))
            != std::future_status::ready)
          return true;
        m_epose_v2_producer_future.get();
      }
      m_epose_v2_pending_envelopes.clear();
      m_epose_v2_pending_epoch = target_epoch;
      m_epose_v2_producer_cancel.store(false, std::memory_order_relaxed);
    }

    if (m_epose_v2_pending_envelopes.empty())
    {
      if (m_epose_v2_producer_future.valid())
      {
        if (m_epose_v2_producer_future.wait_for(std::chrono::seconds(0))
            != std::future_status::ready)
          return true;
        auto result = m_epose_v2_producer_future.get();
        if (result.first == qwertycoin::epose::service_producer_status_v2::cancelled)
          return true;
        if (result.first != qwertycoin::epose::service_producer_status_v2::accepted)
        {
          MERROR("Failed to build bounded EPoSE-v2 enrollment: status "
              << static_cast<unsigned>(result.first));
          m_epose_v2_pending_epoch = std::numeric_limits<uint64_t>::max();
          return false;
        }
        std::vector<blobdata> encoded;
        encoded.reserve(result.second.records.size());
        for (const auto &record : result.second.records)
        {
          qwertycoin::epose::envelope_budget_v2 budget{};
          blobdata envelope;
          if (qwertycoin::epose::encode_envelope_v2(
                  {record}, parameters.limits.envelope, envelope, budget)
              != qwertycoin::epose::envelope_status_v2::accepted)
            return false;
          encoded.push_back(std::move(envelope));
        }
        m_epose_v2_pending_envelopes = std::move(encoded);
        m_epose_v2_last_submission = {};
      }
      else
      {
        uint64_t context_height = 0;
        if (target_epoch < parameters.admission.context_epoch_offset
            || !parameters.timing.epoch_start(
                target_epoch - parameters.admission.context_epoch_offset,
                context_height))
          return false;
        if (chain_height <= context_height)
          return true;
        const crypto::hash context_hash =
            m_blockchain_storage.get_block_id_by_height(context_height);
        if (context_hash == crypto::null_hash)
          return true;

        qwertycoin::epose::service_enrollment_config_v2 configuration{};
        configuration.keystore = m_epose_v2_keystore;
        configuration.reward_address = m_epose_v2_reward_address;
        {
          const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
          configuration.endpoint_transport = m_epose_v2_endpoint.transport;
          configuration.endpoint_host = m_epose_v2_endpoint.host;
          configuration.endpoint_port = m_epose_v2_endpoint.port;
        }
        configuration.target_epoch = target_epoch;
        if (target_epoch > std::numeric_limits<uint64_t>::max() - 2)
          return false;
        configuration.expiry_epoch = target_epoch + 2;
        m_epose_v2_producer_cancel.store(false, std::memory_order_relaxed);
        const bool renewal = existing != descriptors.end();
        const qwertycoin::epose::identity_descriptor_v2 current_descriptor =
            renewal ? *existing : qwertycoin::epose::identity_descriptor_v2{};
        m_epose_v2_producer_future = std::async(
            std::launch::async,
            [parameters, configuration, context_hash, renewal,
             current_descriptor, this]() mutable {
              qwertycoin::epose::service_enrollment_v2 enrollment{};
              const auto status = renewal
                  ? qwertycoin::epose::build_service_renewal_enrollment_v2(
                      parameters, configuration, current_descriptor,
                      context_hash, 1000000,
                      enrollment, &m_epose_v2_producer_cancel)
                  : qwertycoin::epose::build_initial_service_enrollment_v2(
                      parameters, configuration, context_hash, 1000000,
                      enrollment, &m_epose_v2_producer_cancel);
              return std::make_pair(status, std::move(enrollment));
            });
        MGINFO("Started bounded EPoSE-v2 "
            << (renewal ? "renewal and " : "")
            << "RandomX admission search for epoch " << target_epoch);
        return true;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_epose_v2_last_submission.time_since_epoch().count() != 0
        && now - m_epose_v2_last_submission < std::chrono::seconds(30))
      return true;
    bool newly_accepted = false;
    bool relayed = false;
    if (!submit_local_epose_envelopes_v2(
            m_epose_v2_pending_envelopes, newly_accepted, relayed))
      return false;
    m_epose_v2_last_submission = now;
    if (newly_accepted)
      MGINFO("Submitted EPoSE-v2 lifecycle and admission for epoch "
          << target_epoch << (relayed ? " and relayed it" : " locally"));
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::update_epose_v2_receipt_producer()
  {
    if (!m_epose_v2_service_ready || m_offline || !get_protocol()
        || !get_protocol()->is_synchronized()
        || m_epose_v2_discovery_endpoints.empty())
      return true;

    if (m_epose_v2_receipt_future.valid())
    {
      if (m_epose_v2_receipt_future.wait_for(std::chrono::seconds(0))
          != std::future_status::ready)
        return true;
      epose_v2_receipt_job_result result = m_epose_v2_receipt_future.get();
      m_epose_v2_last_receipt_attempt = std::chrono::steady_clock::now();
      if (!result.accepted)
      {
        m_epose_v2_receipt_retries.failed(
            result.slot, epose_v2_steady_milliseconds());
        return true;
      }
      bool newly_accepted = false;
      bool relayed = false;
      if (!submit_local_epose_envelope_v2(
              result.envelope, newly_accepted, relayed))
      {
        m_epose_v2_receipt_retries.failed(
            result.slot, epose_v2_steady_milliseconds());
        return false;
      }
      m_epose_v2_receipt_retries.submitted(
          result.slot, epose_v2_steady_milliseconds());
      if (newly_accepted)
        MGINFO("Submitted authenticated EPoSE-v2 receipt"
            << (relayed ? " and relayed it" : " locally"));
      return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_epose_v2_last_receipt_attempt.time_since_epoch().count() != 0
        && now - m_epose_v2_last_receipt_attempt < std::chrono::seconds(30))
      return true;

    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
      return false;
    const uint64_t inclusion_height =
        m_blockchain_storage.get_current_blockchain_height();
    if (inclusion_height < parameters.timing.activation_height)
      return true;
    const uint64_t epoch =
        (inclusion_height - parameters.timing.activation_height)
        / parameters.timing.epoch_length;
    if (m_epose_v2_receipt_epoch != epoch)
    {
      m_epose_v2_receipt_epoch = epoch;
    }

    qwertycoin::epose::membership_snapshot_v2 snapshot{};
    if (!m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot))
      return true;
    const auto local_member = std::find_if(
        snapshot.members.begin(), snapshot.members.end(), [this](const auto &member) {
          return member.service_public_key
              == m_epose_v2_keystore.service_public_key;
        });
    if (local_member == snapshot.members.end())
      return true;

    uint64_t epoch_start = 0;
    uint64_t evidence_deadline = 0;
    if (!parameters.timing.epoch_start(epoch, epoch_start)
        || !parameters.timing.evidence_deadline(epoch, evidence_deadline)
        || inclusion_height > evidence_deadline)
      return true;

    uint64_t active_round = std::numeric_limits<uint64_t>::max();
    for (uint64_t round = 0; round < parameters.committee.round_offsets.size(); ++round)
    {
      if (epoch_start > std::numeric_limits<uint64_t>::max()
              - parameters.committee.round_offsets[round])
        return false;
      uint64_t first = epoch_start + parameters.committee.round_offsets[round];
      if (round != 0)
      {
        if (first == std::numeric_limits<uint64_t>::max())
          return false;
        ++first;
      }
      uint64_t last = evidence_deadline;
      if (round + 1 < parameters.committee.round_offsets.size())
      {
        if (epoch_start > std::numeric_limits<uint64_t>::max()
                - parameters.committee.round_offsets[round + 1]
            || epoch_start + parameters.committee.round_offsets[round + 1] == 0)
          return false;
        last = epoch_start + parameters.committee.round_offsets[round + 1] - 1;
      }
      if (inclusion_height >= first && inclusion_height <= last)
      {
        active_round = round;
        break;
      }
    }
    if (active_round == std::numeric_limits<uint64_t>::max())
      return true;

    uint64_t anchor_height = snapshot.anchor_height;
    if (active_round != 0)
      anchor_height = epoch_start + parameters.committee.round_offsets[active_round];
    const crypto::hash anchor_hash =
        m_blockchain_storage.get_block_id_by_height(anchor_height);
    if (anchor_hash == crypto::null_hash)
      return true;
    if (!m_epose_v2_receipt_retries.begin_context(
            epoch, active_round, anchor_hash))
      return false;
    cryptonote::block anchor_block{};
    bool orphan = false;
    if (!m_blockchain_storage.get_block_by_hash(anchor_hash, anchor_block, &orphan)
        || orphan || cryptonote::get_block_hash(anchor_block) != anchor_hash)
      return false;
    const cryptonote::blobdata anchor_blob =
        cryptonote::block_to_blob(anchor_block);

    for (const auto &subject : snapshot.members)
    {
      if (subject.service_public_key == m_epose_v2_keystore.service_public_key)
        continue;
      const auto committee = m_blockchain_storage.get_epose_committee_v2(
          epoch, active_round, subject.service_public_key, anchor_hash);
      if (std::none_of(committee.begin(), committee.end(), [this](const auto &entry) {
            return entry.verifier_public_key
                == m_epose_v2_keystore.service_public_key;
          }))
        continue;
      if (m_blockchain_storage.has_epose_receipt_slot_v2(
              epoch, active_round, subject.service_public_key,
              m_epose_v2_keystore.service_public_key))
      {
        const crypto::hash canonical_slot = epose_v2_receipt_slot_hash(
            epoch, active_round, subject.service_public_key,
            m_epose_v2_keystore.service_public_key, anchor_hash);
        m_epose_v2_receipt_retries.canonical(canonical_slot);
        continue;
      }
      const crypto::hash slot = epose_v2_receipt_slot_hash(
          epoch, active_round, subject.service_public_key,
          m_epose_v2_keystore.service_public_key, anchor_hash);
      const uint64_t now_ms = epose_v2_steady_milliseconds();
      if (!m_epose_v2_receipt_retries.can_attempt(slot, now_ms))
        continue;

      qwertycoin::epose::service_challenge_v2 challenge{};
      challenge.service_kind = parameters.committee.service_kind;
      challenge.epoch = epoch;
      challenge.round = active_round;
      challenge.snapshot_hash = snapshot.snapshot_hash;
      challenge.anchor_hash = anchor_hash;
      challenge.subject_public_key = subject.service_public_key;
      challenge.verifier_public_key = m_epose_v2_keystore.service_public_key;
      challenge.endpoint_descriptor_hash = subject.endpoint_descriptor_hash;
      challenge.nonce = epose_v2_challenge_nonce(
          slot, snapshot.snapshot_hash, subject.endpoint_descriptor_hash);
      challenge.requested_object_hash = anchor_hash;
      const qwertycoin::epose::receipt_context_v2 context{
          parameters.nettype, parameters.genesis_hash,
          parameters.parameter_set_hash};
      if (!qwertycoin::epose::validate_service_challenge_v2(challenge, context))
        return false;
      if (!m_epose_v2_receipt_retries.start(slot, now_ms))
        return false;

      const auto discovery = m_epose_v2_discovery_endpoints;
      const auto service_secret = m_epose_v2_keystore.service_secret_key;
      qwertycoin::epose::endpoint_descriptor_v2 cached_endpoint{};
      const bool cached_endpoint_found = get_epose_v2_endpoint_descriptor(
          cached_endpoint, &challenge.endpoint_descriptor_hash);
      m_epose_v2_last_receipt_attempt = now;
      m_epose_v2_receipt_future = std::async(
          std::launch::async,
          [parameters, challenge, context, discovery, service_secret,
           cached_endpoint, cached_endpoint_found, anchor_blob, slot]() mutable {
            epose_v2_receipt_job_result result{};
            result.slot = slot;
            qwertycoin::epose::endpoint_descriptor_v2 endpoint = cached_endpoint;
            bool found = cached_endpoint_found;
            for (const std::string &url : discovery)
            {
              if (found)
                break;
              qwertycoin::epose::endpoint_descriptor_v2 candidate{};
              if (fetch_epose_endpoint_descriptor(
                      url, parameters,
                      challenge.endpoint_descriptor_hash, candidate)
                  && candidate.service_public_key
                      == challenge.subject_public_key
                  && qwertycoin::epose::hash_endpoint_descriptor_v2(
                         parameters.nettype, parameters.genesis_hash,
                         parameters.parameter_set_hash, candidate)
                      == challenge.endpoint_descriptor_hash)
              {
                endpoint = std::move(candidate);
                found = true;
                break;
              }
            }
            if (!found)
              return result;
            qwertycoin::epose::canonical_service_response_v2 response{};
            if (!request_epose_service_response(
                    challenge, endpoint, parameters, response))
              return result;
            const auto authorize = [&challenge, &context](
                const qwertycoin::epose::service_challenge_v2 &candidate,
                const qwertycoin::epose::receipt_context_v2 &candidate_context) {
              return qwertycoin::epose::hash_service_challenge_v2(
                         candidate, candidate_context)
                      == qwertycoin::epose::hash_service_challenge_v2(
                         challenge, context)
                  && candidate_context.nettype == context.nettype
                  && candidate_context.genesis_hash == context.genesis_hash
                  && candidate_context.parameter_set_hash
                      == context.parameter_set_hash;
            };
            const auto source = [&challenge, &anchor_blob](
                const crypto::hash &hash, cryptonote::blobdata &blob) {
              if (hash != challenge.requested_object_hash)
                return false;
              blob = anchor_blob;
              return true;
            };
            qwertycoin::epose::authenticated_service_receipt_v2 receipt{};
            if (qwertycoin::epose::verify_canonical_block_response_v2(
                    challenge, response, context,
                    {EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES},
                    service_secret, authorize, source, receipt)
                != qwertycoin::epose::canonical_service_status_v2::accepted)
              return result;
            qwertycoin::epose::envelope_record_v2 record{};
            if (qwertycoin::epose::encode_service_receipt_record_v2(
                    receipt, context, record)
                != qwertycoin::epose::record_codec_status_v2::accepted)
              return result;
            qwertycoin::epose::envelope_budget_v2 budget{};
            if (qwertycoin::epose::encode_envelope_v2(
                    {record}, parameters.limits.envelope,
                    result.envelope, budget)
                != qwertycoin::epose::envelope_status_v2::accepted)
            {
              result.envelope.clear();
              return result;
            }
            result.accepted = true;
            return result;
          });
      MGINFO("Started bounded EPoSE-v2 service challenge for epoch "
          << epoch << ", round " << active_round);
      return true;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  namespace
  {
    bool append_extra_nonce_to_miner_tx(transaction &miner_tx, const blobdata &extra_nonce)
    {
      if (extra_nonce.empty())
        return true;
      if (!add_extra_nonce_to_tx_extra(miner_tx.extra, extra_nonce))
        return false;
      return sort_tx_extra(miner_tx.extra, miner_tx.extra);
    }
  }
  //-----------------------------------------------------------------------------------------------
  bool core::build_epose_miner_extra_nonce(blobdata& epose_extra_nonce) const
  {
    epose_extra_nonce.clear();
    // HF17 is exclusively EPoSE-v2. The old extra-nonce carrier is never
    // appended to a miner template; v2 records use the typed 0x05 envelope.
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_epose_payloads(
      const std::vector<blobdata>& registration_blobs,
      const std::vector<blobdata>& attestation_blobs,
      std::vector<blobdata>& accepted_registration_blobs,
      std::vector<blobdata>& accepted_attestation_blobs)
  {
    accepted_registration_blobs.clear();
    accepted_attestation_blobs.clear();
    if (registration_blobs.size() + attestation_blobs.size() > qwertycoin::epose::EPOSE_ATTESTATION_RELAY_MAX_BATCH)
      return false;

    // This P2P command carries only legacy fixed-size registrations and
    // attestations. HF17/v2 peers must not treat either collection as a v2
    // lifecycle, admission, or receipt record.
    return registration_blobs.empty() && attestation_blobs.empty();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_epose_envelopes_v2(
      const std::vector<blobdata>& envelopes,
      std::vector<blobdata>& accepted_envelopes)
  {
    return m_blockchain_storage.submit_epose_relay_envelopes_v2(
        envelopes, accepted_envelopes);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::epose_v2_endpoint_hash_is_canonical(
      const crypto::hash &descriptor_hash) const
  {
    if (descriptor_hash == crypto::null_hash)
      return false;
    const uint64_t current_epoch =
        m_blockchain_storage.get_epose_current_epoch();
    for (uint64_t offset = 0;
         offset <= qwertycoin::epose::EPOSE_ENDPOINT_CACHE_MAX_FUTURE_EPOCHS_V2;
         ++offset)
    {
      if (current_epoch > std::numeric_limits<uint64_t>::max() - offset)
        break;
      const uint64_t epoch = current_epoch + offset;
      const auto descriptors =
          m_blockchain_storage.get_epose_identity_descriptors_v2(epoch);
      if (std::any_of(descriptors.begin(), descriptors.end(),
              [&descriptor_hash](const auto &descriptor) {
                return descriptor.endpoint_descriptor_hash == descriptor_hash;
              }))
        return true;

      qwertycoin::epose::membership_snapshot_v2 snapshot{};
      if (m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot)
          && std::any_of(snapshot.members.begin(), snapshot.members.end(),
              [&descriptor_hash](const auto &member) {
                return member.endpoint_descriptor_hash == descriptor_hash;
              }))
        return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_epose_endpoints_v2(
      const std::vector<blobdata>& descriptors,
      std::vector<blobdata>& accepted_descriptors)
  {
    accepted_descriptors.clear();
    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
      return false;
    const uint64_t current_epoch =
        m_blockchain_storage.get_epose_current_epoch();
    std::vector<crypto::hash> canonical_hashes;
    for (uint64_t offset = 0;
         offset <= qwertycoin::epose::EPOSE_ENDPOINT_CACHE_MAX_FUTURE_EPOCHS_V2;
         ++offset)
    {
      if (current_epoch > std::numeric_limits<uint64_t>::max() - offset)
        break;
      const uint64_t epoch = current_epoch + offset;
      const auto lifecycle =
          m_blockchain_storage.get_epose_identity_descriptors_v2(epoch);
      for (const auto &descriptor : lifecycle)
        canonical_hashes.push_back(descriptor.endpoint_descriptor_hash);
      qwertycoin::epose::membership_snapshot_v2 snapshot{};
      if (m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot))
        for (const auto &member : snapshot.members)
          canonical_hashes.push_back(member.endpoint_descriptor_hash);
    }
    std::sort(canonical_hashes.begin(), canonical_hashes.end());
    canonical_hashes.erase(
        std::unique(canonical_hashes.begin(), canonical_hashes.end()),
        canonical_hashes.end());

    const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
    return qwertycoin::epose::admit_endpoint_relay_batch_v2(
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash, current_epoch, descriptors,
        canonical_hashes, m_epose_v2_endpoint_cache, accepted_descriptors)
        == qwertycoin::epose::resource_status_v2::accepted;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::submit_local_epose_envelope_v2(
      const blobdata& envelope,
      bool& newly_accepted,
      bool& relayed)
  {
    return submit_local_epose_envelopes_v2(
        std::vector<blobdata>{envelope}, newly_accepted, relayed);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::submit_local_epose_envelopes_v2(
      const std::vector<blobdata>& envelopes,
      bool& newly_accepted,
      bool& relayed)
  {
    newly_accepted = false;
    relayed = false;
    if (envelopes.empty()
        || std::any_of(envelopes.begin(), envelopes.end(),
            [](const blobdata &envelope) { return envelope.empty(); })
        || m_offline || !get_protocol()->is_synchronized())
      return false;

    std::vector<blobdata> accepted;
    if (!handle_incoming_epose_envelopes_v2(envelopes, accepted))
      return false;
    if (accepted.empty())
      return true;

    NOTIFY_NEW_EPOSE_ENVELOPES_V2::request request{};
    request.envelopes = accepted;
    newly_accepted = true;
    relayed = get_protocol()->relay_epose_envelopes_v2(
        request, boost::uuids::nil_uuid(), epee::net_utils::zone::public_);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::relay_local_epose_v2_endpoint(
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint)
  {
    if (m_offline || !get_protocol() || !get_protocol()->is_synchronized())
      return true;
    const crypto::hash descriptor_hash =
        qwertycoin::epose::hash_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, endpoint);
    if (!epose_v2_endpoint_hash_is_canonical(descriptor_hash))
      return false;
    const auto now = std::chrono::steady_clock::now();
    {
      const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
      if (m_epose_v2_last_relayed_endpoint_hash == descriptor_hash
          && m_epose_v2_last_endpoint_relay != std::chrono::steady_clock::time_point{}
          && now - m_epose_v2_last_endpoint_relay < std::chrono::seconds(60))
        return true;
    }

    blobdata blob;
    if (qwertycoin::epose::encode_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, endpoint, blob)
        != qwertycoin::epose::resource_status_v2::accepted
        || blob.size()
            > qwertycoin::epose::EPOSE_ENDPOINT_DESCRIPTOR_MAX_BLOB_SIZE_V2)
      return false;
    NOTIFY_NEW_EPOSE_ENDPOINTS_V2::request request{};
    request.descriptors.push_back(std::move(blob));
    if (!get_protocol()->relay_epose_endpoints_v2(
            request, boost::uuids::nil_uuid(),
            epee::net_utils::zone::public_))
      return false;
    {
      const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
      m_epose_v2_last_relayed_endpoint_hash = descriptor_hash;
      m_epose_v2_last_endpoint_relay = now;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_epose_v2_endpoint_descriptor(
      qwertycoin::epose::endpoint_descriptor_v2 &descriptor,
      const crypto::hash *required_hash) const
  {
    descriptor = {};
    if (required_hash == nullptr)
    {
      if (!m_epose_v2_service_ready)
        return false;
      const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
      descriptor = m_epose_v2_endpoint;
    }
    else
    {
      if (!epose_v2_endpoint_hash_is_canonical(*required_hash))
        return false;
      const uint64_t current_epoch =
          m_blockchain_storage.get_epose_current_epoch();
      const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
      if (!m_epose_v2_endpoint_cache.find(
              *required_hash, current_epoch, descriptor))
        return false;
    }
    if (descriptor.service_public_key == crypto::null_pkey)
      return false;
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::answer_epose_v2_service_challenge(
      const qwertycoin::epose::service_challenge_v2 &challenge,
      qwertycoin::epose::canonical_service_response_v2 &response) const
  {
    response = {};
    if (!m_epose_v2_service_ready
        || challenge.subject_public_key
            != m_epose_v2_keystore.service_public_key)
      return false;
    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
      return false;
    qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
    if (!get_epose_v2_endpoint_descriptor(
            endpoint, &challenge.endpoint_descriptor_hash))
      return false;
    if (endpoint.service_public_key == crypto::null_pkey)
      return false;
    const qwertycoin::epose::receipt_context_v2 context{
        parameters.nettype, parameters.genesis_hash,
        parameters.parameter_set_hash};
    const auto authorize = [this, &parameters, &endpoint](
        const qwertycoin::epose::service_challenge_v2 &candidate,
        const qwertycoin::epose::receipt_context_v2 &candidate_context) {
      if (candidate_context.nettype != parameters.nettype
          || candidate_context.genesis_hash != parameters.genesis_hash
          || candidate_context.parameter_set_hash
              != parameters.parameter_set_hash
          || candidate.subject_public_key
              != m_epose_v2_keystore.service_public_key
          || candidate.endpoint_descriptor_hash
              != qwertycoin::epose::hash_endpoint_descriptor_v2(
                  parameters.nettype, parameters.genesis_hash,
                  parameters.parameter_set_hash, endpoint)
          || candidate.round >= parameters.committee.round_offsets.size())
        return false;

      qwertycoin::epose::membership_snapshot_v2 snapshot{};
      if (!m_blockchain_storage.get_epose_membership_snapshot_v2(
              candidate.epoch, snapshot)
          || candidate.snapshot_hash != snapshot.snapshot_hash)
        return false;
      const auto subject = std::find_if(
          snapshot.members.begin(), snapshot.members.end(),
          [&candidate](const auto &member) {
            return member.service_public_key == candidate.subject_public_key
                && member.endpoint_descriptor_hash
                    == candidate.endpoint_descriptor_hash;
          });
      if (subject == snapshot.members.end())
        return false;

      uint64_t anchor_height = 0;
      if (candidate.round == 0)
      {
        if (!parameters.timing.committee_anchor(
                candidate.epoch, anchor_height))
          return false;
      }
      else
      {
        uint64_t start = 0;
        if (!parameters.timing.epoch_start(candidate.epoch, start)
            || start > std::numeric_limits<uint64_t>::max()
                - parameters.committee.round_offsets[candidate.round])
          return false;
        anchor_height =
            start + parameters.committee.round_offsets[candidate.round];
      }
      const crypto::hash anchor_hash =
          m_blockchain_storage.get_block_id_by_height(anchor_height);
      if (anchor_hash == crypto::null_hash
          || candidate.anchor_hash != anchor_hash
          || candidate.requested_object_hash != anchor_hash)
        return false;
      uint64_t deadline = 0;
      const uint64_t inclusion_height =
          m_blockchain_storage.get_current_blockchain_height();
      if (!parameters.timing.evidence_deadline(candidate.epoch, deadline)
          || inclusion_height > deadline)
        return false;
      const auto committee = m_blockchain_storage.get_epose_committee_v2(
          candidate.epoch, candidate.round,
          candidate.subject_public_key, candidate.anchor_hash);
      return std::any_of(
          committee.begin(), committee.end(), [&candidate](const auto &entry) {
            return entry.verifier_public_key
                == candidate.verifier_public_key;
          });
    };
    const auto source = [this](
        const crypto::hash &hash, cryptonote::blobdata &blob) {
      cryptonote::block block{};
      bool orphan = false;
      if (!m_blockchain_storage.get_block_by_hash(hash, block, &orphan)
          || orphan || cryptonote::get_block_hash(block) != hash)
        return false;
      blob = cryptonote::block_to_blob(block);
      return true;
    };
    return qwertycoin::epose::answer_canonical_block_challenge_v2(
        challenge, context, {EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES},
        m_epose_v2_keystore.service_secret_key, authorize, source,
        response) == qwertycoin::epose::canonical_service_status_v2::accepted;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_current_blockchain_height() const
  {
    return m_blockchain_storage.get_current_blockchain_height();
  }
  //-----------------------------------------------------------------------------------------------
  void core::get_blockchain_top(uint64_t& height, crypto::hash& top_id) const
  {
    top_id = m_blockchain_storage.get_tail_id(height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks, std::vector<cryptonote::blobdata>& txs) const
  {
    return m_blockchain_storage.get_blocks(start_offset, count, blocks, txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks) const
  {
    return m_blockchain_storage.get_blocks(start_offset, count, blocks);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_blocks(uint64_t start_offset, size_t count, std::vector<block>& blocks) const
  {
    std::vector<std::pair<cryptonote::blobdata, cryptonote::block>> bs;
    if (!m_blockchain_storage.get_blocks(start_offset, count, bs))
      return false;
    for (const auto &b: bs)
      blocks.push_back(b.second);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_transactions(const std::vector<crypto::hash>& txs_ids, std::vector<cryptonote::blobdata>& txs, std::vector<crypto::hash>& missed_txs, bool pruned) const
  {
    return m_blockchain_storage.get_transactions_blobs(txs_ids, txs, missed_txs, pruned);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_split_transactions_blobs(const std::vector<crypto::hash>& txs_ids, std::vector<std::tuple<crypto::hash, cryptonote::blobdata, crypto::hash, cryptonote::blobdata>>& txs, std::vector<crypto::hash>& missed_txs) const
  {
    return m_blockchain_storage.get_split_transactions_blobs(txs_ids, txs, missed_txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_txpool_backlog(std::vector<tx_backlog_entry>& backlog, bool include_sensitive_txes) const
  {
    m_mempool.get_transaction_backlog(backlog, include_sensitive_txes);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_transactions(const std::vector<crypto::hash>& txs_ids, std::vector<transaction>& txs, std::vector<crypto::hash>& missed_txs, bool pruned) const
  {
    return m_blockchain_storage.get_transactions(txs_ids, txs, missed_txs, pruned);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_alternative_blocks(std::vector<block>& blocks) const
  {
    return m_blockchain_storage.get_alternative_blocks(blocks);
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_alternative_blocks_count() const
  {
    return m_blockchain_storage.get_alternative_blocks_count();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::init(const boost::program_options::variables_map& vm, const cryptonote::test_options *test_options, const GetCheckpointsCallback& get_checkpoints/* = nullptr */, bool allow_dns)
  {
    start_time = std::time(nullptr);

    const bool regtest = command_line::get_arg(vm, arg_regtest_on);
    if (test_options != NULL || regtest)
    {
      m_nettype = FAKECHAIN;
    }
    bool r = handle_command_line(vm);
    CHECK_AND_ASSERT_MES(r, false, "Failed to handle command line");
    m_disable_dns_checkpoints |= not allow_dns;

    std::string db_sync_mode = command_line::get_arg(vm, cryptonote::arg_db_sync_mode);
    bool db_salvage = command_line::get_arg(vm, cryptonote::arg_db_salvage) != 0;
    bool fast_sync = command_line::get_arg(vm, arg_fast_block_sync) != 0;
    uint64_t blocks_threads = command_line::get_arg(vm, arg_prep_blocks_threads);
    std::string check_updates_string = command_line::get_arg(vm, arg_check_updates);
    size_t max_txpool_weight = command_line::get_arg(vm, arg_max_txpool_weight);
    bool prune_blockchain = command_line::get_arg(vm, arg_prune_blockchain);
    bool keep_alt_blocks = command_line::get_arg(vm, arg_keep_alt_blocks);
    bool keep_fakechain = command_line::get_arg(vm, arg_keep_fakechain);

    boost::filesystem::path folder(m_config_folder);
    // --regtest already appends "fake" through arg_data_dir. Some tests set
    // FAKECHAIN directly through test_options instead of command line args, so
    // preserve the legacy fakechain isolation for those callers.
    if (m_nettype == FAKECHAIN && !command_line::get_arg(vm, arg_regtest_on))
      folder /= "fake";

    // make sure the data directory exists, and try to lock it
    CHECK_AND_ASSERT_MES (boost::filesystem::exists(folder) || boost::filesystem::create_directories(folder), false,
      std::string("Failed to create directory ").append(folder.string()).c_str());

    // check for blockchain.bin
    try
    {
      const boost::filesystem::path old_files = folder;
      if (boost::filesystem::exists(old_files / "blockchain.bin"))
      {
        MWARNING("Found old-style blockchain.bin in " << old_files.string());
        MWARNING("Qwertycoin now uses a new format. You can either remove blockchain.bin to start syncing");
        MWARNING("the blockchain anew, or use qwertycoin-blockchain-export and qwertycoin-blockchain-import to");
        MWARNING("convert your existing blockchain.bin to the new format. See README.md for instructions.");
        return false;
      }
    }
    // folder might not be a directory, etc, etc
    catch (...) { }

    std::unique_ptr<BlockchainDB> db(new_db());
    if (db == NULL)
    {
      LOG_ERROR("Failed to initialize a database");
      return false;
    }

    folder /= db->get_db_name();
    MGINFO("Loading blockchain from folder " << folder.string() << " ...");

    const std::string filename = folder.string();
    // default to fast:async:1 if overridden
    blockchain_db_sync_mode sync_mode = db_defaultsync;
    bool sync_on_blocks = true;
    uint64_t sync_threshold = 1;

    if (m_nettype == FAKECHAIN && !keep_fakechain)
    {
      // reset the db by removing the database file before opening it
      if (!db->remove_data_file(filename))
      {
        MERROR("Failed to remove data file in " << filename);
        return false;
      }
    }

    try
    {
      uint64_t db_flags = 0;

      std::vector<std::string> options;
      boost::trim(db_sync_mode);
      boost::split(options, db_sync_mode, boost::is_any_of(" :"));
      const bool db_sync_mode_is_default = command_line::is_arg_defaulted(vm, cryptonote::arg_db_sync_mode);

      for(const auto &option : options)
        MDEBUG("option: " << option);

      // default to fast:async:1
      uint64_t DEFAULT_FLAGS = DBF_FAST;

      if(options.size() == 0)
      {
        // default to fast:async:1
        db_flags = DEFAULT_FLAGS;
      }

      bool safemode = false;
      if(options.size() >= 1)
      {
        if(options[0] == "safe")
        {
          safemode = true;
          db_flags = DBF_SAFE;
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_nosync;
        }
        else if(options[0] == "fast")
        {
          db_flags = DBF_FAST;
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_async;
        }
        else if(options[0] == "fastest")
        {
          db_flags = DBF_FASTEST;
          sync_threshold = 1000; // default to fastest:async:1000
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_async;
        }
        else
          db_flags = DEFAULT_FLAGS;
      }

      if(options.size() >= 2 && !safemode)
      {
        if(options[1] == "sync")
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_sync;
        else if(options[1] == "async")
          sync_mode = db_sync_mode_is_default ? db_defaultsync : db_async;
      }

      if(options.size() >= 3 && !safemode)
      {
        char *endptr;
        uint64_t threshold = strtoull(options[2].c_str(), &endptr, 0);
        if (*endptr == '\0' || !strcmp(endptr, "blocks"))
        {
          sync_on_blocks = true;
          sync_threshold = threshold;
        }
        else if (!strcmp(endptr, "bytes"))
        {
          sync_on_blocks = false;
          sync_threshold = threshold;
        }
        else
        {
          LOG_ERROR("Invalid db sync mode: " << options[2]);
          return false;
        }
      }

      if (db_salvage)
        db_flags |= DBF_SALVAGE;

      db->open(filename, db_flags);
      if(!db->m_open)
        return false;
    }
    catch (const DB_ERROR& e)
    {
      LOG_ERROR("Error opening database: " << e.what());
      return false;
    }

    m_blockchain_storage.set_user_options(blocks_threads,
        sync_on_blocks, sync_threshold, sync_mode, fast_sync);

    try
    {
      if (!command_line::is_arg_defaulted(vm, arg_block_notify))
      {
        struct hash_notify
        {
          tools::Notify cmdline;

          void operator()(std::uint64_t, epee::span<const block> blocks) const
          {
            for (const block& bl : blocks)
              cmdline.notify("%s", epee::string_tools::pod_to_hex(get_block_hash(bl)).c_str(), NULL);
          }
        };

        m_blockchain_storage.add_block_notify(hash_notify{{command_line::get_arg(vm, arg_block_notify).c_str()}});
      }
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to parse block notify spec: " << e.what());
    }

    try
    {
      if (!command_line::is_arg_defaulted(vm, arg_reorg_notify))
        m_blockchain_storage.set_reorg_notify(std::shared_ptr<tools::Notify>(new tools::Notify(command_line::get_arg(vm, arg_reorg_notify).c_str())));
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to parse reorg notify spec: " << e.what());
    }

    try
    {
      if (!command_line::is_arg_defaulted(vm, arg_block_rate_notify))
        m_block_rate_notify.reset(new tools::Notify(command_line::get_arg(vm, arg_block_rate_notify).c_str()));
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to parse block rate notify spec: " << e.what());
    }

    const std::pair<uint8_t, uint64_t> regtest_hard_forks[3] = {std::make_pair(1, 0), std::make_pair(mainnet_hard_forks[num_mainnet_hard_forks-1].version, 1), std::make_pair(0, 0)};
    const cryptonote::test_options regtest_test_options = {
      regtest_hard_forks,
      0
    };
    const difficulty_type fixed_difficulty = command_line::get_arg(vm, arg_fixed_difficulty);
    r = m_blockchain_storage.init(db.release(), m_nettype, m_offline, regtest ? &regtest_test_options : test_options, fixed_difficulty, get_checkpoints);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize blockchain storage");

    if (m_epose_local_service_node_config.enabled)
    {
      CHECK_AND_ASSERT_MES(!prune_blockchain, false,
          "--" << arg_prune_blockchain.name << " cannot be used together with --" << arg_service_node.name
          << "; EPoSE service nodes require an unpruned chain database for the current testnet protocol");
      CHECK_AND_ASSERT_MES(!m_blockchain_storage.get_blockchain_pruning_seed(), false,
          "EPoSE service-node mode requires an unpruned chain database for the current testnet protocol");
    }
    if (m_epose_v2_service_enabled)
    {
      CHECK_AND_ASSERT_MES(!prune_blockchain, false,
          "EPoSE-v2 service mode requires an unpruned chain database");
      CHECK_AND_ASSERT_MES(!m_blockchain_storage.get_blockchain_pruning_seed(), false,
          "EPoSE-v2 service mode requires an unpruned chain database");
      CHECK_AND_ASSERT_MES(init_epose_v2_service_runtime(), false,
          "Failed to initialize EPoSE-v2 service runtime");
    }

    r = m_mempool.init(max_txpool_weight, m_nettype == FAKECHAIN);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize memory pool");

    // now that we have a valid m_blockchain_storage, we can clean out any
    // transactions in the pool that do not conform to the current fork
    m_mempool.validate(m_blockchain_storage.get_current_hard_fork_version());

    bool show_time_stats = command_line::get_arg(vm, arg_show_time_stats) != 0;
    m_blockchain_storage.set_show_time_stats(show_time_stats);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize blockchain storage");

    block_sync_size = command_line::get_arg(vm, arg_block_sync_size);
    if (block_sync_size > BLOCKS_SYNCHRONIZING_MAX_COUNT)
      MERROR("Error --block-sync-size cannot be greater than " << BLOCKS_SYNCHRONIZING_MAX_COUNT);

    MGINFO("Loading checkpoints");

    // load json & DNS checkpoints, and verify them
    // with respect to what blocks we already have
    const bool skip_dns_checkpoints = !command_line::get_arg(vm, arg_dns_checkpoints);
    CHECK_AND_ASSERT_MES(update_checkpoints(skip_dns_checkpoints), false, "One or more checkpoints loaded from json or dns conflicted with existing checkpoints.");

   // DNS versions checking
    if (check_updates_string == "disabled" || not allow_dns)
      check_updates_level = UPDATES_DISABLED;
    else if (check_updates_string == "notify")
      check_updates_level = UPDATES_NOTIFY;
    else if (check_updates_string == "download")
      check_updates_level = UPDATES_DOWNLOAD;
    else if (check_updates_string == "update")
      check_updates_level = UPDATES_UPDATE;
    else {
      MERROR("Invalid argument to --check-updates: " << check_updates_string);
      return false;
    }

    r = m_miner.init(vm, m_nettype);
    CHECK_AND_ASSERT_MES(r, false, "Failed to initialize miner instance");

    if (!keep_alt_blocks && !m_blockchain_storage.get_db().is_read_only())
      m_blockchain_storage.get_db().drop_alt_blocks();

    if (prune_blockchain)
    {
      // display a message if the blockchain is not pruned yet
      if (!m_blockchain_storage.get_blockchain_pruning_seed())
      {
        MGINFO("Pruning blockchain...");
        CHECK_AND_ASSERT_MES(m_blockchain_storage.prune_blockchain(), false, "Failed to prune blockchain");
      }
      else
      {
        CHECK_AND_ASSERT_MES(m_blockchain_storage.update_blockchain_pruning(), false, "Failed to update blockchain pruning");
      }
    }

    return load_state_data();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::set_genesis_block(const block& b)
  {
    return m_blockchain_storage.reset_and_set_genesis_block(b);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::load_state_data()
  {
    // may be some code later
    return true;
  }
  //-----------------------------------------------------------------------------------------------
    bool core::deinit()
  {
    m_miner.stop();
    m_mempool.deinit();
    m_blockchain_storage.deinit();
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::test_drop_download()
  {
    m_test_drop_download = false;
  }
  //-----------------------------------------------------------------------------------------------
  void core::test_drop_download_height(uint64_t height)
  {
    m_test_drop_download_height = height;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_test_drop_download() const
  {
    return m_test_drop_download;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_test_drop_download_height() const
  {
    if (m_test_drop_download_height == 0)
      return true;

    if (get_blockchain_storage().get_current_blockchain_height() <= m_test_drop_download_height)
      return true;

    return false;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_tx(const blobdata& tx_blob, tx_verification_context& tvc, relay_method tx_relay, bool relayed)
  {
    tvc = {};

    TRY_ENTRY();

    CRITICAL_REGION_LOCAL(m_incoming_tx_lock);

    if (tx_blob.size() > get_max_tx_size())
    {
      LOG_PRINT_L1("WRONG TRANSACTION BLOB, too big size " << tx_blob.size() << ", rejected");
      tvc.m_verifivation_failed = true;
      tvc.m_too_big = true;
      return false;
    }

    transaction tx;
    crypto::hash txid;
    if (!parse_and_validate_tx_from_blob(tx_blob, tx, txid))
    {
      LOG_PRINT_L1("Incoming transactions failed to parse, rejected");
      tvc.m_verifivation_failed = true;
      return false;
    }

    const uint64_t tx_weight = get_transaction_weight(tx, tx_blob.size());
    if (!add_new_tx(tx, txid, tx_blob, tx_weight, tvc, tx_relay, relayed))
      return false;

    if (tvc.m_verifivation_failed)
    {
      MERROR_VER("Transaction verification failed: " << txid);
      return false;
    }
    else if (tvc.m_verifivation_impossible)
    {
      MERROR_VER("Transaction verification impossible: " << txid);
      return false;
    }
    else if (!tvc.m_added_to_pool)
    {
      MDEBUG("Transaction " << txid << " not added to pool");
      return true;
    }

    MDEBUG("tx added to pool: " << txid);

    return true;
    CATCH_ENTRY_L0("core::handle_incoming_tx()", false);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_semantic(const transaction& tx, tx_verification_context& tvc,
      uint8_t hf_version)
  {
    if(!tx.vin.size())
    {
      MERROR_VER("tx with empty inputs, rejected for tx id= " << get_transaction_hash(tx));
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_input = true;
      return false;
    }

    if(!check_inputs_types_supported(tx))
    {
      MERROR_VER("unsupported input types for tx id= " << get_transaction_hash(tx));
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_input = true;
      return false;
    }

    if(!check_outs_valid(tx))
    {
      MERROR_VER("tx with invalid outputs, rejected for tx id= " << get_transaction_hash(tx));
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_output = true;
      return false;
    }
    if (tx.version > 1)
    {
      if (tx.rct_signatures.outPk.size() != tx.vout.size())
      {
        MERROR_VER("tx with mismatched vout/outPk count, rejected for tx id= " << get_transaction_hash(tx));
        tvc.m_verifivation_failed = true;
        tvc.m_invalid_output = true;
        return false;
      }
    }

    if(!check_money_overflow(tx))
    {
      MERROR_VER("tx has money overflow, rejected for tx id= " << get_transaction_hash(tx));
      tvc.m_verifivation_failed = true;
      tvc.m_overspend = true;
      return false;
    }

    if (tx.version == 1)
    {
      uint64_t amount_in = 0;
      get_inputs_money_amount(tx, amount_in);
      uint64_t amount_out = get_outs_money_amount(tx);

      if(amount_in <= amount_out)
      {
        MERROR_VER("tx with wrong amounts: ins " << amount_in << ", outs " << amount_out << ", rejected for tx id= " << get_transaction_hash(tx));
        tvc.m_verifivation_failed = true;
        tvc.m_overspend = true;
        return false;
      }
    }
    // for version > 1, ringct signatures check verifies amounts match

    //check if tx use different key images
    if(!check_tx_inputs_keyimages_diff(tx))
    {
      MERROR_VER("tx uses a single key image more than once");
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_input = true;
      return false;
    }

    if (!check_tx_inputs_ring_members_diff(tx, hf_version))
    {
      MERROR_VER("tx uses duplicate ring members");
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_input = true;
      return false;
    }

    if (!check_tx_inputs_keyimages_domain(tx))
    {
      MERROR_VER("tx uses key image not in the valid domain");
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_input = true;
      return false;
    }

    if (!check_output_types(tx, hf_version))
    {
      MERROR_VER("tx does not use valid output type(s)");
      tvc.m_verifivation_failed = true;
      tvc.m_invalid_output = true;
      return false;
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::is_key_image_spent(const crypto::key_image &key_image) const
  {
    return m_blockchain_storage.have_tx_keyimg_as_spent(key_image);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::are_key_images_spent(const std::vector<crypto::key_image>& key_im, std::vector<bool> &spent) const
  {
    spent.clear();
    for(auto& ki: key_im)
    {
      spent.push_back(m_blockchain_storage.have_tx_keyimg_as_spent(ki));
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_block_sync_size(uint64_t height) const
  {
    static const uint64_t quick_height = m_nettype == TESTNET ? 801219 : m_nettype == MAINNET ? 1220516 : 0;
    size_t res = 0;
    if (block_sync_size > 0)
      res = block_sync_size;
    else if (height >= quick_height)
      res = BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
    else
      res = BLOCKS_SYNCHRONIZING_DEFAULT_COUNT_PRE_V4;

    static size_t max_block_size = 0;
    if (max_block_size == 0)
    {
      const char *env = getenv("SEEDHASH_EPOCH_BLOCKS");
      if (env)
      {
        int n = atoi(env);
        if (n <= 0)
          n = BLOCKS_SYNCHRONIZING_MAX_COUNT;
        size_t p = 1;
        while (p < (size_t)n)
          p <<= 1;
        max_block_size = p;
      }
      else
        max_block_size = BLOCKS_SYNCHRONIZING_MAX_COUNT;
    }
    if (res > max_block_size)
    {
      static bool warned = false;
      if (!warned)
      {
        MWARNING("Clamping block sync size to " << max_block_size);
        warned = true;
      }
      res = max_block_size;
    }
    return res;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::are_key_images_spent_in_pool(const std::vector<crypto::key_image>& key_im, std::vector<bool> &spent) const
  {
    spent.clear();

    return m_mempool.check_for_key_images(key_im, spent);
  }
  //-----------------------------------------------------------------------------------------------
  std::pair<boost::multiprecision::uint128_t, boost::multiprecision::uint128_t> core::get_coinbase_tx_sum(const uint64_t start_offset, const size_t count)
  {
    boost::multiprecision::uint128_t emission_amount = 0;
    boost::multiprecision::uint128_t total_fee_amount = 0;
    if (count)
    {
      const uint64_t end = start_offset + count - 1;
      m_blockchain_storage.for_blocks_range(start_offset, end,
        [this, &emission_amount, &total_fee_amount](uint64_t, const crypto::hash& hash, const block& b){
      std::vector<transaction> txs;
      std::vector<crypto::hash> missed_txs;
      uint64_t coinbase_amount = get_outs_money_amount(b.miner_tx);
      this->get_transactions(b.tx_hashes, txs, missed_txs, true);
      uint64_t tx_fee_amount = 0;
      for(const auto& tx: txs)
      {
        tx_fee_amount += get_tx_fee(tx);
      }
      
      emission_amount += coinbase_amount - tx_fee_amount;
      total_fee_amount += tx_fee_amount;
      return true;
      });
    }

    return std::pair<boost::multiprecision::uint128_t, boost::multiprecision::uint128_t>(emission_amount, total_fee_amount);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_inputs_keyimages_diff(const transaction& tx)
  {
    std::unordered_set<crypto::key_image> ki;
    for(const auto& in: tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      if(!ki.insert(tokey_in.k_image).second)
        return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_inputs_ring_members_diff(const transaction& tx, const uint8_t hf_version)
  {
    if (hf_version >= 6)
    {
      for(const auto& in: tx.vin)
      {
        CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
        for (size_t n = 1; n < tokey_in.key_offsets.size(); ++n)
          if (tokey_in.key_offsets[n] == 0)
            return false;
      }
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_tx_inputs_keyimages_domain(const transaction& tx)
  {
    std::unordered_set<crypto::key_image> ki;
    for(const auto& in: tx.vin)
    {
      CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, tokey_in, false);
      if (rct::ki2rct(tokey_in.k_image) == rct::identity())
        return false;
      if (!(rct::scalarmultKey(rct::ki2rct(tokey_in.k_image), rct::curveOrder()) == rct::identity()))
        return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::add_new_tx(transaction& tx, tx_verification_context& tvc, relay_method tx_relay, bool relayed)
  {
    crypto::hash tx_hash = get_transaction_hash(tx);
    blobdata bl;
    t_serializable_object_to_blob(tx, bl);
    size_t tx_weight = get_transaction_weight(tx, bl.size());
    return add_new_tx(tx, tx_hash, bl, tx_weight, tvc, tx_relay, relayed);
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_blockchain_total_transactions() const
  {
    return m_blockchain_storage.get_total_transactions();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::add_new_tx(transaction& tx, const crypto::hash& tx_hash, const cryptonote::blobdata &blob, size_t tx_weight, tx_verification_context& tvc, relay_method tx_relay, bool relayed)
  {
    if(m_mempool.have_tx(tx_hash, relay_category::legacy))
    {
      LOG_PRINT_L2("tx " << tx_hash << "already have transaction in tx_pool");
      return true;
    }

    if(m_blockchain_storage.have_tx(tx_hash))
    {
      LOG_PRINT_L2("tx " << tx_hash << " already have transaction in blockchain");
      return true;
    }

    uint8_t version = m_blockchain_storage.get_current_hard_fork_version();
    const bool res = m_mempool.add_tx(tx, tx_hash, blob, tx_weight, tvc, tx_relay, relayed, version);

    // If new incoming tx passed verification and entered the pool, notify ZMQ
    if (!tvc.m_verifivation_failed && res && matches_category(tvc.m_relay, relay_category::legacy))
    {
      m_blockchain_storage.notify_txpool_event({txpool_event{
        .tx = tx,
        .hash = tx_hash,
        .blob_size = blob.size(),
        .weight = tx_weight,
        .res = true}});
    }

    return res;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::relay_txpool_transactions()
  {
    // we attempt to relay txes that should be relayed, but were not
    std::vector<std::tuple<crypto::hash, cryptonote::blobdata, relay_method>> txs;
    if (m_mempool.get_relayable_transactions(txs) && !txs.empty())
    {
      NOTIFY_NEW_TRANSACTIONS::request public_req{};
      NOTIFY_NEW_TRANSACTIONS::request private_req{};
      NOTIFY_NEW_TRANSACTIONS::request stem_req{};
      for (auto& tx : txs)
      {
        switch (std::get<2>(tx))
        {
          default:
          case relay_method::none:
            break;
          case relay_method::local:
            private_req.txs.push_back(std::move(std::get<1>(tx)));
            break;
          case relay_method::forward:
            stem_req.txs.push_back(std::move(std::get<1>(tx)));
            break;
          case relay_method::block:
          case relay_method::fluff:
          case relay_method::stem:
            public_req.txs.push_back(std::move(std::get<1>(tx)));
            break;
        }
      }

      /* All txes are sent on randomized timers per connection in
         `src/cryptonote_protocol/levin_notify.cpp.` They are either sent with
         "white noise" delays or via  diffusion (Dandelion++ fluff). So
         re-relaying public and private _should_ be acceptable here. */
      const boost::uuids::uuid source = boost::uuids::nil_uuid();
      if (!public_req.txs.empty())
        get_protocol()->relay_transactions(public_req, source, epee::net_utils::zone::public_, relay_method::fluff);
      if (!private_req.txs.empty())
        get_protocol()->relay_transactions(private_req, source, epee::net_utils::zone::invalid, relay_method::local);
      if (!stem_req.txs.empty())
        get_protocol()->relay_transactions(stem_req, source, epee::net_utils::zone::public_, relay_method::stem);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::notify_txpool_event(const epee::span<const cryptonote::blobdata> tx_blobs, epee::span<const crypto::hash> tx_hashes, epee::span<const cryptonote::transaction> txs, const std::vector<bool> &just_broadcasted) const
  {
    if (tx_blobs.size() != tx_hashes.size() || tx_blobs.size() != txs.size() || tx_blobs.size() != just_broadcasted.size())
      return false;

    /* Publish txs via ZMQ that are "just broadcasted" by the daemon. This is
       done here in order to guarantee txs
       are pub'd via ZMQ when we know the daemon has/will broadcast to other
       nodes & *after* the tx is visible in the pool. This should get called
       when the user submits a tx to a daemon in the "fluff" epoch relaying txs
       via a public network. */
    if (std::count(just_broadcasted.begin(), just_broadcasted.end(), true) == 0)
      return true;

    std::vector<txpool_event> results{};
    results.resize(tx_blobs.size());
    for (std::size_t i = 0; i < results.size(); ++i)
    {
      results[i].tx = std::move(txs[i]);
      results[i].hash = std::move(tx_hashes[i]);
      results[i].blob_size = tx_blobs[i].size();
      results[i].weight = results[i].tx.pruned ? get_pruned_transaction_weight(results[i].tx) : get_transaction_weight(results[i].tx, results[i].blob_size);
      results[i].res = just_broadcasted[i];
    }

    m_blockchain_storage.notify_txpool_event(std::move(results));

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::on_transactions_relayed(const epee::span<const cryptonote::blobdata> tx_blobs, const relay_method tx_relay)
  {
    // lock ensures duplicate txs aren't pub'd via zmq
    CRITICAL_REGION_LOCAL(m_incoming_tx_lock);

    std::vector<crypto::hash> tx_hashes{};
    tx_hashes.resize(tx_blobs.size());

    std::vector<cryptonote::transaction> txs{};
    txs.resize(tx_blobs.size());

    for (std::size_t i = 0; i < tx_blobs.size(); ++i)
    {
      if (!parse_and_validate_tx_from_blob(tx_blobs[i], txs[i], tx_hashes[i]))
      {
        LOG_ERROR("Failed to parse relayed transaction");
        return;
      }
    }

    std::vector<bool> just_broadcasted{};
    just_broadcasted.reserve(tx_hashes.size());

    m_mempool.set_relayed(epee::to_span(tx_hashes), tx_relay, just_broadcasted);

    if (matches_category(tx_relay, relay_category::legacy))
      notify_txpool_event(tx_blobs, epee::to_span(tx_hashes), epee::to_span(txs), just_broadcasted);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_block_template(block& b, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
  {
    blobdata epose_extra_nonce;
    if (!build_epose_miner_extra_nonce(epose_extra_nonce))
      return false;
    if (!m_blockchain_storage.create_block_template(b, adr, diffic, height, expected_reward, ex_nonce.empty() ? epose_extra_nonce : ex_nonce, seed_height, seed_hash))
      return false;
    return ex_nonce.empty() || append_extra_nonce_to_miner_tx(b.miner_tx, epose_extra_nonce);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_block_template(block& b, const crypto::hash *prev_block, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
  {
    blobdata epose_extra_nonce;
    if (!build_epose_miner_extra_nonce(epose_extra_nonce))
      return false;
    if (!m_blockchain_storage.create_block_template(b, prev_block, adr, diffic, height, expected_reward, ex_nonce.empty() ? epose_extra_nonce : ex_nonce, seed_height, seed_hash))
      return false;
    return ex_nonce.empty() || append_extra_nonce_to_miner_tx(b.miner_tx, epose_extra_nonce);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_miner_data(uint8_t& major_version, uint64_t& height, crypto::hash& prev_id, crypto::hash& seed_hash, difficulty_type& difficulty, uint64_t& median_weight, uint64_t& already_generated_coins, std::vector<tx_block_template_backlog_entry>& tx_backlog)
  {
    return m_blockchain_storage.get_miner_data(major_version, height, prev_id, seed_hash, difficulty, median_weight, already_generated_coins, tx_backlog);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, bool clip_pruned, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) const
  {
    return m_blockchain_storage.find_blockchain_supplement(qblock_ids, clip_pruned, resp);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::find_blockchain_supplement(const uint64_t req_start_block, const std::list<crypto::hash>& qblock_ids, std::vector<std::pair<std::pair<cryptonote::blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, cryptonote::blobdata> > > >& blocks, uint64_t& total_height, uint64_t& start_height, bool pruned, bool get_miner_tx_hash, size_t max_block_count, size_t max_tx_count) const
  {
    return m_blockchain_storage.find_blockchain_supplement(req_start_block, qblock_ids, blocks, total_height, start_height, pruned, get_miner_tx_hash, max_block_count, max_tx_count);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_outs(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res) const
  {
    return m_blockchain_storage.get_outs(req, res);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_output_distribution(uint64_t amount, uint64_t from_height, uint64_t to_height, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) const
  {
    return m_blockchain_storage.get_output_distribution(amount, from_height, to_height, start_height, distribution, base);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) const
  {
    return m_blockchain_storage.get_tx_outputs_gindexs(tx_id, indexs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_tx_outputs_gindexs(const crypto::hash& tx_id, size_t n_txes, std::vector<std::vector<uint64_t>>& indexs) const
  {
    return m_blockchain_storage.get_tx_outputs_gindexs(tx_id, n_txes, indexs);
  }
  //-----------------------------------------------------------------------------------------------
  void core::pause_mine()
  {
    m_miner.pause();
  }
  //-----------------------------------------------------------------------------------------------
  void core::resume_mine()
  {
    m_miner.resume();
  }
  //-----------------------------------------------------------------------------------------------
  block_complete_entry get_block_complete_entry(block& b, tx_memory_pool &pool)
  {
    block_complete_entry bce;
    bce.block = cryptonote::block_to_blob(b);
    bce.block_weight = 0; // we can leave it to 0, those txes aren't pruned
    for (const auto &tx_hash: b.tx_hashes)
    {
      cryptonote::blobdata txblob;
      CHECK_AND_ASSERT_THROW_MES(pool.get_transaction(tx_hash, txblob, relay_category::all), "Transaction not found in pool");
      bce.txs.push_back({txblob, crypto::null_hash});
    }
    return bce;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_block_found(block& b, block_verification_context &bvc)
  {
    bvc = {};
    m_miner.pause();
    std::vector<block_complete_entry> blocks;
    try
    {
      blocks.push_back(get_block_complete_entry(b, m_mempool));
    }
    catch (const std::exception &e)
    {
      m_miner.resume();
      return false;
    }
    std::vector<block> pblocks;
    if (!prepare_handle_incoming_blocks(blocks, pblocks))
    {
      MERROR("Block found, but failed to prepare to add");
      m_miner.resume();
      return false;
    }
    m_blockchain_storage.add_new_block(b, bvc);
    cleanup_handle_incoming_blocks(true);
    //anyway - update miner template
    update_miner_block_template();
    m_miner.resume();


    CHECK_AND_ASSERT_MES(!bvc.m_verifivation_failed, false, "mined block failed verification");
    if(bvc.m_added_to_main_chain)
    {
      cryptonote_connection_context exclude_context = {};
      NOTIFY_NEW_FLUFFY_BLOCK::request arg{};
      arg.current_blockchain_height = m_blockchain_storage.get_current_blockchain_height();
      std::vector<crypto::hash> missed_txs;
      for (const auto &tx_hash : b.tx_hashes)
      {
        if (m_blockchain_storage.have_tx(tx_hash))
          continue;
        missed_txs.push_back(tx_hash);
      }
      if(missed_txs.size() &&  m_blockchain_storage.get_block_id_by_height(get_block_height(b)) != get_block_hash(b))
      {
        LOG_PRINT_L1("Block found but, seems that reorganize just happened after that, do not relay this block");
        return true;
      }
      CHECK_AND_ASSERT_MES(!missed_txs.size(), false, "can't find some transactions in found block:" << get_block_hash(b)
        << " b.tx_hashes.size()=" << b.tx_hashes.size() << ", missed_txs.size()" << missed_txs.size());

      block_to_blob(b, arg.b.block);
      // Relay an empty fluffy block
      arg.b.txs.clear();

      m_pprotocol->relay_block(arg, exclude_context);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::is_synchronized() const
  {
    return m_pprotocol != nullptr && m_pprotocol->is_synchronized();
  }
  //-----------------------------------------------------------------------------------------------
  void core::on_synchronized()
  {
    m_miner.on_synchronized();
  }
  //-----------------------------------------------------------------------------------------------
  void core::safesyncmode(const bool onoff)
  {
    m_blockchain_storage.safesyncmode(onoff);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::add_new_block(const block& b, block_verification_context& bvc,
    pool_supplement& extra_block_txs)
  {
    return m_blockchain_storage.add_new_block(b, bvc, extra_block_txs);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::prepare_handle_incoming_blocks(const std::vector<block_complete_entry> &blocks_entry, std::vector<block> &blocks)
  {
    m_incoming_tx_lock.lock();
    bool success = false;
    try { success = m_blockchain_storage.prepare_handle_incoming_blocks(blocks_entry, blocks); }
    catch (const std::exception &e) { MERROR("Failed prepare handle incoming blocks: " << e.what()); }
    catch (...) { MERROR("Failed prepare handling incoming blocks"); }
    if (!success)
    {
      cleanup_handle_incoming_blocks(false);
      return false;
    }
    return true;
  }

  //-----------------------------------------------------------------------------------------------
  bool core::cleanup_handle_incoming_blocks(bool force_sync)
  {
    bool success = false;
    try {
      success = m_blockchain_storage.cleanup_handle_incoming_blocks(force_sync);
    }
    catch (...) {}
    m_incoming_tx_lock.unlock();
    return success;
  }

  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_block(const blobdata& block_blob, const block *b,
    block_verification_context& bvc, bool update_miner_blocktemplate)
  {
    pool_supplement ps{};
    return handle_incoming_block(block_blob, b, bvc, ps, update_miner_blocktemplate);
  }

  //-----------------------------------------------------------------------------------------------
  bool core::handle_incoming_block(const blobdata& block_blob, const block *b,
    block_verification_context& bvc, pool_supplement& extra_block_txs, bool update_miner_blocktemplate)
  {
    TRY_ENTRY();

    bvc = {};

    if (!check_incoming_block_size(block_blob))
    {
      bvc.m_verifivation_failed = true;
      return false;
    }

    if (((size_t)-1) <= 0xffffffff && block_blob.size() >= 0x3fffffff)
      MWARNING("This block's size is " << block_blob.size() << ", closing on the 32 bit limit");

    block lb;
    if (!b)
    {
      crypto::hash block_hash;
      if(!parse_and_validate_block_from_blob(block_blob, lb, block_hash))
      {
        LOG_PRINT_L1("Failed to parse and validate new block");
        bvc.m_verifivation_failed = true;
        return false;
      }
      b = &lb;
    }
    add_new_block(*b, bvc, extra_block_txs);
    if(update_miner_blocktemplate && bvc.m_added_to_main_chain)
       update_miner_block_template();
    return true;

    CATCH_ENTRY_L0("core::handle_incoming_block()", false);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_single_incoming_block(const blobdata& block_blob,
    const block *b,
    block_verification_context& bvc,
    pool_supplement& extra_block_txs,
    bool update_miner_blocktemplate)
  {
    // Note: this estimate can be quite far off since fluffy blocks won't contain all their
    // transactions in the payload, but also this value doesn't *need* to be super precise. It
    // is used to trigger database backing store syncing once it hits a threshold, and since
    // we under-count the byte size here, it might result in under-syncing the backing store.
    // If force refresh is enabled, though, which the user turns on if they are vigilant about
    // saving each block, then it doesn't matter either way: cleanup_handle_incoming_blocks()
    // always triggers a sync.
    size_t block_total_bytes = block_blob.size();
    for (const auto &t : extra_block_txs.txs_by_txid)
      block_total_bytes += t.second.second.size();

    CRITICAL_REGION_LOCAL(m_incoming_tx_lock);

    // Match each call to prepare_handle_incoming_block_no_preprocess() with a call to
    // cleanup_handle_incoming_blocks()
    m_blockchain_storage.prepare_handle_incoming_block_no_preprocess(block_total_bytes);
    const auto auto_cleanup = epee::misc_utils::create_scope_leave_handler([this](){
      this->m_blockchain_storage.cleanup_handle_incoming_blocks();
    });

    return handle_incoming_block(block_blob,
      b,
      bvc,
      extra_block_txs,
      update_miner_blocktemplate);
  }
  //-----------------------------------------------------------------------------------------------
  // Used by the RPC server to check the size of an incoming
  // block_blob
  bool core::check_incoming_block_size(const blobdata& block_blob) const
  {
    // note: we assume block weight is always >= block blob size, so we check incoming
    // blob size against the block weight limit, which acts as a sanity check without
    // having to parse/weigh first; in fact, since the block blob is the block header
    // plus the tx hashes, the weight will typically be much larger than the blob size
    if(block_blob.size() > m_blockchain_storage.get_current_cumulative_block_weight_limit() + BLOCK_SIZE_SANITY_LEEWAY)
    {
      LOG_PRINT_L1("WRONG BLOCK BLOB, sanity check failed on size " << block_blob.size() << ", rejected");
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  crypto::hash core::get_tail_id() const
  {
    return m_blockchain_storage.get_tail_id();
  }
  //-----------------------------------------------------------------------------------------------
  difficulty_type core::get_block_cumulative_difficulty(uint64_t height) const
  {
    return m_blockchain_storage.get_db().get_block_cumulative_difficulty(height);
  }
  //-----------------------------------------------------------------------------------------------
  size_t core::get_pool_transactions_count(bool include_sensitive_txes) const
  {
    return m_mempool.get_transactions_count(include_sensitive_txes);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::have_block_unlocked(const crypto::hash& id, int *where) const
  {
    return m_blockchain_storage.have_block_unlocked(id, where);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::have_block(const crypto::hash& id, int *where) const
  {
    return m_blockchain_storage.have_block(id, where);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_transactions_info(const std::vector<crypto::hash>& txids, std::vector<std::pair<crypto::hash, tx_memory_pool::tx_details>>& txs, bool include_sensitive_txes) const
  {
    return m_mempool.get_transactions_info(txids, txs, include_sensitive_txes);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_transactions(std::vector<transaction>& txs, bool include_sensitive_data) const
  {
    m_mempool.get_transactions(txs, include_sensitive_data);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_transaction_hashes(std::vector<crypto::hash>& txs, bool include_sensitive_data) const
  {
    m_mempool.get_transaction_hashes(txs, include_sensitive_data);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_info(time_t start_time, bool include_sensitive_txes, size_t max_tx_count, std::vector<std::pair<crypto::hash, tx_memory_pool::tx_details>>& added_txs, std::vector<crypto::hash>& remaining_added_txids, std::vector<crypto::hash>& removed_txs, bool& incremental) const
  {
    return m_mempool.get_pool_info(start_time, include_sensitive_txes, max_tx_count, added_txs, remaining_added_txids, removed_txs, incremental);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_transaction_stats(struct txpool_stats& stats, bool include_sensitive_data) const
  {
    m_mempool.get_transaction_stats(stats, include_sensitive_data);
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_transaction(const crypto::hash &id, cryptonote::blobdata& tx, relay_category tx_category) const
  {
    return m_mempool.get_transaction(id, tx, tx_category);
  }  
  //-----------------------------------------------------------------------------------------------
  bool core::pool_has_tx(const crypto::hash &id) const
  {
    return m_mempool.have_tx(id, relay_category::legacy);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_transactions_and_spent_keys_info(std::vector<tx_info>& tx_infos, std::vector<spent_key_image_info>& key_image_infos, bool include_sensitive_data) const
  {
    return m_mempool.get_transactions_and_spent_keys_info(tx_infos, key_image_infos, include_sensitive_data);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_pool_for_rpc(std::vector<cryptonote::rpc::tx_in_pool>& tx_infos, cryptonote::rpc::key_images_with_tx_hashes& key_image_infos, bool include_sensitive) const
  {
    return m_mempool.get_pool_for_rpc(tx_infos, key_image_infos, include_sensitive);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_short_chain_history(std::list<crypto::hash>& ids, uint64_t& current_height) const
  {
    return m_blockchain_storage.get_short_chain_history(ids, current_height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp, cryptonote_connection_context& context)
  {
    return m_blockchain_storage.handle_get_objects(arg, rsp);
  }
  //-----------------------------------------------------------------------------------------------
  crypto::hash core::get_block_id_by_height(uint64_t height) const
  {
    return m_blockchain_storage.get_block_id_by_height(height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_block_by_hash(const crypto::hash &h, block &blk, bool *orphan) const
  {
    return m_blockchain_storage.get_block_by_hash(h, blk, orphan);
  }
  //-----------------------------------------------------------------------------------------------
  std::string core::print_pool(bool short_format) const
  {
    return m_mempool.print_pool(short_format);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::update_miner_block_template()
  {
    m_miner.on_block_chain_update();
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::on_idle()
  {
    if(!m_starter_message_showed)
    {
      std::string main_message;
      if (m_offline)
        main_message = "The daemon is running offline and will not attempt to sync to the Qwertycoin network.";
      else
        main_message = "The daemon will start synchronizing with the network. This may take a long time to complete.";
      MGINFO_YELLOW(ENDL << "**********************************************************************" << ENDL
        << main_message << ENDL
        << ENDL
        << "You can set the level of process detailization through \"set_log <level|categories>\" command," << ENDL
        << "where <level> is between 0 (no details) and 4 (very verbose), or custom category based levels (eg, *:WARNING)." << ENDL
        << ENDL
        << "Use the \"help\" command to see the list of available commands." << ENDL
        << "Use \"help <command>\" to see a command's documentation." << ENDL
        << "**********************************************************************" << ENDL);
      m_starter_message_showed = true;
    }

    relay_txpool_transactions(); // txpool handles periodic DB checking
    m_check_updates_interval.do_call(boost::bind(&core::check_updates, this));
    m_check_disk_space_interval.do_call(boost::bind(&core::check_disk_space, this));
    m_block_rate_interval.do_call(boost::bind(&core::check_block_rate, this));
    m_blockchain_pruning_interval.do_call(boost::bind(&core::update_blockchain_pruning, this));
    m_diff_recalc_interval.do_call(boost::bind(&core::recalculate_difficulties, this));
    m_miner.on_idle();
    m_mempool.on_idle();
    if (!update_epose_v2_service_producer())
      MERROR("EPoSE-v2 service producer update failed; it will retry without changing consensus state");
    if (!update_epose_v2_receipt_producer())
      MERROR("EPoSE-v2 receipt producer update failed; it will retry without changing consensus state");
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  uint8_t core::get_ideal_hard_fork_version() const
  {
    return get_blockchain_storage().get_ideal_hard_fork_version();
  }
  //-----------------------------------------------------------------------------------------------
  uint8_t core::get_ideal_hard_fork_version(uint64_t height) const
  {
    return get_blockchain_storage().get_ideal_hard_fork_version(height);
  }
  //-----------------------------------------------------------------------------------------------
  uint8_t core::get_hard_fork_version(uint64_t height) const
  {
    return get_blockchain_storage().get_hard_fork_version(height);
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_earliest_ideal_height_for_version(uint8_t version) const
  {
    return get_blockchain_storage().get_earliest_ideal_height_for_version(version);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_updates()
  {
    static const char software[] = "qwertycoin";
#ifdef BUILD_TAG
    static const char buildtag[] = BOOST_PP_STRINGIZE(BUILD_TAG);
    static const char subdir[] = "cli"; // because it can never be simple
#else
    static const char buildtag[] = "source";
    static const char subdir[] = "source"; // because it can never be simple
#endif

    if (m_offline)
      return true;

    if (check_updates_level == UPDATES_DISABLED)
      return true;

    std::string version, hash;
    MCDEBUG("updates", "Checking for a new " << software << " version for " << buildtag);
    if (!tools::check_updates(software, buildtag, version, hash))
      return false;

    if (tools::vercmp(version.c_str(), MONERO_VERSION) <= 0)
    {
      m_update_available = false;
      return true;
    }

    std::string url = tools::get_update_url(software, subdir, buildtag, version, true);
    MCLOG_CYAN(el::Level::Info, "global", "Version " << version << " of " << software << " for " << buildtag << " is available: " << url << ", SHA256 hash " << hash);
    m_update_available = true;

    if (check_updates_level == UPDATES_NOTIFY)
      return true;

    url = tools::get_update_url(software, subdir, buildtag, version, false);
    std::string filename;
    const char *slash = strrchr(url.c_str(), '/');
    if (slash)
      filename = slash + 1;
    else
      filename = std::string(software) + "-update-" + version;
    boost::filesystem::path path(epee::string_tools::get_current_module_folder());
    path /= filename;

    boost::unique_lock<boost::mutex> lock(m_update_mutex);

    if (m_update_download != 0)
    {
      MCDEBUG("updates", "Already downloading update");
      return true;
    }

    crypto::hash file_hash;
    if (!tools::sha256sum(path.string(), file_hash) || (hash != epee::string_tools::pod_to_hex(file_hash)))
    {
      MCDEBUG("updates", "We don't have that file already, downloading");
      const std::string tmppath = path.string() + ".tmp";
      if (epee::file_io_utils::is_file_exist(tmppath))
      {
        MCDEBUG("updates", "We have part of the file already, resuming download");
      }
      m_last_update_length = 0;
      m_update_download = tools::download_async(tmppath, url, [this, hash, path](const std::string &tmppath, const std::string &uri, bool success) {
        bool remove = false, good = true;
        if (success)
        {
          crypto::hash file_hash;
          if (!tools::sha256sum(tmppath, file_hash))
          {
            MCERROR("updates", "Failed to hash " << tmppath);
            remove = true;
            good = false;
          }
          else if (hash != epee::string_tools::pod_to_hex(file_hash))
          {
            MCERROR("updates", "Download from " << uri << " does not match the expected hash");
            remove = true;
            good = false;
          }
        }
        else
        {
          MCERROR("updates", "Failed to download " << uri);
          good = false;
        }
        boost::unique_lock<boost::mutex> lock(m_update_mutex);
        m_update_download = 0;
        if (success && !remove)
        {
          std::error_code e = tools::replace_file(tmppath, path.string());
          if (e)
          {
            MCERROR("updates", "Failed to rename downloaded file");
            good = false;
          }
        }
        else if (remove)
        {
          if (!boost::filesystem::remove(tmppath))
          {
            MCERROR("updates", "Failed to remove invalid downloaded file");
            good = false;
          }
        }
        if (good)
          MCLOG_CYAN(el::Level::Info, "updates", "New version downloaded to " << path.string());
      }, [this](const std::string &path, const std::string &uri, size_t length, ssize_t content_length) {
        if (length >= m_last_update_length + 1024 * 1024 * 10)
        {
          m_last_update_length = length;
          MCDEBUG("updates", "Downloaded " << length << "/" << (content_length ? std::to_string(content_length) : "unknown"));
        }
        return true;
      });
    }
    else
    {
      MCDEBUG("updates", "We already have " << path << " with expected hash");
    }

    lock.unlock();

    if (check_updates_level == UPDATES_DOWNLOAD)
      return true;

    MCERROR("updates", "Download/update not implemented yet");
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_disk_space()
  {
    uint64_t free_space = get_free_space();
    if (free_space < 1ull * 1024 * 1024 * 1024) // 1 GB
    {
      const el::Level level = el::Level::Warning;
      MCLOG_RED(level, "global", "Free space is below 1 GB on " << m_config_folder);
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  double factorial(unsigned int n)
  {
    if (n <= 1)
      return 1.0;
    double f = n;
    while (n-- > 1)
      f *= n;
    return f;
  }
  //-----------------------------------------------------------------------------------------------
  static double probability1(unsigned int blocks, unsigned int expected)
  {
    // https://www.umass.edu/wsp/resources/poisson/#computing
    return pow(expected, blocks) / (factorial(blocks) * exp(expected));
  }
  //-----------------------------------------------------------------------------------------------
  static double probability(unsigned int blocks, unsigned int expected)
  {
    double p = 0.0;
    if (blocks <= expected)
    {
      for (unsigned int b = 0; b <= blocks; ++b)
        p += probability1(b, expected);
    }
    else if (blocks > expected)
    {
      for (unsigned int b = blocks; b <= expected * 3 /* close enough */; ++b)
        p += probability1(b, expected);
    }
    return p;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_block_rate()
  {
    if (m_offline || m_nettype == FAKECHAIN || m_target_blockchain_height > get_current_blockchain_height() || m_target_blockchain_height == 0)
    {
      MDEBUG("Not checking block rate, offline or syncing");
      return true;
    }

    static constexpr double threshold = 1. / (864000 / DIFFICULTY_TARGET_V2); // one false positive every 10 days
    static constexpr unsigned int max_blocks_checked = 150;

    const time_t now = time(NULL);
    const std::vector<time_t> timestamps = m_blockchain_storage.get_last_block_timestamps(max_blocks_checked);

    static const unsigned int seconds[] = { 5400, 3600, 1800, 1200, 600 };
    for (size_t n = 0; n < sizeof(seconds)/sizeof(seconds[0]); ++n)
    {
      unsigned int b = 0;
      const time_t time_boundary = now - static_cast<time_t>(seconds[n]);
      for (time_t ts: timestamps) b += ts >= time_boundary;
      const double p = probability(b, seconds[n] / DIFFICULTY_TARGET_V2);
      MDEBUG("blocks in the last " << seconds[n] / 60 << " minutes: " << b << " (probability " << p << ")");
      if (p < threshold)
      {
        MWARNING("There were " << b << (b == max_blocks_checked ? " or more" : "") << " blocks in the last " << seconds[n] / 60 << " minutes, there might be large hash rate changes, or we might be partitioned, cut off from the Qwertycoin network or under attack, or your computer's time is off. Or it could be just sheer bad luck.");

        std::shared_ptr<tools::Notify> block_rate_notify = m_block_rate_notify;
        if (block_rate_notify)
        {
          auto expected = seconds[n] / DIFFICULTY_TARGET_V2;
          block_rate_notify->notify("%t", std::to_string(seconds[n] / 60).c_str(), "%b", std::to_string(b).c_str(), "%e", std::to_string(expected).c_str(), NULL);
        }

        break; // no need to look further
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::recalculate_difficulties()
  {
    m_blockchain_storage.recalculate_difficulties();
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::flush_invalid_blocks()
  {
    m_blockchain_storage.flush_invalid_blocks();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_txpool_complement(std::vector<crypto::hash> hashes, std::vector<cryptonote::blobdata> &txes)
  {
    return m_mempool.get_complement(std::move(hashes), txes);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::update_blockchain_pruning()
  {
    return m_blockchain_storage.update_blockchain_pruning();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::check_blockchain_pruning()
  {
    return m_blockchain_storage.check_blockchain_pruning();
  }
  //-----------------------------------------------------------------------------------------------
  void core::set_target_blockchain_height(uint64_t target_blockchain_height)
  {
    m_target_blockchain_height = target_blockchain_height;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_target_blockchain_height() const
  {
    return m_target_blockchain_height;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::prevalidate_block_hashes(uint64_t height, const std::vector<crypto::hash> &hashes, const std::vector<uint64_t> &weights)
  {
    return get_blockchain_storage().prevalidate_block_hashes(height, hashes, weights);
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_free_space() const
  {
    boost::filesystem::path path(m_config_folder);
    boost::filesystem::space_info si = boost::filesystem::space(path);
    return si.available;
  }
  //-----------------------------------------------------------------------------------------------
  uint32_t core::get_blockchain_pruning_seed() const
  {
    return get_blockchain_storage().get_blockchain_pruning_seed();
  }
  //-----------------------------------------------------------------------------------------------
  bool core::prune_blockchain(uint32_t pruning_seed)
  {
    CHECK_AND_ASSERT_MES(
        !m_epose_local_service_node_config.enabled && !m_epose_v2_service_enabled,
        false, "EPoSE service-node mode requires an unpruned chain database");
    return get_blockchain_storage().prune_blockchain(pruning_seed);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::is_within_compiled_block_hash_area(uint64_t height) const
  {
    return get_blockchain_storage().is_within_compiled_block_hash_area(height);
  }
  //-----------------------------------------------------------------------------------------------
  bool core::has_block_weights(uint64_t height, uint64_t nblocks) const
  {
    return get_blockchain_storage().has_block_weights(height, nblocks);
  }
  //-----------------------------------------------------------------------------------------------
  std::time_t core::get_start_time() const
  {
    return start_time;
  }
  //-----------------------------------------------------------------------------------------------
  void core::graceful_exit()
  {
    raise(SIGTERM);
  }
}
