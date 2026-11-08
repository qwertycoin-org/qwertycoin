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

#include "block_rate_probability.h"
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/scope_exit.hpp>
#include <boost/uuid/nil_generator.hpp>

#include <algorithm>
#include <atomic>
#include <sstream>

#include "string_tools.h"
using namespace epee;

#include <unordered_set>
#include "cryptonote_core.h"
#include "epose_http_client.h"
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
#include "epose/diagnostics_v2.h"
#include "epose/transport_committee_v2.h"
#include "epose/transport_profile_v2.h"
#include "epose/service_probe_connector_v2.h"
#include "epose/envelope_v2.h"
#include "epose/relay_hardening_v2.h"
#include "epose/record_codec_v2.h"
#include "cryptonote_core/epose_warning_throttle.h"
#include "epose/service_epoch.h"
#include "epose/coordinator_v2.h"
#include "epose/relay_pool_v2.h"

#include <boost/filesystem.hpp>

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "cn"

DISABLE_VS_WARNINGS(4355)

#define MERROR_VER(x) MCERROR("verify", x)

// basically at least how many bytes the block itself serializes to without the miner tx
#define BLOCK_SIZE_SANITY_LEEWAY 100

namespace
{

  bool epose_v2_producer_step_failed(
      const char *reason, std::atomic<uint64_t> &count)
  {
    MDEBUG("EPoSE-v2 service producer step failed: " << reason);
    return cryptonote::epose_producer_step_failed(count);
  }

  const char *epose_v2_relay_ingress_rejection_name(
      const qwertycoin::epose::relay_ingress_rejection_v2 rejection)
  {
    using reason = qwertycoin::epose::relay_ingress_rejection_v2;
    switch (rejection)
    {
    case reason::none: return "none";
    case reason::invalid_pool_or_policy: return "invalid_pool_or_policy";
    case reason::envelope_budget: return "envelope_budget";
    case reason::envelope_parse: return "envelope_parse";
    case reason::receipt_structure: return "receipt_structure";
    case reason::receipt_window_unavailable: return "receipt_window_unavailable";
    case reason::receipt_late_unauthenticated: return "receipt_late_unauthenticated";
    case reason::receipt_slot_unauthenticated: return "receipt_slot_unauthenticated";
    case reason::semantic_rejected: return "semantic_rejected";
    case reason::queue_expired: return "queue_expired";
    case reason::queue_full: return "queue_full";
    case reason::queue_rejected: return "queue_rejected";
    case reason::receipt_window_closed: return "receipt_window_closed";
    }
    return "unknown";
  }

  const char *epose_v2_lifecycle_status_name(
      const qwertycoin::epose::lifecycle_status_v2 status)
  {
    using result = qwertycoin::epose::lifecycle_status_v2;
    switch (status)
    {
    case result::accepted: return "accepted";
    case result::idempotent_duplicate: return "idempotent_duplicate";
    case result::invalid_context: return "invalid_context";
    case result::invalid_action: return "invalid_action";
    case result::invalid_descriptor: return "invalid_descriptor";
    case result::nonseparated_authorities: return "nonseparated_authorities";
    case result::identity_not_found: return "identity_not_found";
    case result::identity_already_exists: return "identity_already_exists";
    case result::service_key_in_use: return "service_key_in_use";
    case result::wrong_previous_descriptor: return "wrong_previous_descriptor";
    case result::wrong_sequence: return "wrong_sequence";
    case result::retroactive_change: return "retroactive_change";
    case result::overlapping_change: return "overlapping_change";
    case result::invalid_transition: return "invalid_transition";
    case result::invalid_operator_signature: return "invalid_operator_signature";
    case result::invalid_service_signature: return "invalid_service_signature";
    }
    return "unknown";
  }

  const char *epose_v2_semantic_status_name(
      const qwertycoin::epose::semantic_status_v2 status)
  {
    using result = qwertycoin::epose::semantic_status_v2;
    switch (status)
    {
    case result::accepted: return "accepted";
    case result::invalid_configuration: return "invalid_configuration";
    case result::before_activation: return "before_activation";
    case result::context_unavailable: return "context_unavailable";
    case result::invalid_record: return "invalid_record";
    case result::invalid_lifecycle: return "invalid_lifecycle";
    case result::lifecycle_binding_mismatch: return "lifecycle_binding_mismatch";
    case result::invalid_admission: return "invalid_admission";
    case result::invalid_receipt: return "invalid_receipt";
    case result::invalid_payment_proof: return "invalid_payment_proof";
    case result::payment_proof_wrong_carrier: return "payment_proof_wrong_carrier";
    case result::duplicate_payment_proof: return "duplicate_payment_proof";
    case result::outside_retained_window: return "outside_retained_window";
    case result::arithmetic_overflow: return "arithmetic_overflow";
    }
    return "unknown";
  }

  const char *epose_v2_relay_ingress_status_name(
      const qwertycoin::epose::relay_ingress_status_v2 status)
  {
    switch (status)
    {
    case qwertycoin::epose::relay_ingress_status_v2::accepted:
      return "accepted";
    case qwertycoin::epose::relay_ingress_status_v2::invalid_configuration:
      return "invalid_configuration";
    case qwertycoin::epose::relay_ingress_status_v2::invalid_batch:
      return "invalid_batch";
    case qwertycoin::epose::relay_ingress_status_v2::expired:
      return "expired";
    case qwertycoin::epose::relay_ingress_status_v2::capacity_exhausted:
      return "capacity_exhausted";
    case qwertycoin::epose::relay_ingress_status_v2::context_unavailable:
      return "context_unavailable";
    case qwertycoin::epose::relay_ingress_status_v2::outside_retained_window:
      return "outside_retained_window";
    }
    return "unknown";
  }
  uint8_t parse_epose_transport_families_v2(const std::string &value)
  {
    if (value == "v4") return 1;
    if (value == "v6") return 2;
    if (value == "dual") return 3;
    return 0;
  }

  bool epose_outbound_family_reachable_v2(bool ipv6)
  {
    using boost::asio::ip::tcp;
    boost::asio::io_context io;
    tcp::socket socket(io);
    boost::asio::steady_timer timer(io);
    boost::system::error_code error;
    const auto address = boost::asio::ip::make_address(
        ipv6 ? "2606:4700:4700::1111" : "1.1.1.1", error);
    if (error)
      return false;
    bool reachable = false;
    socket.async_connect(tcp::endpoint(address, 443), [&](const boost::system::error_code &result) {
      reachable = !result;
      timer.cancel();
    });
    timer.expires_after(std::chrono::milliseconds(700));
    timer.async_wait([&](const boost::system::error_code &result) {
      if (!result)
        socket.cancel();
    });
    io.run();
    return reachable;
  }

  uint8_t epose_reachable_dial_capabilities_v2(uint8_t configured)
  {
    static std::atomic<bool> fallback_warned{false};
    const uint8_t reached = qwertycoin::epose::reachable_dial_capabilities_v2(
        configured, epose_outbound_family_reachable_v2);
    if (reached != 0 || configured == 0)
      fallback_warned.store(false, std::memory_order_relaxed);
    else if (!fallback_warned.exchange(true, std::memory_order_relaxed))
      MWARNING("EPoSE-v2 outbound reachability probe failed for every configured family; advertising the configured dial families");
    return qwertycoin::epose::dial_capabilities_or_configured_v2(configured, reached);
  }

  bool epose_endpoint_matches_member_v2(
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
      const qwertycoin::epose::frozen_member_v2 &member)
  {
    return member.descriptor_version == 1
        || (endpoint.version == 2
            && static_cast<uint8_t>(endpoint.transport) == member.endpoint_transport
            && endpoint.dial_capabilities == member.dial_capabilities
            && endpoint.dns_address_families == member.dns_address_families);
  }

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

  uint64_t epose_v2_utc_milliseconds()
  {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return elapsed < 0 ? 0 : static_cast<uint64_t>(elapsed);
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
  const command_line::arg_descriptor<bool, false> arg_devnet_on  = {
    "devnet"
  , "Run on the development network where the contracts hard fork is active from a low height. The wallet must be launched with --devnet flag."
  , false
  };
  const command_line::arg_descriptor<bool> arg_regtest_on  = {
    "regtest"
  , "Run in a regression testing mode."
  , false
  };
  const command_line::arg_descriptor<uint64_t> arg_regtest_fork_height = {
    "regtest-fork-height"
  , "Regtest only: activate the contracts hard fork at this height (must be > 0). 0 keeps regtest on the EPoSE hard fork."
  , 0
  };
  const command_line::arg_descriptor<bool> arg_regtest_epose_v2 = {
    "regtest-epose-v2"
  , "Regtest only: enable the EPoSE-v2 service and ChainLock profile"
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
  const command_line::arg_descriptor<std::string, false, true, 4> arg_data_dir = {
    "data-dir"
  , "Specify data directory"
  , tools::get_default_data_dir()
  , {{ &arg_testnet_on, &arg_stagenet_on, &arg_regtest_on, &arg_devnet_on }}
  , [](std::array<bool, 4> nets, bool defaulted, std::string val)->std::string {
      if (nets[0])
        return (boost::filesystem::path(val) / "testnet").string();
      else if (nets[1])
        return (boost::filesystem::path(val) / "stagenet").string();
      else if (nets[2])
        return (boost::filesystem::path(val) / "fake").string();
      else if (nets[3])
        return (boost::filesystem::path(val) / "devnet").string();
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
  , "Retired EPoSE-v1 option; use --epose-v2-service"
  , false
  };
  const command_line::arg_descriptor<std::string> arg_service_node_key = {
    "service-node-key"
  , "Retired EPoSE-v1 option; use --epose-v2-keystore"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_service_reward_address = {
    "service-reward-address"
  , "Retired EPoSE-v1 option; use --epose-v2-reward-address"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_service_reward_view_key = {
    "service-reward-view-key"
  , "Retired EPoSE-v1 option; EPoSE v2 never accepts a wallet private view key"
  , ""
  };
  const command_line::arg_descriptor<std::string> arg_service_node_advertise_address = {
    "service-node-advertise-address"
  , "Retired EPoSE-v1 option; use --epose-v2-endpoint-host and --epose-v2-endpoint-port"
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
  const command_line::arg_descriptor<bool> arg_epose_v2_repair_keystore_permissions = {
    "epose-v2-repair-keystore-permissions"
  , "Restrict only the configured EPoSE-v2 keystore file permissions before loading it (one-time recovery; key contents and parent directory are unchanged)"
  , false
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
  const command_line::arg_descriptor<std::string> arg_epose_v2_dial_families = {
    "epose-v2-dial-families"
  , "Outbound EPoSE verifier dial families: v4, v6, or dual"
  , "v4"
  };
  const command_line::arg_descriptor<std::string> arg_epose_v2_dns_families = {
    "epose-v2-dns-families"
  , "Declared A/AAAA families for a DNS EPoSE endpoint: v4, v6, or dual"
  , "v4"
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
    m_blockchain_storage.m_epose_recovery_intake = [this](const std::function<void()> &operation) {
      CRITICAL_REGION_LOCAL(m_incoming_tx_lock);
      m_blockchain_storage.prepare_handle_incoming_block_no_preprocess(0);
      BOOST_SCOPE_EXIT_ALL(&) {
        if (!m_blockchain_storage.cleanup_handle_incoming_blocks())
          m_blockchain_storage.m_epose_rebuild_fatal = true;
      };
      try
      {
        operation();
      }
      catch (...)
      {
        m_blockchain_storage.m_batch_success = false;
        throw;
      }
    };
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
    m_epose_v2_receipt_cancel.store(true, std::memory_order_relaxed);
    if (m_epose_v2_active_receipt_job.context.attempt_id != 0)
    {
      const uint64_t now_steady_ms = epose_v2_steady_milliseconds();
      qwertycoin::epose::receipt_attempt_terminal_v2 terminal{};
      terminal.context = m_epose_v2_active_receipt_job.context;
      terminal.outcome = qwertycoin::epose::receipt_attempt_outcome_v2::cancelled;
      terminal.reason = qwertycoin::epose::receipt_failure_reason_v2::cancelled;
      terminal.completed_utc_ms = epose_v2_utc_milliseconds();
      terminal.duration_ms = now_steady_ms >= terminal.context.started_steady_ms
          ? now_steady_ms - terminal.context.started_steady_ms : 0;
      const auto effect = m_epose_v2_receipt_diagnostics.terminal(
          terminal, now_steady_ms);
      if (effect.terminal_recorded)
        MDEBUG("event=epose_receipt_attempt_terminal utc_ms="
            << terminal.completed_utc_ms << " attempt_id="
            << terminal.context.attempt_id
            << " epoch=" << terminal.context.epoch
            << " round=" << terminal.context.round
            << " height=" << terminal.context.chain_height
            << " outcome=cancelled stage=none reason=cancelled duration_ms="
            << terminal.duration_ms
            << " retry_scheduled=false next_retry_utc_ms=0");
      m_epose_v2_active_receipt_job = {};
    }
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
    command_line::add_arg(desc, arg_devnet_on);
    command_line::add_arg(desc, arg_regtest_on);
    command_line::add_arg(desc, arg_regtest_fork_height);
    command_line::add_arg(desc, arg_regtest_epose_v2);
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
    command_line::add_arg(desc, arg_epose_v2_repair_keystore_permissions);
    command_line::add_arg(desc, arg_epose_v2_reward_address);
    command_line::add_arg(desc, arg_epose_v2_endpoint_host);
    command_line::add_arg(desc, arg_epose_v2_endpoint_port);
    command_line::add_arg(desc, arg_epose_v2_dial_families);
    command_line::add_arg(desc, arg_epose_v2_dns_families);
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
    const bool devnet = command_line::get_arg(vm, arg_devnet_on);
    const bool regtest = command_line::get_arg(vm, arg_regtest_on);
    if (testnet + stagenet + devnet + regtest > 1)
      throw std::runtime_error("More than one network type argument was specified");
    return testnet ? TESTNET : stagenet ? STAGENET : devnet ? DEVNET : regtest ? FAKECHAIN : MAINNET;
  }
  //-----------------------------------------------------------------------------------------------
  uint64_t core::get_regtest_fork_height_from_args(const boost::program_options::variables_map& vm)
  {
    const uint64_t height = command_line::get_arg(vm, arg_regtest_fork_height);
    if (height != 0 && !command_line::get_arg(vm, arg_regtest_on))
      throw std::runtime_error("--regtest-fork-height requires --regtest");
    return height;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::handle_command_line(const boost::program_options::variables_map& vm)
  {
    if (m_nettype != FAKECHAIN)
    {
      m_nettype = get_network_type_from_args(vm);
    }

    if (vm.count(arg_regtest_fork_height.name) && !command_line::is_arg_defaulted(vm, arg_regtest_fork_height)
        && m_nettype != FAKECHAIN)
    {
      MERROR("--" << arg_regtest_fork_height.name << " requires --" << arg_regtest_on.name);
      return false;
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
    m_epose_v2_repair_keystore_permissions =
        command_line::get_arg(vm, arg_epose_v2_repair_keystore_permissions);
    m_epose_v2_reward_address_string = command_line::get_arg(vm, arg_epose_v2_reward_address);
    m_epose_v2_endpoint_host = command_line::get_arg(vm, arg_epose_v2_endpoint_host);
    m_epose_v2_endpoint_port = command_line::get_arg(vm, arg_epose_v2_endpoint_port);
    m_epose_v2_dial_families = parse_epose_transport_families_v2(
        command_line::get_arg(vm, arg_epose_v2_dial_families));
    m_epose_v2_dns_families = parse_epose_transport_families_v2(
        command_line::get_arg(vm, arg_epose_v2_dns_families));
    if (m_epose_v2_service_enabled
        && (m_epose_v2_dial_families == 0 || m_epose_v2_dns_families == 0))
      return false;
    m_epose_v2_discovery_endpoints = command_line::get_arg(vm, arg_epose_v2_discovery_endpoint);
    if (m_epose_v2_repair_keystore_permissions && !m_epose_v2_service_enabled)
    {
      MERROR("--epose-v2-repair-keystore-permissions requires --epose-v2-service");
      return false;
    }
    if (!config.enabled)
      return true;

    // The inherited HF17-v1 options disclose a reward view secret and create
    // legacy registration objects. They are intentionally not an input to the
    // genesis-native v2 protocol. Keep the switches recognizable so operators
    // receive a deterministic error, but never load keys or construct v1 state.
    MERROR("The legacy --service-node interface is retired for QWC-HF17/EPoSE-v2; use --epose-v2-service and the v2 operator guide in docs/epose/SERVICE_NODE.md");
    return false;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::init_epose_v2_service_runtime()
  {
    m_epose_v2_service_ready = false;
    m_epose_v2_pending_epoch = std::numeric_limits<uint64_t>::max();
    m_epose_v2_pending_envelopes.clear();
    m_epose_v2_identity_deregistered = false;
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
    if (m_epose_v2_repair_keystore_permissions)
    {
      if (!qwertycoin::epose::repair_service_keystore_permissions_v2(
              m_epose_v2_keystore_path, error))
      {
        MERROR("Failed to repair EPoSE-v2 keystore permissions: " << error);
        return false;
      }
      MGINFO("Repaired only the EPoSE-v2 keystore file permissions; key material and parent directory were not changed. Remove --epose-v2-repair-keystore-permissions after this successful start.");
    }
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
    if (!qwertycoin::epose::load_chainlock_signer_state_v1(
            m_epose_v2_keystore_path + ".chainlock",
            m_chainlock_signer_state, error))
    {
      MERROR("Failed to load chainlock signer state: " << error);
      return false;
    }
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
    if (is_qwc_contracts_hardfork(m_blockchain_storage.get_ideal_hard_fork_version(
            m_blockchain_storage.get_current_blockchain_height())))
    {
      endpoint.version = 2;
      endpoint.dial_capabilities = epose_reachable_dial_capabilities_v2(
          m_epose_v2_dial_families);
      endpoint.dns_address_families = endpoint.transport
              == qwertycoin::epose::endpoint_transport_v2::dns
          ? m_epose_v2_dns_families : 0;
    }
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
    cache_epose_v2_endpoint(parameters, endpoint);
    const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
    m_epose_v2_endpoint = endpoint;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::cache_epose_v2_endpoint(
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
    {
      MWARNING("EPoSE-v2 local endpoint was not admitted to the bounded discovery cache");
      return false;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::restore_epose_v2_current_endpoint(
      const qwertycoin::epose::consensus_parameters_v2 &parameters)
  {
    const uint64_t current_epoch =
        m_blockchain_storage.get_epose_current_epoch();
    std::vector<qwertycoin::epose::identity_descriptor_v2> descriptors;
    m_blockchain_storage.get_epose_identity_descriptors_v2(current_epoch, descriptors);
    const auto existing = std::find_if(
        descriptors.begin(), descriptors.end(), [this](const auto &descriptor) {
          return descriptor.identity_id == m_epose_v2_identity_id;
        });
    if (existing == descriptors.end())
      return true;
    if (existing->service_public_key
            != m_epose_v2_keystore.service_public_key
        || existing->reward_address.m_view_public_key
            != m_epose_v2_reward_address.m_view_public_key
        || existing->reward_address.m_spend_public_key
            != m_epose_v2_reward_address.m_spend_public_key)
    {
      MERROR("EPoSE-v2 current descriptor is bound to unexpected local identity material");
      return false;
    }

    qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
    if (!build_epose_v2_configured_endpoint(
            parameters, existing->sequence, existing->expiry_epoch, endpoint))
      return false;
    const crypto::hash endpoint_hash =
        qwertycoin::epose::hash_endpoint_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, endpoint);
    if (endpoint_hash != existing->endpoint_descriptor_hash)
    {
      if (epose_warning_once_per_epoch(
              m_epose_v2_last_current_endpoint_warning_epoch, current_epoch))
        MWARNING("EPoSE-v2 configured endpoint commitment does not match the current descriptor for epoch "
            << current_epoch);
      return true;
    }
    return cache_epose_v2_endpoint(parameters, endpoint)
        && relay_local_epose_v2_endpoint(parameters, endpoint, true);
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
      return epose_v2_producer_step_failed("consensus_parameters_unavailable", m_epose_v2_step_failed_count);
    qwertycoin::epose::lifecycle_record_v2 latest_record{};
    const bool has_history =
        m_blockchain_storage.get_epose_latest_lifecycle_record_v2(
            m_epose_v2_identity_id, latest_record);
    if (has_history
        && latest_record.action
            == qwertycoin::epose::lifecycle_action_v2::deregister_identity)
    {
      if (!m_epose_v2_identity_deregistered)
        MERROR("EPoSE-v2 local identity is deregistered; enrollment is terminal");
      m_epose_v2_identity_deregistered = true;
      m_epose_v2_producer_cancel.store(true, std::memory_order_relaxed);
      m_epose_v2_pending_envelopes.clear();
      return true;
    }
    m_epose_v2_identity_deregistered = false;
    if (has_history
        && latest_record.next_descriptor.service_public_key
            != m_epose_v2_keystore.service_public_key)
    {
      MERROR("EPoSE-v2 local identity is bound to an unexpected service key");
      return false;
    }
    if (has_history
        && (latest_record.next_descriptor.reward_address.m_view_public_key
                != m_epose_v2_reward_address.m_view_public_key
            || latest_record.next_descriptor.reward_address.m_spend_public_key
                != m_epose_v2_reward_address.m_spend_public_key))
    {
      MERROR("EPoSE-v2 configured reward address does not match the canonical descriptor; a frozen reward is never redirected silently");
      return false;
    }
    if (!restore_epose_v2_current_endpoint(parameters))
      return epose_v2_producer_step_failed("current_endpoint_restore_failed", m_epose_v2_step_failed_count);

    uint64_t target_epoch = m_blockchain_storage.get_epose_current_epoch() + 1;
    uint64_t cutoff = 0;
    if (!parameters.timing.enrollment_cutoff(target_epoch, cutoff))
      return epose_v2_producer_step_failed("enrollment_cutoff_unavailable", m_epose_v2_step_failed_count);
    if (chain_height >= cutoff)
    {
      if (target_epoch == std::numeric_limits<uint64_t>::max())
        return epose_v2_producer_step_failed("target_epoch_overflow", m_epose_v2_step_failed_count);
      ++target_epoch;
    }

    const std::string pending_path = (boost::filesystem::path(m_config_folder)
        / "epose-v2-pending-enrollment.bin").string();
    const auto discard_pending = [&] {
      boost::system::error_code error;
      boost::filesystem::remove(pending_path, error);
    };
    std::vector<qwertycoin::epose::identity_descriptor_v2> descriptors;
    m_blockchain_storage.get_epose_identity_descriptors_v2(target_epoch, descriptors);
    const auto existing = std::find_if(
        descriptors.begin(), descriptors.end(), [this](const auto &descriptor) {
          return descriptor.identity_id == m_epose_v2_identity_id;
        });
    if (has_history
        && latest_record.next_descriptor.effective_epoch > target_epoch)
      return epose_v2_producer_step_failed("descriptor_ahead_of_target", m_epose_v2_step_failed_count);
    if (existing != descriptors.end())
    {
      qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
      if (!build_epose_v2_configured_endpoint(
              parameters, existing->sequence, existing->expiry_epoch,
              endpoint))
        return epose_v2_producer_step_failed("endpoint_rebuild_failed", m_epose_v2_step_failed_count);
      if (qwertycoin::epose::hash_endpoint_descriptor_v2(
              parameters.nettype, parameters.genesis_hash,
              parameters.parameter_set_hash, endpoint)
          == existing->endpoint_descriptor_hash)
      {
        remember_epose_v2_endpoint(parameters, endpoint);
        if (!relay_local_epose_v2_endpoint(parameters, endpoint))
          return epose_v2_producer_step_failed("endpoint_relay_failed", m_epose_v2_step_failed_count);
      }
      else if (epose_warning_once_per_epoch(
                   m_epose_v2_last_target_endpoint_warning_epoch, target_epoch))
        MWARNING("EPoSE-v2 configured endpoint commitment does not match the target descriptor for epoch "
            << target_epoch);
      if (m_blockchain_storage.has_epose_admission_v2(
              m_epose_v2_identity_id, target_epoch))
      {
        m_epose_v2_producer_cancel.store(true, std::memory_order_relaxed);
        m_epose_v2_pending_envelopes.clear();
        discard_pending();
        m_epose_v2_pending_epoch = std::numeric_limits<uint64_t>::max();
        return true;
      }
    }

    uint64_t context_height = 0;
    if (target_epoch < parameters.admission.context_epoch_offset
        || !parameters.timing.epoch_start(
            target_epoch - parameters.admission.context_epoch_offset,
            context_height))
      return epose_v2_producer_step_failed("context_height_unavailable", m_epose_v2_step_failed_count);
    if (chain_height <= context_height)
      return true;
    const crypto::hash context_hash =
        m_blockchain_storage.get_block_id_by_height(context_height);
    if (context_hash == crypto::null_hash)
      return true;
    const crypto::hash descriptor_hash = has_history
        ? qwertycoin::epose::hash_identity_descriptor_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, latest_record.next_descriptor)
        : crypto::null_hash;

    if (!qwertycoin::epose::service_enrollment_inputs_match_v2(
            target_epoch, descriptor_hash, context_hash,
            m_epose_v2_pending_epoch, m_epose_v2_pending_descriptor_hash,
            m_epose_v2_pending_context_hash))
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
      m_epose_v2_pending_descriptor_hash = descriptor_hash;
      m_epose_v2_pending_context_hash = context_hash;
      m_epose_v2_producer_cancel.store(false, std::memory_order_relaxed);
    }

    if (m_epose_v2_pending_envelopes.empty() && !m_epose_v2_producer_future.valid())
    {
      std::vector<qwertycoin::epose::envelope_record_v2> restored;
      if (qwertycoin::epose::load_pending_enrollment_v2(pending_path, parameters,
              m_epose_v2_identity_id, target_epoch, chain_height, context_hash, restored))
      {
        std::vector<blobdata> envelopes;
        for (const auto &record : restored)
        {
          blobdata envelope;
          qwertycoin::epose::envelope_budget_v2 budget{};
          if (qwertycoin::epose::encode_envelope_v2({record},
                  parameters.limits.envelope, envelope, budget)
              != qwertycoin::epose::envelope_status_v2::accepted)
            return false;
          envelopes.push_back(std::move(envelope));
        }
        // Authenticate against current canonical state, including predecessor,
        // descriptor binding, cutoff and reorg context, before any P2P relay.
        std::vector<blobdata> accepted;
        const auto status = admit_local_epose_envelopes_v2(
            envelopes, accepted, epose_local_ingress_origin::enrollment);
        if (status == qwertycoin::epose::relay_ingress_status_v2::capacity_exhausted
            || status == qwertycoin::epose::relay_ingress_status_v2::context_unavailable
            || status == qwertycoin::epose::relay_ingress_status_v2::invalid_configuration)
          return true; // Keep solved work on disk and retry when capacity returns.
        if (status == qwertycoin::epose::relay_ingress_status_v2::accepted)
        {
          m_epose_v2_pending_envelopes = std::move(envelopes);
          m_epose_v2_last_submission = {};
        }
        else
          discard_pending();
      }
      else
        discard_pending();
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
          m_epose_v2_pending_epoch = std::numeric_limits<uint64_t>::max();
          const auto action = epose_enrollment_failure_result(
              result.first,
              is_qwc_contracts_hardfork(
                  m_blockchain_storage.get_ideal_hard_fork_version(chain_height)),
              m_epose_v2_last_enrollment_warning_epoch, target_epoch,
              m_epose_v2_step_failed_count);
          if (action != epose_enrollment_failure_action::error)
          {
            if (action == epose_enrollment_failure_action::retry_with_warning)
              MWARNING("EPoSE-v2 enrollment endpoint is invalid for epoch " << target_epoch);
            return true;
          }
          MERROR("Failed to build bounded EPoSE-v2 enrollment: status "
              << static_cast<unsigned>(result.first));
          return false;
        }
        if (result.second.endpoint_update.present
            && is_qwc_contracts_hardfork(m_blockchain_storage.get_ideal_hard_fork_version(
                chain_height)))
          remember_epose_v2_endpoint(parameters, result.second.endpoint_update.descriptor);
        std::vector<blobdata> encoded;
        encoded.reserve(result.second.records.size());
        for (const auto &record : result.second.records)
        {
          qwertycoin::epose::envelope_budget_v2 budget{};
          blobdata envelope;
          if (qwertycoin::epose::encode_envelope_v2(
                  {record}, parameters.limits.envelope, envelope, budget)
              != qwertycoin::epose::envelope_status_v2::accepted)
            return epose_v2_producer_step_failed("envelope_encoding_failed", m_epose_v2_step_failed_count);
          encoded.push_back(std::move(envelope));
        }
        m_epose_v2_pending_envelopes = std::move(encoded);
      }
      else
      {
        const auto now = std::chrono::steady_clock::now();
        if (m_epose_v2_last_submission.time_since_epoch().count() != 0
            && now - m_epose_v2_last_submission < epose_v2_attempt_interval())
          return true;
        qwertycoin::epose::service_enrollment_config_v2 configuration{};
        configuration.keystore = m_epose_v2_keystore;
        configuration.reward_address = m_epose_v2_reward_address;
        {
          const std::lock_guard<std::mutex> lock(m_epose_v2_endpoint_mutex);
          configuration.endpoint_transport = m_epose_v2_endpoint.transport;
          configuration.endpoint_host = m_epose_v2_endpoint.host;
          configuration.endpoint_port = m_epose_v2_endpoint.port;
          configuration.endpoint_version = m_epose_v2_endpoint.version;
          configuration.dial_capabilities = m_epose_v2_endpoint.dial_capabilities;
          configuration.dns_address_families = m_epose_v2_endpoint.dns_address_families;
        }
        if (is_qwc_contracts_hardfork(m_blockchain_storage.get_ideal_hard_fork_version(
                chain_height)))
        {
          configuration.endpoint_version = 2;
          configuration.dial_capabilities = epose_reachable_dial_capabilities_v2(
              m_epose_v2_dial_families);
          configuration.dns_address_families = configuration.endpoint_transport
                  == qwertycoin::epose::endpoint_transport_v2::dns
              ? m_epose_v2_dns_families : 0;
        }
        configuration.target_epoch = target_epoch;
        if (target_epoch > std::numeric_limits<uint64_t>::max() - 2)
          return epose_v2_producer_step_failed("expiry_epoch_overflow", m_epose_v2_step_failed_count);
        configuration.expiry_epoch = target_epoch + 2;
        m_epose_v2_producer_cancel.store(false, std::memory_order_relaxed);
        m_epose_v2_producer_future = std::async(
            std::launch::async,
            [parameters, configuration, context_hash, has_history,
             latest_record, pending_path, this]() mutable {
              const crypto::rx_thread_state_guard randomx_state;
              qwertycoin::epose::service_enrollment_v2 enrollment{};
              const auto status = qwertycoin::epose::build_service_enrollment_v2(
                  parameters, configuration,
                  has_history ? &latest_record : nullptr, context_hash,
                  qwertycoin::epose::EPOSE_V2_LAUNCH_ADMISSION_MAX_NONCE_ATTEMPTS,
                  enrollment, &m_epose_v2_producer_cancel);
              if (status == qwertycoin::epose::service_producer_status_v2::accepted
                  && !qwertycoin::epose::save_pending_enrollment_v2(
                      pending_path, parameters, enrollment.records))
                MERROR("Failed to persist completed EPoSE-v2 admission work");
              return std::make_pair(status, std::move(enrollment));
            });
        MGINFO("Started bounded EPoSE-v2 RandomX enrollment search for epoch "
            << target_epoch);
        return true;
      }
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_epose_v2_last_submission.time_since_epoch().count() != 0
        && now - m_epose_v2_last_submission < epose_v2_attempt_interval())
      return true;
    bool newly_accepted = false;
    bool relayed = false;
    bool save_failed = false;
    const auto status = qwertycoin::epose::redeliver_pending_enrollment_v2(
        pending_path, parameters, m_epose_v2_pending_envelopes, chain_height,
        m_blockchain_storage.has_epose_admission_v2(m_epose_v2_identity_id, target_epoch),
        [&](const std::vector<blobdata> &envelopes, std::vector<blobdata> &accepted) {
          return admit_local_epose_envelopes_v2(
              envelopes, accepted, epose_local_ingress_origin::enrollment);
        },
        [&](const std::vector<blobdata> &envelopes) {
          NOTIFY_NEW_EPOSE_ENVELOPES_V2::request request{};
          request.envelopes = envelopes;
          relayed = get_protocol()->relay_epose_envelopes_v2(
              request, boost::uuids::nil_uuid(), epee::net_utils::zone::public_);
        }, save_failed, newly_accepted);
    if (save_failed)
      MERROR("Failed to persist completed EPoSE-v2 admission work; continuing delivery");
    if (status != qwertycoin::epose::relay_ingress_status_v2::accepted)
    {
      m_epose_v2_last_submission = now;
      if (status == qwertycoin::epose::relay_ingress_status_v2::invalid_batch
          || status == qwertycoin::epose::relay_ingress_status_v2::expired)
      {
        discard_pending();
        m_epose_v2_pending_envelopes.clear();
      }
      return true;
    }
    m_epose_v2_last_submission = now;
    if (newly_accepted)
      MGINFO("Submitted EPoSE-v2 lifecycle and admission for epoch "
          << target_epoch << (relayed ? " and relayed it" : " locally"));
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  std::chrono::seconds core::epose_v2_attempt_interval() const
  {
    return std::chrono::seconds(m_nettype == DEVNET ? 1 : 30);
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
      epose_v2_receipt_job_result result{};
      try
      {
        result = m_epose_v2_receipt_future.get();
      }
      catch (...)
      {
        result = m_epose_v2_active_receipt_job;
        result.stage = qwertycoin::epose::receipt_attempt_stage_v2::none;
        result.reason = qwertycoin::epose::receipt_failure_reason_v2::internal_execution_failure;
      }
      m_epose_v2_active_receipt_job = {};
      m_epose_v2_last_receipt_attempt = std::chrono::steady_clock::now();
      const auto finish_attempt = [this](
          const epose_v2_receipt_job_result &finished,
          const qwertycoin::epose::receipt_attempt_outcome_v2 outcome,
          const qwertycoin::epose::receipt_attempt_stage_v2 stage,
          const qwertycoin::epose::receipt_failure_reason_v2 reason,
          const bool local_accepted,
          const bool relayed) {
        const uint64_t now_steady_ms = epose_v2_steady_milliseconds();
        const uint64_t now_utc_ms = epose_v2_utc_milliseconds();
        const auto retry = m_epose_v2_receipt_retries.status(finished.slot);
        qwertycoin::epose::receipt_attempt_terminal_v2 terminal{};
        terminal.context = finished.context;
        terminal.outcome = outcome;
        terminal.stage = stage;
        terminal.reason = reason;
        terminal.completed_utc_ms = now_utc_ms;
        terminal.duration_ms = now_steady_ms >= finished.context.started_steady_ms
            ? now_steady_ms - finished.context.started_steady_ms : 0;
        terminal.retry_scheduled = retry.found
            && retry.next_attempt_ms > now_steady_ms;
        if (terminal.retry_scheduled)
          terminal.next_retry_utc_ms = now_utc_ms
              + (retry.next_attempt_ms - now_steady_ms);
        terminal.local_submission_accepted = local_accepted;
        terminal.local_submission_relayed = relayed;
        const auto effect = m_epose_v2_receipt_diagnostics.terminal(
            terminal, now_steady_ms);
        if (!effect.terminal_recorded)
          return;
        std::ostringstream terminal_event;
        terminal_event << "event=epose_receipt_attempt_terminal utc_ms="
            << terminal.completed_utc_ms << " attempt_id="
            << terminal.context.attempt_id
            << " epoch=" << terminal.context.epoch
            << " round=" << terminal.context.round
            << " height=" << terminal.context.chain_height
            << " outcome=" << qwertycoin::epose::to_string(outcome)
            << " stage=" << qwertycoin::epose::to_string(stage)
            << " reason=" << qwertycoin::epose::to_string(reason)
            << " duration_ms=" << terminal.duration_ms
            << " retry_scheduled=" << terminal.retry_scheduled
            << " next_retry_utc_ms=" << terminal.next_retry_utc_ms;
        if (reason
            == qwertycoin::epose::receipt_failure_reason_v2::internal_execution_failure)
          MERROR(terminal_event.str());
        else
          MDEBUG(terminal_event.str());
        if (effect.emit_repeated_warning)
          MWARNING("event=epose_receipt_repeated_failure utc_ms="
              << now_utc_ms << " epoch="
              << terminal.context.epoch
              << " round=" << terminal.context.round
              << " stage=" << qwertycoin::epose::to_string(stage)
              << " reason=" << qwertycoin::epose::to_string(reason)
              << " suppressed=" << effect.suppressed_since_last_warning);
        if (effect.recovered_after_failures != 0)
          MGINFO("event=epose_receipt_recovered utc_ms=" << now_utc_ms
              << " attempt_id="
              << terminal.context.attempt_id
              << " epoch=" << terminal.context.epoch
              << " round=" << terminal.context.round
              << " previous_failures=" << effect.recovered_after_failures);
      };
      if (!result.accepted)
      {
        m_epose_v2_receipt_retries.failed(
            result.slot, epose_v2_steady_milliseconds());
        const auto outcome = result.reason
                == qwertycoin::epose::receipt_failure_reason_v2::cancelled
            ? qwertycoin::epose::receipt_attempt_outcome_v2::cancelled
            : qwertycoin::epose::receipt_attempt_outcome_v2::failure;
        finish_attempt(result, outcome, result.stage, result.reason, false, false);
        return true;
      }
      bool newly_accepted = false;
      bool relayed = false;
      bool ingress_expired = false;
      if (!submit_local_epose_envelopes_v2(
              std::vector<blobdata>{result.envelope}, newly_accepted, relayed,
              epose_local_ingress_origin::receipt, &ingress_expired))
      {
        m_epose_v2_receipt_retries.failed(
            result.slot, epose_v2_steady_milliseconds());
        const bool expired = ingress_expired
            || m_blockchain_storage.get_current_blockchain_height()
                > result.context.deadline_height;
        finish_attempt(result,
            expired
                ? qwertycoin::epose::receipt_attempt_outcome_v2::expired
                : qwertycoin::epose::receipt_attempt_outcome_v2::failure,
            qwertycoin::epose::receipt_attempt_stage_v2::local_submission,
            expired
                ? qwertycoin::epose::receipt_failure_reason_v2::deadline_expired
                : qwertycoin::epose::receipt_failure_reason_v2::local_submission_rejected,
            false, false);
        return ingress_expired;
      }
      const bool relay_hardening_active =
          m_blockchain_storage.get_current_blockchain_height()
              >= qwertycoin::epose::relay_hardening_height_v2(m_nettype);
      // Preserve the reviewed pre-activation behavior. Before A, a new signed
      // variant may be sent directly when the local complete-record queue
      // reports a duplicate. At and after A, retries retain and redeliver the
      // exact authenticated envelope under the bounded transport policy.
      if (!relay_hardening_active && !newly_accepted)
      {
        NOTIFY_NEW_EPOSE_ENVELOPES_V2::request request{};
        request.envelopes.push_back(result.envelope);
        relayed = get_protocol()->relay_epose_envelopes_v2(
            request, boost::uuids::nil_uuid(),
            epee::net_utils::zone::public_);
      }
      m_epose_v2_receipt_retries.submitted(
          result.slot, epose_v2_steady_milliseconds(),
          relay_hardening_active ? result.envelope : std::string{});
      finish_attempt(result,
          qwertycoin::epose::receipt_attempt_outcome_v2::success,
          qwertycoin::epose::receipt_attempt_stage_v2::local_submission,
          qwertycoin::epose::receipt_failure_reason_v2::none,
          newly_accepted, relayed);
      if (newly_accepted)
        MGINFO("event=epose_receipt_submitted utc_ms="
            << epose_v2_utc_milliseconds() << " epoch="
            << result.context.epoch << " round=" << result.context.round
            << " attempt_id=" << result.context.attempt_id
            << " local_pool=accepted"
            << " relayed=" << relayed);
      return true;
    }

    const auto now = std::chrono::steady_clock::now();
    const uint64_t inclusion_height =
        m_blockchain_storage.get_current_blockchain_height();
    if (!qwertycoin::epose::receipt_launch_allowed_v2(
            m_blockchain_storage.get_ideal_hard_fork_version(inclusion_height),
            m_epose_v2_last_receipt_attempt.time_since_epoch().count() != 0,
            now - m_epose_v2_last_receipt_attempt,
            epose_v2_attempt_interval()))
      return true;
    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
      return false;
    if (inclusion_height < parameters.timing.activation_height)
      return true;
    const uint64_t epoch =
        (inclusion_height - parameters.timing.activation_height)
        / parameters.timing.epoch_length;
    if (m_epose_v2_receipt_epoch != epoch)
    {
      if (m_epose_v2_receipt_epoch != std::numeric_limits<uint64_t>::max())
        maybe_log_epose_v2_diagnostic_summary(
            m_epose_v2_receipt_epoch,
            std::numeric_limits<uint64_t>::max(), inclusion_height, true);
      m_epose_v2_receipt_epoch = epoch;
    }

    qwertycoin::epose::membership_snapshot_v2 snapshot{};
    if (m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot)
        != Blockchain::epose_query_status::found)
      return true;
    const auto local_member = std::find_if(
        snapshot.members.begin(), snapshot.members.end(), [this](const auto &member) {
          return member.service_public_key
              == m_epose_v2_keystore.service_public_key;
        });
    if (local_member == snapshot.members.end())
    {
      m_epose_v2_receipt_diagnostics.skipped(
          qwertycoin::epose::receipt_scheduler_skip_v2::not_local_member);
      return true;
    }
    const uint8_t verifier_dial_capabilities = snapshot.transport_aware
        ? local_member->dial_capabilities : 0;

    uint64_t epoch_start = 0;
    uint64_t evidence_deadline = 0;
    if (!parameters.timing.epoch_start(epoch, epoch_start)
        || !parameters.timing.evidence_deadline(epoch, evidence_deadline))
      return true;
    if (inclusion_height > evidence_deadline)
    {
      maybe_log_epose_v2_diagnostic_summary(
          epoch, std::numeric_limits<uint64_t>::max(), inclusion_height, false);
      return true;
    }

    uint64_t active_round = std::numeric_limits<uint64_t>::max();
    uint64_t active_round_deadline = evidence_deadline;
    for (uint64_t round = 0; round < parameters.committee.round_offsets.size(); ++round)
    {
      uint64_t first = 0;
      uint64_t last = 0;
      if (!qwertycoin::epose::receipt_inclusion_window_v2(
              parameters.timing, parameters.committee, epoch, round,
              first, last))
        return false;
      if (inclusion_height >= first && inclusion_height <= last)
      {
        active_round = round;
        active_round_deadline = last;
        break;
      }
    }
    if (active_round == std::numeric_limits<uint64_t>::max())
    {
      m_epose_v2_receipt_diagnostics.skipped(
          qwertycoin::epose::receipt_scheduler_skip_v2::no_active_round);
      // The transition from an active round into a gap is the round-end
      // summary. Subsequent gap blocks remain subject to the periodic limit.
      maybe_log_epose_v2_diagnostic_summary(
          epoch, std::numeric_limits<uint64_t>::max(), inclusion_height, false);
      return true;
    }
    maybe_log_epose_v2_diagnostic_summary(
        epoch, active_round, inclusion_height, false);

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
      {
        m_epose_v2_receipt_diagnostics.skipped(
            qwertycoin::epose::receipt_scheduler_skip_v2::not_committee_member);
        continue;
      }
      if (m_blockchain_storage.has_epose_receipt_slot_v2(
              epoch, active_round, subject.service_public_key,
              m_epose_v2_keystore.service_public_key))
      {
        const crypto::hash canonical_slot = epose_v2_receipt_slot_hash(
            epoch, active_round, subject.service_public_key,
            m_epose_v2_keystore.service_public_key, anchor_hash);
        m_epose_v2_receipt_retries.canonical(canonical_slot);
        uint64_t canonical_attempt_id = 0;
        if (m_epose_v2_receipt_diagnostics.canonical_inclusion(
                epoch, active_round, canonical_slot, &canonical_attempt_id))
          MGINFO("event=epose_receipt_canonical_inclusion utc_ms="
              << epose_v2_utc_milliseconds() << " epoch=" << epoch
              << " round=" << active_round << " attempt_id="
              << (canonical_attempt_id ? std::to_string(canonical_attempt_id) : "unknown")
              << " height=" << inclusion_height
              << " stage=canonical_inclusion outcome=success");
        m_epose_v2_receipt_diagnostics.skipped(
            qwertycoin::epose::receipt_scheduler_skip_v2::already_canonical);
        continue;
      }
      const crypto::hash slot = epose_v2_receipt_slot_hash(
          epoch, active_round, subject.service_public_key,
          m_epose_v2_keystore.service_public_key, anchor_hash);
      const uint64_t now_ms = epose_v2_steady_milliseconds();
      const auto retry = m_epose_v2_receipt_retries.status(slot);
      if (inclusion_height >= qwertycoin::epose::relay_hardening_height_v2(m_nettype)
          && retry.found && retry.has_envelope)
      {
        std::string envelope;
        if (m_epose_v2_receipt_retries.transport_retry(
                slot, now_ms, envelope))
        {
          NOTIFY_NEW_EPOSE_ENVELOPES_V2::request request{};
          request.envelopes.push_back(std::move(envelope));
          const bool resent = get_protocol()->relay_epose_envelopes_v2(
              request, boost::uuids::nil_uuid(),
              epee::net_utils::zone::public_);
          m_epose_v2_receipt_diagnostics.transport_retry(
              epoch, active_round, resent);
          MDEBUG("event=epose_receipt_transport_retry utc_ms="
              << epose_v2_utc_milliseconds() << " epoch=" << epoch
              << " round=" << active_round
              << " height=" << inclusion_height
              << " attempt=" << (retry.transport_attempts + 1)
              << " relayed=" << resent
              << " canonical_inclusion=false");
        }
        continue;
      }
      if (!m_epose_v2_receipt_retries.can_attempt(slot, now_ms))
      {
        m_epose_v2_receipt_diagnostics.skipped(
            qwertycoin::epose::receipt_scheduler_skip_v2::retry_backoff);
        continue;
      }

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
      qwertycoin::epose::receipt_attempt_context_v2 attempt{};
      attempt.attempt_id = m_epose_v2_receipt_diagnostics.next_attempt_id();
      attempt.epoch = epoch;
      attempt.round = active_round;
      attempt.chain_height = inclusion_height;
      attempt.deadline_height = active_round_deadline;
      attempt.slot = slot;
      attempt.subject_identity = subject.service_public_key;
      attempt.verifier_identity = m_epose_v2_keystore.service_public_key;
      attempt.endpoint_commitment = subject.endpoint_descriptor_hash;
      attempt.started_utc_ms = epose_v2_utc_milliseconds();
      attempt.started_steady_ms = now_ms;
      if (!m_epose_v2_receipt_diagnostics.started(attempt))
      {
        m_epose_v2_receipt_retries.failed(slot, now_ms);
        MERROR("event=epose_receipt_attempt_internal_failure utc_ms="
            << epose_v2_utc_milliseconds() << " attempt_id="
            << attempt.attempt_id
            << " stage=diagnostic_start outcome=failure reason=internal_execution_failure");
        return false;
      }
      m_epose_v2_active_receipt_job = {};
      m_epose_v2_active_receipt_job.slot = slot;
      m_epose_v2_active_receipt_job.context = attempt;
      m_epose_v2_receipt_cancel.store(false, std::memory_order_relaxed);
      m_epose_v2_last_receipt_attempt = now;
      try
      {
        m_epose_v2_receipt_future = std::async(
            std::launch::async,
            [parameters, challenge, context, discovery, service_secret, subject,
             verifier_dial_capabilities,
             cached_endpoint, cached_endpoint_found, anchor_blob, slot, attempt,
             cancel = &m_epose_v2_receipt_cancel]() mutable {
            const crypto::rx_thread_state_guard randomx_state;
            epose_v2_receipt_job_result result{};
            result.slot = slot;
            result.context = attempt;
            qwertycoin::epose::endpoint_descriptor_v2 endpoint = cached_endpoint;
            qwertycoin::epose::canonical_service_response_v2 response{};
            qwertycoin::epose::authenticated_service_receipt_v2 receipt{};
            qwertycoin::epose::envelope_record_v2 record{};
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
            using qwertycoin::epose::receipt_attempt_stage_v2;
            using qwertycoin::epose::receipt_failure_reason_v2;
            std::vector<qwertycoin::epose::receipt_attempt_step_v2> steps;
            steps.emplace_back([&]() {
              if (cached_endpoint_found)
              {
                if (endpoint.service_public_key != challenge.subject_public_key)
                  return qwertycoin::epose::receipt_step_result_v2{
                      receipt_attempt_stage_v2::descriptor_validation,
                      receipt_failure_reason_v2::response_identity_mismatch};
                if (qwertycoin::epose::hash_endpoint_descriptor_v2(
                        parameters.nettype, parameters.genesis_hash,
                        parameters.parameter_set_hash, endpoint)
                    != challenge.endpoint_descriptor_hash)
                  return qwertycoin::epose::receipt_step_result_v2{
                      receipt_attempt_stage_v2::descriptor_validation,
                      receipt_failure_reason_v2::descriptor_commitment_mismatch};
                return qwertycoin::epose::receipt_step_result_v2{};
              }
              qwertycoin::epose::receipt_step_result_v2 best{
                  receipt_attempt_stage_v2::descriptor_lookup,
                  receipt_failure_reason_v2::descriptor_unavailable};
              for (const std::string &url : discovery)
              {
                qwertycoin::epose::endpoint_descriptor_v2 candidate{};
                const auto fetched = fetch_epose_endpoint_descriptor(
                    url, parameters, challenge.endpoint_descriptor_hash,
                    candidate);
                if (!fetched.accepted())
                {
                  if (fetched.reason
                      != receipt_failure_reason_v2::descriptor_unavailable)
                    best = fetched;
                  continue;
                }
                if (candidate.service_public_key != challenge.subject_public_key)
                {
                  best = {receipt_attempt_stage_v2::descriptor_validation,
                      receipt_failure_reason_v2::response_identity_mismatch};
                  continue;
                }
                endpoint = std::move(candidate);
                return qwertycoin::epose::receipt_step_result_v2{};
              }
              return best;
            });
            steps.emplace_back([&]() {
              return epose_endpoint_matches_member_v2(endpoint, subject)
                  ? qwertycoin::epose::receipt_step_result_v2{}
                  : qwertycoin::epose::receipt_step_result_v2{
                      receipt_attempt_stage_v2::descriptor_validation,
                      receipt_failure_reason_v2::descriptor_invalid};
            });
            steps.emplace_back([&]() {
              return request_epose_service_response(
                  challenge, endpoint, parameters, response, {}, verifier_dial_capabilities);
            });
            steps.emplace_back([&]() {
              const auto status =
                  qwertycoin::epose::verify_canonical_block_response_v2(
                      challenge, response, context,
                      {QWC_EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES},
                      service_secret, authorize, source, receipt);
              return qwertycoin::epose::receipt_step_result_v2{
                  receipt_attempt_stage_v2::response_validation,
                  qwertycoin::epose::map_canonical_service_failure_v2(status)};
            });
            steps.emplace_back([&]() {
              const auto status =
                  qwertycoin::epose::encode_service_receipt_record_v2(
                      receipt, context, record);
              return qwertycoin::epose::receipt_step_result_v2{
                  receipt_attempt_stage_v2::receipt_encoding,
                  status == qwertycoin::epose::record_codec_status_v2::accepted
                      ? receipt_failure_reason_v2::none
                      : receipt_failure_reason_v2::receipt_encoding_failed};
            });
            steps.emplace_back([&]() {
              qwertycoin::epose::envelope_budget_v2 budget{};
              const auto status = qwertycoin::epose::encode_envelope_v2(
                  {record}, parameters.limits.envelope,
                  result.envelope, budget);
              if (status != qwertycoin::epose::envelope_status_v2::accepted)
                result.envelope.clear();
              return qwertycoin::epose::receipt_step_result_v2{
                  receipt_attempt_stage_v2::envelope_encoding,
                  status == qwertycoin::epose::envelope_status_v2::accepted
                      ? receipt_failure_reason_v2::none
                      : receipt_failure_reason_v2::envelope_encoding_failed};
            });
            const auto attempt_result =
                qwertycoin::epose::run_receipt_attempt_steps_v2(
                    steps, [cancel]() {
                      return cancel->load(std::memory_order_relaxed);
                    });
            result.stage = attempt_result.stage;
            result.reason = attempt_result.reason;
            if (!attempt_result.accepted())
              return result;
            result.accepted = true;
            return result;
            });
      }
      catch (...)
      {
        m_epose_v2_receipt_retries.failed(slot, epose_v2_steady_milliseconds());
        qwertycoin::epose::receipt_attempt_terminal_v2 terminal{};
        terminal.context = attempt;
        terminal.outcome = qwertycoin::epose::receipt_attempt_outcome_v2::failure;
        terminal.reason = qwertycoin::epose::receipt_failure_reason_v2::internal_execution_failure;
        terminal.completed_utc_ms = epose_v2_utc_milliseconds();
        terminal.duration_ms = epose_v2_steady_milliseconds() >= attempt.started_steady_ms
            ? epose_v2_steady_milliseconds() - attempt.started_steady_ms : 0;
        m_epose_v2_receipt_diagnostics.terminal(
            terminal, epose_v2_steady_milliseconds());
        m_epose_v2_active_receipt_job = {};
        MERROR("event=epose_receipt_attempt_internal_failure utc_ms="
            << terminal.completed_utc_ms << " attempt_id="
            << attempt.attempt_id
            << " stage=async_launch outcome=failure reason=internal_execution_failure");
        return false;
      }
      MDEBUG("event=epose_receipt_attempt_started utc_ms="
          << attempt.started_utc_ms << " attempt_id="
          << attempt.attempt_id << " epoch=" << epoch
          << " round=" << active_round << " height=" << inclusion_height
          << " subject=" << epee::string_tools::pod_to_hex(attempt.subject_identity)
          << " verifier=" << epee::string_tools::pod_to_hex(attempt.verifier_identity)
          << " endpoint_commitment="
          << epee::string_tools::pod_to_hex(attempt.endpoint_commitment)
          << " deadline_height=" << attempt.deadline_height
          << " stage=descriptor_lookup outcome=started");
      return true;
    }
    return true;
  }
  //-----------------------------------------------------------------------------------------------
  void core::maybe_log_epose_v2_diagnostic_summary(
      const uint64_t epoch,
      const uint64_t round,
      const uint64_t height,
      const bool force)
  {
    if (m_epose_v2_last_diagnostic_summary_height == height
        && m_epose_v2_last_diagnostic_summary_epoch == epoch
        && m_epose_v2_last_diagnostic_summary_round == round)
      return;
    const bool boundary = m_epose_v2_last_diagnostic_summary_epoch != epoch
        || m_epose_v2_last_diagnostic_summary_round != round;
    const bool periodic = m_epose_v2_last_diagnostic_summary_height
            == std::numeric_limits<uint64_t>::max()
        || height >= m_epose_v2_last_diagnostic_summary_height + 60;
    if (!force && !boundary && !periodic)
      return;

    qwertycoin::epose::receipt_diagnostics_snapshot_v2 operations{};
    qwertycoin::epose::qualification_diagnostics_v2 qualification{};
    if (!get_epose_v2_diagnostics(operations, qualification, epoch))
      return;

    uint64_t started = 0;
    uint64_t completed = 0;
    uint64_t succeeded = 0;
    uint64_t failed = 0;
    uint64_t cancelled = 0;
    uint64_t expired = 0;
    uint64_t submitted = 0;
    uint64_t canonical = 0;
    std::ostringstream rounds;
    for (const auto &entry : operations.rounds)
    {
      if (entry.epoch != epoch)
        continue;
      started += entry.attempts_started;
      completed += entry.attempts_completed;
      succeeded += entry.attempts_succeeded;
      failed += entry.attempts_failed;
      cancelled += entry.attempts_cancelled;
      expired += entry.attempts_expired;
      submitted += entry.local_submissions_accepted;
      canonical += entry.canonical_inclusions_observed;
      if (rounds.tellp() != std::streampos(0))
        rounds << ',';
      rounds << entry.round << ':' << entry.attempts_started << '/'
          << entry.attempts_completed << '/' << entry.attempts_failed << '/'
          << entry.attempts_cancelled << '/' << entry.attempts_expired;
    }

    std::ostringstream failures;
    for (const auto &entry : operations.failures)
    {
      if (entry.epoch != epoch)
        continue;
      if (failures.tellp() != std::streampos(0))
        failures << ',';
      failures << entry.round << ':'
          << qwertycoin::epose::to_string(entry.stage) << ':'
          << qwertycoin::epose::to_string(entry.reason) << ':'
          << entry.count;
    }

    std::ostringstream coverage;
    std::ostringstream required_receipts;
    if (!qualification.available)
    {
      coverage << "unknown";
      required_receipts << "unknown";
    }
    else
    {
      for (size_t index = 0;
           index < qualification.canonical_unique_receipts.size(); ++index)
      {
        if (index != 0)
        {
          coverage << ',';
          required_receipts << ',';
        }
        coverage << qualification.canonical_unique_receipts[index] << '/'
            << qualification.committee_sizes[index];
        required_receipts << qualification.required_receipts[index];
      }
    }
    const char *state =
        qwertycoin::epose::qualification_state_v2(qualification);
    const uint64_t summary_utc_ms = epose_v2_utc_milliseconds();
    MGINFO("event=epose_receipt_summary utc_ms=" << summary_utc_ms
        << " epoch=" << epoch
        << " round="
        << (round == std::numeric_limits<uint64_t>::max()
            ? std::string("none") : std::to_string(round))
        << " height=" << height
        << " attempts_started=" << started
        << " attempts_completed=" << completed
        << " attempts_succeeded=" << succeeded
        << " attempts_failed=" << failed
        << " attempts_cancelled=" << cancelled
        << " attempts_expired=" << expired
        << " attempts_in_flight=" << operations.attempts_in_flight
        << " local_submissions=" << submitted
        << " canonical_inclusions=" << canonical
        << " round_counters=" << (rounds.str().empty() ? "none" : rounds.str())
        << " failure_groups="
        << (failures.str().empty() ? "none" : failures.str())
        << " canonical_unique_receipts=" << coverage.str()
        << " required_receipts=" << required_receipts.str()
        << " rounds_passed="
        << (qualification.available
            ? std::to_string(qualification.rounds_passed) : "unknown")
        << " rounds_required="
        << (qualification.available
            ? std::to_string(qualification.rounds_required) : "unknown")
        << " subject_in_snapshot="
        << (qualification.available
            ? (qualification.subject_in_snapshot ? "true" : "false")
            : "unknown")
        << " qualification=" << state);
    if (qualification.available && qualification.finalized
        && !qualification.qualified)
      MGINFO("event=epose_qualification_final utc_ms=" << summary_utc_ms
          << " epoch=" << epoch
          << " outcome=not_qualified canonical_unique_receipts="
          << coverage.str() << " required_receipts="
          << required_receipts.str() << " rounds_passed="
          << qualification.rounds_passed << " rounds_required="
          << qualification.rounds_required << " unmet_rounds="
          << qualification.rounds_remaining << " unmet_rule="
          << qwertycoin::epose::qualification_unmet_rule_v2(qualification));

    m_epose_v2_last_diagnostic_summary_height = height;
    m_epose_v2_last_diagnostic_summary_epoch = epoch;
    m_epose_v2_last_diagnostic_summary_round = round;
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
    const auto status = m_blockchain_storage.submit_epose_relay_envelopes_v2(
        envelopes, accepted_envelopes);
    // Capacity and expiry are local, non-malicious conditions. Ignore these
    // P2P submissions without relaying them, but do not punish the sender.
    return status == qwertycoin::epose::relay_ingress_status_v2::accepted
        || status == qwertycoin::epose::relay_ingress_status_v2::context_unavailable
        || status == qwertycoin::epose::relay_ingress_status_v2::expired
        || status
            == qwertycoin::epose::relay_ingress_status_v2::capacity_exhausted;
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
      std::vector<qwertycoin::epose::identity_descriptor_v2> descriptors;
      m_blockchain_storage.get_epose_identity_descriptors_v2(epoch, descriptors);
      if (std::any_of(descriptors.begin(), descriptors.end(),
              [&descriptor_hash](const auto &descriptor) {
                return descriptor.endpoint_descriptor_hash == descriptor_hash;
              }))
        return true;

      qwertycoin::epose::membership_snapshot_v2 snapshot{};
      if (m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot)
              == Blockchain::epose_query_status::found
          && std::any_of(snapshot.members.begin(), snapshot.members.end(),
              [&descriptor_hash](const auto &member) {
                return member.endpoint_descriptor_hash == descriptor_hash;
              }))
        return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------------------
  Blockchain::chainlock_ingress core::handle_incoming_chainlock(
      const blobdata& lock, blobdata& relay_blob)
  {
    return m_blockchain_storage.handle_incoming_chainlock(lock, relay_blob);
  }
  //-----------------------------------------------------------------------------------------------
  void core::queue_chainlock_signing(uint64_t height, epee::span<const block> blocks)
  {
    if (blocks.empty())
      return;
    const std::lock_guard<std::mutex> lock(m_chainlock_queue_mutex);
    m_chainlock_queued_tip = std::make_pair(
        height + blocks.size() - 1, get_block_hash(blocks[blocks.size() - 1]));
  }
  //-----------------------------------------------------------------------------------------------
  void core::relay_chainlock_blob(blobdata blob)
  {
    if (m_offline || !get_protocol())
      return;
    NOTIFY_CHAINLOCK_V1::request request{};
    request.lock = std::move(blob);
    get_protocol()->relay_chainlock(
        request, boost::uuids::nil_uuid(), epee::net_utils::zone::public_);
  }
  //-----------------------------------------------------------------------------------------------
  void core::sign_queued_chainlock()
  {
    boost::optional<std::pair<uint64_t, crypto::hash>> tip;
    {
      const std::lock_guard<std::mutex> lock(m_chainlock_queue_mutex);
      tip.swap(m_chainlock_queued_tip);
    }
    if (!tip || !m_epose_v2_service_ready || m_offline || !is_synchronized())
      return;
    const uint64_t height = tip->first;
    const crypto::hash &block_hash = tip->second;
    const std::lock_guard<std::mutex> signer_lock(m_chainlock_signer_mutex);
    if (height <= m_chainlock_signer_state.height // H-8
        || m_blockchain_storage.get_tail_id() != block_hash
        || !is_qwc_contracts_hardfork(
            m_blockchain_storage.get_ideal_hard_fork_version(height))
        || (m_chainlock_signer_state.height != 0
            && m_blockchain_storage.get_block_id_by_height(m_chainlock_signer_state.height)
                != m_chainlock_signer_state.block_hash
            && !m_blockchain_storage.canonical_chainlock_supersedes(
                m_chainlock_signer_state.height)))
      return;
    uint64_t epoch = 0;
    qwertycoin::epose::chainlock_quorum_v1 quorum;
    qwertycoin::epose::chainlock_context_v1 context{};
    size_t signer_index = 0;
    if (!m_blockchain_storage.get_chainlock_epoch(height, epoch)
        || !m_blockchain_storage.get_chainlock_quorum(epoch, quorum)
        || !m_blockchain_storage.get_chainlock_context(context)
        || !quorum.index_of(m_epose_v2_keystore.service_public_key, signer_index))
      return;
    qwertycoin::epose::chainlock_v1 share;
    if (!qwertycoin::epose::sign_chainlock_share_v1(
            height, block_hash, quorum,
            m_epose_v2_keystore.service_public_key,
            m_epose_v2_keystore.service_secret_key, context, share))
    {
      MERROR("Failed to sign chainlock share for height " << height);
      return;
    }
    std::string error;
    const qwertycoin::epose::chainlock_signer_state_v1 signed_state{height, block_hash};
    if (!qwertycoin::epose::save_chainlock_signer_state_v1(
            m_epose_v2_keystore_path + ".chainlock", signed_state, error))
    {
      MERROR("Chainlock share for height " << height << " withheld: " << error);
      return;
    }
    m_chainlock_signer_state = signed_state;
    blobdata relay_blob;
    if (m_blockchain_storage.handle_incoming_chainlock(
            qwertycoin::epose::serialize_chainlock_v1(share), relay_blob)
        == Blockchain::chainlock_ingress::accepted)
      relay_chainlock_blob(std::move(relay_blob));
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
      std::vector<qwertycoin::epose::identity_descriptor_v2> lifecycle;
      m_blockchain_storage.get_epose_identity_descriptors_v2(epoch, lifecycle);
      for (const auto &descriptor : lifecycle)
        canonical_hashes.push_back(descriptor.endpoint_descriptor_hash);
      qwertycoin::epose::membership_snapshot_v2 snapshot{};
      if (m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot)
          == Blockchain::epose_query_status::found)
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
      bool& relayed,
      epose_local_ingress_origin origin)
  {
    return submit_local_epose_envelopes_v2(
        std::vector<blobdata>{envelope}, newly_accepted, relayed, origin);
  }
  //-----------------------------------------------------------------------------------------------
  qwertycoin::epose::relay_ingress_status_v2 core::admit_local_epose_envelopes_v2(
      const std::vector<blobdata> &envelopes,
      std::vector<blobdata> &accepted,
      epose_local_ingress_origin origin)
  {
    qwertycoin::epose::relay_ingress_trace_v2 trace{};
    const auto ingress_status = m_blockchain_storage.submit_epose_relay_envelopes_v2(
        envelopes, accepted, &trace);
    if (ingress_status != qwertycoin::epose::relay_ingress_status_v2::accepted)
    {
      const uint64_t epoch = m_blockchain_storage.get_epose_current_epoch();
      if (origin != epose_local_ingress_origin::rpc_submit)
        MDEBUG("EPoSE-v2 service producer step failed: relay_ingress_rejected");
      if (epose_local_ingress_rejection_warning(origin, epoch,
              m_epose_v2_last_ingress_rejection_epoch,
              m_epose_v2_last_receipt_rejection_epoch,
              m_epose_v2_last_rpc_rejection_epoch,
              m_epose_v2_step_failed_count))
        MWARNING("EPoSE-v2 relay ingress rejected local envelopes: status "
          << epose_v2_relay_ingress_status_name(ingress_status)
          << ", reason " << epose_v2_relay_ingress_rejection_name(trace.rejection)
          << ", semantic " << epose_v2_semantic_status_name(trace.semantic)
          << ", lifecycle " << epose_v2_lifecycle_status_name(trace.lifecycle)
          << ", envelope " << trace.envelope_index << " of " << envelopes.size()
          << ", chain height " << m_blockchain_storage.get_current_blockchain_height());
    }
    return ingress_status;
  }

  bool core::submit_local_epose_envelopes_v2(
      const std::vector<blobdata>& envelopes,
      bool& newly_accepted,
      bool& relayed,
      epose_local_ingress_origin origin,
      bool *ingress_expired)
  {
    newly_accepted = false;
    relayed = false;
    if (ingress_expired != nullptr)
      *ingress_expired = false;
    if (envelopes.empty()
        || std::any_of(envelopes.begin(), envelopes.end(),
            [](const blobdata &envelope) { return envelope.empty(); })
        || m_offline || !get_protocol()->is_synchronized())
      return false;

    std::vector<blobdata> accepted;
    const auto ingress_status = admit_local_epose_envelopes_v2(envelopes, accepted, origin);
    if (ingress_status != qwertycoin::epose::relay_ingress_status_v2::accepted)
    {
      if (ingress_expired != nullptr
          && qwertycoin::epose::local_ingress_is_expected_race_v2(ingress_status))
        *ingress_expired = true;
      return false;
    }
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
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
      const bool current_epoch_descriptor)
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
      const crypto::hash &last_hash = current_epoch_descriptor
          ? m_epose_v2_last_relayed_current_endpoint_hash
          : m_epose_v2_last_relayed_endpoint_hash;
      const auto &last_relay = current_epoch_descriptor
          ? m_epose_v2_last_current_endpoint_relay
          : m_epose_v2_last_endpoint_relay;
      if (last_hash == descriptor_hash
          && last_relay != std::chrono::steady_clock::time_point{}
          && now - last_relay < std::chrono::seconds(60))
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
      if (current_epoch_descriptor)
      {
        m_epose_v2_last_relayed_current_endpoint_hash = descriptor_hash;
        m_epose_v2_last_current_endpoint_relay = now;
      }
      else
      {
        m_epose_v2_last_relayed_endpoint_hash = descriptor_hash;
        m_epose_v2_last_endpoint_relay = now;
      }
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
      if (m_blockchain_storage.get_epose_membership_snapshot_v2(
              candidate.epoch, snapshot) != Blockchain::epose_query_status::found
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
        challenge, context, {QWC_EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES},
        m_epose_v2_keystore.service_secret_key, authorize, source,
        response) == qwertycoin::epose::canonical_service_status_v2::accepted;
  }
  //-----------------------------------------------------------------------------------------------
  bool core::get_epose_v2_diagnostics(
      qwertycoin::epose::receipt_diagnostics_snapshot_v2 &operations,
      qwertycoin::epose::qualification_diagnostics_v2 &qualification,
      const uint64_t requested_epoch) const
  {
    operations = m_epose_v2_receipt_diagnostics.snapshot();
    const auto relay =
        m_blockchain_storage.get_epose_relay_pool_diagnostics_v2();
    operations.relay_queue_items = relay.queue_items;
    operations.relay_queue_bytes = relay.queue_bytes;
    operations.relay_exact_duplicates = relay.exact_duplicates;
    operations.relay_slot_variants = relay.slot_variants;
    operations.relay_expired_round_receipts = relay.expired_round_receipts;
    operations.relay_invalid_context_receipts = relay.invalid_context_receipts;
    operations.relay_canonical_receipts = relay.canonical_receipts;
    operations.relay_capacity_rejections = relay.capacity_rejections;
    operations.relay_template_selected_receipts =
        relay.template_selected_receipts;
    qualification = {};
    if (!m_epose_v2_service_ready)
      return true;

    qwertycoin::epose::consensus_parameters_v2 parameters{};
    if (!m_blockchain_storage.get_epose_consensus_parameters_v2(parameters))
      return true;
    const uint64_t height = m_blockchain_storage.get_current_blockchain_height();
    if (height < parameters.timing.activation_height)
      return true;
    const uint64_t current_epoch =
        (height - parameters.timing.activation_height)
        / parameters.timing.epoch_length;
    const uint64_t epoch = requested_epoch == std::numeric_limits<uint64_t>::max()
        ? current_epoch : requested_epoch;
    if (epoch > current_epoch)
      return true;
    qualification.epoch = epoch;
    qualification.chain_height = height;
    if (!parameters.timing.evidence_deadline(
            epoch, qualification.evidence_deadline_height))
      return true;

    qwertycoin::epose::membership_snapshot_v2 snapshot{};
    if (m_blockchain_storage.get_epose_membership_snapshot_v2(epoch, snapshot)
        != Blockchain::epose_query_status::found)
      return true;
    const auto historical_member = std::find_if(
        snapshot.members.begin(), snapshot.members.end(), [this](const auto &member) {
          return member.identity_id == m_epose_v2_identity_id;
        });
    qualification.subject_in_snapshot = historical_member != snapshot.members.end();
    if (qualification.subject_in_snapshot)
      qualification.subject_identity = historical_member->service_public_key;

    std::vector<size_t> coverage;
    if (qualification.subject_in_snapshot)
    {
      if (!m_blockchain_storage.get_epose_receipt_coverage_v2(
              epoch, qualification.subject_identity, coverage))
        return true;
    }
    else
      coverage.assign(parameters.committee.round_count, 0);

    qualification.available = true;
    qualification.rounds_required = parameters.committee.rounds_required;
    const size_t committee_size = qualification.subject_in_snapshot
        ? qwertycoin::epose::compatible_committee_size_v2(
              snapshot, *historical_member, parameters.committee.committee_size)
        : 0;
    const bool judged = qwertycoin::epose::compatible_committee_judged_v2(
        snapshot, committee_size, parameters.committee.min_compatible_committee);
    if (qualification.subject_in_snapshot)
    {
      qualification.dial_capabilities = historical_member->dial_capabilities;
      qualification.dns_address_families = historical_member->dns_address_families;
      qualification.rounds_judged = judged ? parameters.committee.round_count : 0;
      qualification.rounds_unjudged = judged ? 0 : parameters.committee.round_count;
    }
    const size_t required =
        qwertycoin::epose::required_receipts_for_committee_size_v2(
            committee_size);
    qualification.canonical_unique_receipts.reserve(coverage.size());
    qualification.committee_sizes.assign(coverage.size(),
        qualification.subject_in_snapshot ? committee_size : 0);
    qualification.required_receipts.assign(coverage.size(),
        qualification.subject_in_snapshot ? required : 0);
    for (const size_t count : coverage)
    {
      qualification.canonical_unique_receipts.push_back(count);
      if (judged
          && count >= required && required != 0)
        ++qualification.rounds_passed;
    }
    qualification.rounds_remaining = qualification.rounds_passed
            >= qualification.rounds_required
        ? 0 : qualification.rounds_required - qualification.rounds_passed;

    qwertycoin::epose::qualification_set_v2 finalized{};
    qualification.finalized = m_blockchain_storage.get_epose_qualification_v2(
        epoch, finalized) == Blockchain::epose_query_status::found;
    if (qualification.finalized)
      qualification.qualified = std::find(
          finalized.qualified_nodes.begin(), finalized.qualified_nodes.end(),
          qualification.subject_identity)
          != finalized.qualified_nodes.end();
    return true;
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
  bool core::init(const boost::program_options::variables_map& vm, const cryptonote::test_options *test_options, const GetCheckpointsCallback& get_checkpoints/* = nullptr */, bool allow_dns, const qwertycoin::epose::consensus_parameters_v2 *epose_test_parameters)
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

    m_blockchain_storage.add_block_notify(
        [this](uint64_t height, epee::span<const block> blocks)
        { queue_chainlock_signing(height, blocks); });

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

    // QWC's public chain is HF17-native. The height-21,600 EPoSE relay
    // hardening is local policy, not a block-version transition. Starting
    // regtest at v1 creates a different genesis from wallet clients and makes
    // the disposable RPC test network reject itself.
    const uint64_t regtest_fork_height = regtest ? get_regtest_fork_height_from_args(vm) : 0;
    const std::pair<uint8_t, uint64_t> regtest_hard_forks[3] = {
      std::make_pair(HF_VERSION_QWC_EPOSE, 0),
      regtest_fork_height
          ? std::pair<uint8_t, uint64_t>(HF_VERSION_QWC_CONTRACTS, regtest_fork_height)
          : std::pair<uint8_t, uint64_t>(0, 0),
      std::make_pair(0, 0)
    };
    const cryptonote::test_options regtest_test_options = {
      regtest_hard_forks,
      0
    };
    const difficulty_type fixed_difficulty = command_line::get_arg(vm, arg_fixed_difficulty);
    qwertycoin::epose::consensus_parameters_v2 regtest_epose_parameters{};
    qwertycoin::epose::relay_policy_v2 regtest_epose_relay_policy{};
    const qwertycoin::epose::relay_policy_v2 *test_relay_policy = nullptr;
    if (command_line::get_arg(vm, arg_regtest_epose_v2))
    {
      CHECK_AND_ASSERT_MES(regtest && regtest_fork_height != 0 && epose_test_parameters == nullptr,
          false, "--regtest-epose-v2 requires --regtest and --regtest-fork-height");
      block genesis{};
      generate_genesis_block(genesis, get_config(FAKECHAIN).GENESIS_TX,
          get_config(FAKECHAIN).GENESIS_NONCE, HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_EPOSE);
      const crypto::hash genesis_hash = get_block_hash(genesis);
      CHECK_AND_ASSERT_MES(qwertycoin::epose::compiled_consensus_parameters_v2(
              FAKECHAIN, genesis_hash, regtest_epose_parameters)
          && qwertycoin::epose::compiled_relay_policy_v2(
              FAKECHAIN, genesis_hash, regtest_epose_parameters.parameter_set_hash,
              regtest_epose_relay_policy), false, "Invalid regtest EPoSE-v2 profile");
      epose_test_parameters = &regtest_epose_parameters;
      test_relay_policy = &regtest_epose_relay_policy;
    }
    r = m_blockchain_storage.init(db.release(), m_nettype, m_offline, regtest ? &regtest_test_options : test_options, fixed_difficulty, get_checkpoints, epose_test_parameters, test_relay_policy);
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

    if (!keep_alt_blocks && !m_blockchain_storage.get_db().is_read_only()
        && !m_blockchain_storage.is_epose_enabled())
      m_blockchain_storage.get_db().drop_alt_blocks();
    else if (!keep_alt_blocks && !m_blockchain_storage.get_db().is_read_only()
        && is_qwc_contracts_hardfork(m_blockchain_storage.get_ideal_hard_fork_version()))
      m_blockchain_storage.prune_irrecoverable_alternatives();

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
  bool core::handle_incoming_tx(const blobdata& tx_blob, tx_verification_context& tvc, relay_method tx_relay, bool relayed, bool reject_mined)
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

    if (reject_mined && m_blockchain_storage.have_tx(txid))
    {
      tvc.m_verifivation_failed = true;
      tvc.m_double_spend = true;
      tvc.m_no_drop_offense = true;
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
        [this, &emission_amount, &total_fee_amount](uint64_t height, const crypto::hash& hash, const block& b){
      std::vector<transaction> txs;
      std::vector<crypto::hash> missed_txs;
      uint64_t coinbase_amount = get_outs_money_amount(b.miner_tx);
      this->get_transactions(b.tx_hashes, txs, missed_txs, true);
      uint64_t tx_fee_amount = 0;
      uint64_t minted = 0;
      if (!sum_contract_miner_fees(txs, b.major_version, tx_fee_amount) || !m_blockchain_storage.get_vm_minted(height, b.major_version, minted))
        return false;

      emission_amount += coinbase_amount - minted - tx_fee_amount;
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

    uint8_t version = m_blockchain_storage.get_ideal_hard_fork_version(m_blockchain_storage.get_current_blockchain_height());
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
    const bool added = m_blockchain_storage.add_new_block(b, bvc);
    const bool committed = cleanup_handle_incoming_blocks(true);
    //anyway - update miner template
    update_miner_block_template();
    m_miner.resume();

    if (!added || !committed)
    {
      if (!committed && bvc.m_added_to_main_chain)
      {
        bvc.m_added_to_main_chain = false;
        bvc.m_retryable_busy = true;
      }
      return false;
    }

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
    sign_queued_chainlock();
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
    bool cleanup_called = false;
    const auto auto_cleanup = epee::misc_utils::create_scope_leave_handler([this, &cleanup_called](){
      if (!cleanup_called)
        this->m_blockchain_storage.cleanup_handle_incoming_blocks();
    });

    const bool handled = handle_incoming_block(block_blob,
      b,
      bvc,
      extra_block_txs,
      update_miner_blocktemplate);
    cleanup_called = true;
    const bool committed = m_blockchain_storage.cleanup_handle_incoming_blocks();
    if (!committed && bvc.m_added_to_main_chain)
    {
      bvc.m_added_to_main_chain = false;
      bvc.m_retryable_busy = true;
    }
    return handled && committed;
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
  bool core::pool_has_expired_window_tx(const crypto::hash &id) const
  {
    return m_mempool.have_expired_window_tx(id);
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
    sign_queued_chainlock();
    for (blobdata &blob : m_blockchain_storage.retry_pending_chainlocks())
      relay_chainlock_blob(std::move(blob));
    m_check_updates_interval.do_call(boost::bind(&core::check_updates, this));
    m_check_disk_space_interval.do_call(boost::bind(&core::check_disk_space, this));
    m_block_rate_interval.do_call(boost::bind(&core::check_block_rate, this));
    m_blockchain_pruning_interval.do_call(boost::bind(&core::update_blockchain_pruning, this));
    m_diff_recalc_interval.do_call(boost::bind(&core::recalculate_difficulties, this));
    m_miner.on_idle();
    m_mempool.on_idle();
    if (!(m_epose_idle_service_update_hook
            ? m_epose_idle_service_update_hook()
            : update_epose_v2_service_producer())
        && epose_warning_once_per_epoch(m_epose_v2_last_service_update_failure_epoch,
            m_epose_idle_epoch_hook
                ? m_epose_idle_epoch_hook()
                : m_blockchain_storage.get_epose_current_epoch()))
    {
      MERROR("EPoSE-v2 service producer update failed; it will retry without changing consensus state");
      if (m_epose_idle_service_warning_hook)
        m_epose_idle_service_warning_hook();
    }
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

    if (tools::vercmp(version.c_str(), QWERTYCOIN_VERSION) <= 0)
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
  bool core::check_block_rate()
  {
    if (m_offline || m_nettype == FAKECHAIN || m_target_blockchain_height > get_current_blockchain_height() || m_target_blockchain_height == 0)
    {
      MDEBUG("Not checking block rate, offline or syncing");
      return true;
    }

    const double threshold = 1. / (864000 / block_target(m_nettype)); // one false positive every 10 days
    static constexpr unsigned int seconds[] = { 5400, 3600, 1800, 1200, 600 };
    const unsigned int max_blocks_checked = std::max<uint64_t>(150, blocks_for_duration(m_nettype, seconds[0]));

    const time_t now = time(NULL);
    const std::vector<time_t> timestamps = m_blockchain_storage.get_last_block_timestamps(max_blocks_checked);
    for (size_t n = 0; n < sizeof(seconds)/sizeof(seconds[0]); ++n)
    {
      unsigned int b = 0;
      const time_t time_boundary = now - static_cast<time_t>(seconds[n]);
      for (time_t ts: timestamps) b += ts >= time_boundary;
      const double p = block_rate_probability(b, seconds[n] / block_target(m_nettype));
      MDEBUG("blocks in the last " << seconds[n] / 60 << " minutes: " << b << " (probability " << p << ")");
      if (p < threshold)
      {
        MWARNING("There were " << b << (b == max_blocks_checked ? " or more" : "") << " blocks in the last " << seconds[n] / 60 << " minutes, there might be large hash rate changes, or we might be partitioned, cut off from the Qwertycoin network or under attack, or your computer's time is off. Or it could be just sheer bad luck.");

        std::shared_ptr<tools::Notify> block_rate_notify = m_block_rate_notify;
        if (block_rate_notify)
        {
          auto expected = seconds[n] / block_target(m_nettype);
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
