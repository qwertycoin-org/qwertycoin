// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose_http_client.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <map>
#include <limits>
#include <mutex>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/optional.hpp>

#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "misc_log_ex.h"
#include "net/net_parse_helpers.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/portable_storage_template_helper.h"
#include "string_tools.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "cn"

namespace cryptonote
{
namespace
{
  using steady = std::chrono::steady_clock;
  using qwertycoin::epose::receipt_attempt_stage_v2;
  using qwertycoin::epose::receipt_failure_reason_v2;
  using qwertycoin::epose::receipt_step_result_v2;

  constexpr size_t EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES = 2 * 1024 * 1024;
  constexpr size_t EPOSE_V2_MAX_DESCRIPTOR_RESPONSE_BYTES = 4096;
  constexpr size_t EPOSE_V2_MAX_SERVICE_RESPONSE_BYTES =
      EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES * 2 + 4096;
  constexpr size_t EPOSE_V2_MAX_RESPONSE_HEADER_BYTES = 16 * 1024;
  constexpr size_t EPOSE_V2_READ_CHUNK_BYTES = 16 * 1024;
  constexpr uint64_t EPOSE_V2_TARGET_MS = uint64_t(DIFFICULTY_TARGET_V2) * 1000;
  static_assert(EPOSE_V2_MAX_SERVICE_RECV_BYTES
      == EPOSE_V2_MAX_SERVICE_RESPONSE_BYTES + EPOSE_V2_MAX_RESPONSE_HEADER_BYTES,
      "service receive bound");
  static_assert(EPOSE_V2_MAX_DESCRIPTOR_RECV_BYTES
      == EPOSE_V2_MAX_DESCRIPTOR_RESPONSE_BYTES + EPOSE_V2_MAX_RESPONSE_HEADER_BYTES,
      "descriptor receive bound");

  void append_le64(std::string &out, uint64_t value)
  {
    for (int i = 0; i < 8; ++i)
      out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
  }

  epose_network_v2::cancel_resolve resolve_host(const std::string &host,
      boost::asio::io_context &io, epose_network_v2::resolve_handler complete)
  {
    const auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(io);
    resolver->async_resolve(host, "0",
        [resolver, complete](const boost::system::error_code &error,
            boost::asio::ip::tcp::resolver::results_type results) {
      std::vector<boost::asio::ip::address> addresses;
      for (const auto &result : results)
        addresses.push_back(result.endpoint().address());
      complete(error, std::move(addresses));
    });
    return [resolver] { resolver->cancel(); };
  }

  bool unreachable_connect_error(const boost::system::error_code &error)
  {
    return error == boost::asio::error::host_unreachable
        || error == boost::asio::error::network_unreachable
        || error == boost::asio::error::network_down
        || error == boost::asio::error::address_family_not_supported;
  }

  receipt_failure_reason_v2 classify_connect_error(const boost::system::error_code &error)
  {
    if (error == boost::asio::error::connection_refused)
      return receipt_failure_reason_v2::connection_refused;
    if (error == boost::asio::error::timed_out)
      return receipt_failure_reason_v2::connect_timeout;
    if (unreachable_connect_error(error))
      return receipt_failure_reason_v2::connect_unreachable;
    return receipt_failure_reason_v2::transport_failure;
  }

  // Higher says more about the peer: refused, unreachable, other, timed_out;
  // 0 for no error.
  int connect_error_rank(const boost::system::error_code &error)
  {
    if (!error)
      return 0;
    if (error == boost::asio::error::connection_refused)
      return 4;
    if (unreachable_connect_error(error))
      return 3;
    if (error == boost::asio::error::timed_out)
      return 1;
    return 2;
  }

  std::string lower_ascii(std::string value)
  {
    for (char &c : value)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
  }

  std::string trim_ows(const std::string &value)
  {
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
      ++begin;
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
      --end;
    return value.substr(begin, end - begin);
  }

  receipt_step_result_v2 decode_descriptor(const std::string &payload,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const crypto::hash &required_hash,
      qwertycoin::epose::endpoint_descriptor_v2 &descriptor)
  {
    descriptor = {};
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response response{};
    if (!epee::serialization::load_t_from_json(response, payload))
      return {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed};
    if (response.status != CORE_RPC_STATUS_OK || !response.ready)
      return {receipt_attempt_stage_v2::descriptor_lookup,
          receipt_failure_reason_v2::descriptor_unavailable};
    if (response.version > std::numeric_limits<uint8_t>::max()
        || response.transport > std::numeric_limits<uint8_t>::max()
        || response.port > std::numeric_limits<uint16_t>::max()
        || response.service_kind > std::numeric_limits<uint8_t>::max()
        || response.service_version > std::numeric_limits<uint8_t>::max())
      return {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_invalid};
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
        || !epee::string_tools::hex_to_pod(response.signature, descriptor.signature))
    {
      descriptor = {};
      return {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_invalid};
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
      return {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_commitment_mismatch};
    }
    // Discovery is an authenticated directory: the signed endpoint may belong
    // to a third party. The service request dials descriptor.host/port.
    if (qwertycoin::epose::validate_discovered_endpoint_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, descriptor, required_hash)
        != qwertycoin::epose::resource_status_v2::accepted)
    {
      descriptor = {};
      return {receipt_attempt_stage_v2::descriptor_validation,
          receipt_failure_reason_v2::descriptor_invalid};
    }
    return {};
  }

  receipt_step_result_v2 decode_service_response(const std::string &payload,
      qwertycoin::epose::canonical_service_response_v2 &service_response)
  {
    service_response = {};
    cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::response response{};
    if (!epee::serialization::load_t_from_json(response, payload))
      return {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed};
    if (response.status != CORE_RPC_STATUS_OK)
      return {receipt_attempt_stage_v2::http_exchange,
          receipt_failure_reason_v2::remote_challenge_rejected};
    if (response.block_blob.size() % 2 != 0
        || response.block_blob.size() / 2
            > EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES)
      return {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_oversized};
    if (!epee::string_tools::parse_hexstr_to_binbuff(
            response.block_blob, service_response.block_blob)
        || !epee::string_tools::hex_to_pod(
            response.subject_signature, service_response.subject_signature))
    {
      service_response = {};
      return {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed};
    }
    return {};
  }
}

  uint64_t epose_receipt_round_budget_ms_v2(
      const uint64_t height, const uint64_t deadline_height)
  {
    if (height > deadline_height)
      return 0;
    const uint64_t blocks = deadline_height - height + 1;
    if (blocks == 0 || blocks > std::numeric_limits<uint64_t>::max() / EPOSE_V2_TARGET_MS)
      return std::numeric_limits<uint64_t>::max();
    return blocks * EPOSE_V2_TARGET_MS;
  }

  uint64_t epose_receipt_jitter_window_ms_v2(
      const uint64_t first_height, const uint64_t last_height)
  {
    if (last_height < first_height)
      return 0;
    const uint64_t blocks = last_height - first_height + 1;
    if (blocks == 0 || blocks >= 100)
      return EPOSE_V2_TARGET_MS;
    return blocks * EPOSE_V2_TARGET_MS / 100;
  }

  uint64_t epose_receipt_start_offset_ms_v2(
      const crypto::public_key &local, const crypto::public_key &peer,
      const uint64_t epoch, const uint64_t round, const uint64_t window_ms)
  {
    if (window_ms == 0)
      return 0;
    static const char domain[] = "QWC_EPOSE_RECEIPT_JITTER_V2";
    std::string data(domain, sizeof(domain) - 1);
    data.append(reinterpret_cast<const char *>(&local), sizeof(local));
    data.append(reinterpret_cast<const char *>(&peer), sizeof(peer));
    append_le64(data, epoch);
    append_le64(data, round);
    const crypto::hash digest = crypto::cn_fast_hash(data.data(), data.size());
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
      value |= uint64_t(static_cast<uint8_t>(digest.data[i])) << (8 * i);
    return value % window_ms;
  }

  uint64_t epose_receipt_recv_total_ms_v2(
      const uint64_t max_bytes, const uint64_t slack_ms, const uint64_t min_rate)
  {
    constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
    if (min_rate == 0)
      return max;
    // ceil(max_bytes * 1000 / min_rate) without overflowing the product.
    const uint64_t whole = max_bytes / min_rate;
    const uint64_t rest = max_bytes % min_rate;
    if (whole > max / 1000 || rest > max / 1000)
      return max;
    const uint64_t rest_ms = rest * 1000 / min_rate + (rest * 1000 % min_rate != 0 ? 1 : 0);
    if (whole * 1000 > max - rest_ms)
      return max;
    const uint64_t transfer = whole * 1000 + rest_ms;
    return transfer > max - slack_ms ? max : transfer + slack_ms;
  }

  steady::time_point epose_stage_deadlines_v2::stage(
      const std::chrono::milliseconds cap) const
  {
    const auto now = steady::now();
    if (round_deadline <= now)
      return round_deadline;
    if (cap.count() < 0)
      return now;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(round_deadline - now) <= cap)
      return round_deadline;
    return now + cap;
  }

  bool public_epose_dial_address(const boost::asio::ip::address &address)
  {
    // Local transport policy only. Do not reuse this in descriptor/chain validation.
    if (address.is_v4())
    {
      const auto b = address.to_v4().to_bytes();
      return !(b[0] == 0 || b[0] == 10 || b[0] == 127 || b[0] >= 224
          || (b[0] == 100 && (b[1] & 0xc0) == 64)
          || (b[0] == 169 && b[1] == 254)
          || (b[0] == 172 && (b[1] & 0xf0) == 16)
          || (b[0] == 192 && b[1] == 168)
          || (b[0] == 192 && b[1] == 0 && (b[2] == 0 || b[2] == 2))
          || (b[0] == 192 && b[1] == 88 && b[2] == 99)
          || (b[0] == 198 && (b[1] == 18 || b[1] == 19))
          || (b[0] == 198 && b[1] == 51 && b[2] == 100)
          || (b[0] == 203 && b[1] == 0 && b[2] == 113));
    }
    const auto v6 = address.to_v6();
    const auto b = v6.to_bytes();
    if (v6.scope_id() != 0)
      return false;
    const bool nat64 = b[0] == 0 && b[1] == 0x64 && b[2] == 0xff && b[3] == 0x9b
        && b[4] == 0 && (b[5] == 0 || b[5] == 1)
        && std::all_of(b.begin() + 6, b.begin() + 12, [](uint8_t x) { return x == 0; });
    if (v6.is_v4_mapped() || nat64)
      return public_epose_dial_address(boost::asio::ip::address_v4(
          boost::asio::ip::address_v4::bytes_type{{b[12], b[13], b[14], b[15]}}));
    // Only native global unicast otherwise; excludes the rest of the local-use
    // NAT64 /48 outside 64:ff9b:1::/96, compatible IPv4,
    // unspecified, loopback, ULA, link/site-local, multicast and reserved space.
    // Also exclude protocol assignments (including Teredo), 6to4 and documentation.
    return (b[0] & 0xe0) == 0x20
        && !(b[0] == 0x20 && b[1] == 0x01 && b[2] < 2)
        && !(b[0] == 0x20 && b[1] == 0x02)
        && !(b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x0d && b[3] == 0xb8)
        && !(b[0] == 0x3f && b[1] == 0xff && (b[2] & 0xf0) == 0);
  }

  epose_network_v2::cancel_operation connect_epose_address(
      const boost::asio::ip::tcp::endpoint &endpoint, boost::asio::io_context &io,
      const steady::time_point deadline, epose_network_v2::connect_handler complete)
  {
    struct connection
    {
      boost::asio::io_context &io;
      boost::asio::ip::tcp::socket socket;
      boost::asio::steady_timer timer;
      epose_network_v2::connect_handler complete;
      steady::time_point deadline;
      bool done = false;
      connection(boost::asio::io_context &context, epose_network_v2::connect_handler handler,
          steady::time_point at)
        : io(context), socket(context), timer(context), complete(std::move(handler)), deadline(at) {}
    };
    const auto pending = std::make_shared<connection>(io, std::move(complete), deadline);
    pending->timer.expires_at(deadline);
    pending->timer.async_wait([pending](const boost::system::error_code &error) {
      if (!error)
      {
        boost::system::error_code ignored;
        pending->socket.close(ignored);
      }
    });
    pending->socket.async_connect(endpoint, [pending](const boost::system::error_code &error) {
      pending->timer.cancel();
      if (pending->done)
        return;
      pending->done = true;
      auto handler = std::move(pending->complete);
      pending->complete = {};
      if (!error && steady::now() < pending->deadline)
      {
        handler({}, std::move(pending->socket));
        return;
      }
      boost::system::error_code reported = error;
      if (!error || steady::now() >= pending->deadline)
        reported = boost::asio::error::timed_out;
      boost::system::error_code ignored;
      pending->socket.close(ignored);
      handler(reported, boost::asio::ip::tcp::socket(pending->io));
    });
    return [pending] {
      if (pending->done)
        return;
      pending->done = true;
      pending->complete = {};
      pending->timer.cancel();
      boost::system::error_code ignored;
      pending->socket.close(ignored);
    };
  }

  epose_dial_budget_v2::epose_dial_budget_v2(const size_t limit) : limit_(limit)
  {
  }

  size_t epose_dial_budget_v2::in_use() const
  {
    return in_use_;
  }

  bool epose_dial_budget_v2::try_acquire()
  {
    if (in_use_ >= limit_ || !waiters_.empty())
      return false;
    ++in_use_;
    return true;
  }

  void epose_dial_budget_v2::release()
  {
    if (!waiters_.empty())
    {
      // The unit passes on: in_use_ is unchanged.
      auto waiter = std::move(waiters_.front());
      waiters_.erase(waiters_.begin());
      waiter.second(waiter.first);
      return;
    }
    if (in_use_ != 0)
      --in_use_;
  }

  uint64_t epose_dial_budget_v2::wait(std::function<void(uint64_t)> wake)
  {
    const uint64_t id = ++next_waiter_;
    waiters_.emplace_back(id, std::move(wake));
    return id;
  }

  void epose_dial_budget_v2::leave(const uint64_t id)
  {
    waiters_.erase(std::remove_if(waiters_.begin(), waiters_.end(),
        [id](const std::pair<uint64_t, std::function<void(uint64_t)>> &waiter) {
          return waiter.first == id;
        }), waiters_.end());
  }

  std::vector<boost::asio::ip::address> epose_happy_eyeballs_order_v2(
      const std::vector<boost::asio::ip::address> &addresses, const bool prefer_v6)
  {
    std::vector<boost::asio::ip::address> preferred;
    std::vector<boost::asio::ip::address> other;
    for (const auto &address : addresses)
      (address.is_v6() == prefer_v6 ? preferred : other).push_back(address);
    std::vector<boost::asio::ip::address> ordered;
    ordered.reserve(addresses.size());
    for (size_t i = 0; i < preferred.size() || i < other.size(); ++i)
    {
      if (i < preferred.size())
        ordered.push_back(preferred[i]);
      if (i < other.size())
        ordered.push_back(other[i]);
    }
    return ordered;
  }

  epose_network_v2::cancel_operation happy_eyeballs_epose_dial_v2(
      epose_network_v2::dial_request request, boost::asio::io_context &io,
      epose_network_v2::connect_handler complete)
  {
    using qwertycoin::epose::receipt_dial_family_counters_v2;
    struct attempt
    {
      uint64_t id = 0;
      boost::asio::ip::address address;
      epose_network_v2::cancel_operation cancel;
    };
    // The job's own unit covers one live attempt; between events
    // extras <= max(live.size() - 1, 0), so no borrowed unit sits idle.
    struct dial : std::enable_shared_from_this<dial>
    {
      epose_network_v2::dial_request request;
      boost::asio::io_context &io;
      boost::asio::steady_timer timer;
      epose_network_v2::connect_handler complete;
      std::vector<attempt> live;
      size_t next = 0;       // next candidate in request.addresses
      size_t extras = 0;     // budget units held
      uint64_t last_attempt = 0;
      uint64_t timer_id = 0;
      uint64_t waiter = 0;   // budget waiter id, 0 = none
      bool done = false;

      dial(epose_network_v2::dial_request value, boost::asio::io_context &context,
          epose_network_v2::connect_handler handler)
        : request(std::move(value)), io(context), timer(context), complete(std::move(handler)) {}

      receipt_dial_family_counters_v2 &family(const boost::asio::ip::address &address)
      {
        return address.is_v6() ? request.counters->ipv6 : request.counters->ipv4;
      }

      std::vector<attempt>::iterator find(uint64_t id)
      {
        return std::find_if(live.begin(), live.end(),
            [id](const attempt &entry) { return entry.id == id; });
      }

      // Skips (and counts) candidates the dial policy forbids; true while a
      // dialable candidate remains.
      bool more()
      {
        while (next < request.addresses.size()
            && !public_epose_dial_address(request.addresses[next]))
        {
          ++family(request.addresses[next]).prohibited;
          request.report->prohibited = true;
          ++next;
        }
        return next < request.addresses.size();
      }

      void leave_budget()
      {
        if (waiter != 0)
          request.budget->leave(waiter);
        waiter = 0;
      }

      // Returns every extra unit the live attempts do not use.
      void settle()
      {
        while (extras > 0 && extras >= live.size())
        {
          --extras;
          request.budget->release();
        }
      }

      // Starts the next candidate on a unit the dial already holds and
      // restarts the attempt delay from now.
      bool launch()
      {
        if (!more())
          return false;
        if (steady::now() >= request.deadline)
        {
          request.report->expired = true; // a candidate is left undialed
          return false;
        }
        leave_budget();
        const auto address = request.addresses[next++];
        const uint64_t id = ++last_attempt;
        ++family(address).attempts;
        live.push_back({id, address, {}});
        auto self = shared_from_this();
        const auto failed = [self, id](const boost::system::error_code &code) {
          boost::asio::post(self->io, [self, id, code] {
            self->finished(id, code, boost::asio::ip::tcp::socket(self->io));
          });
        };
        try
        {
          auto cancel = request.connect(boost::asio::ip::tcp::endpoint(address, request.port),
              io, request.deadline,
              [self, id](boost::system::error_code error, boost::asio::ip::tcp::socket socket) {
                // Always on a fresh handler, so no completion re-enters launch().
                const auto shared = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));
                boost::asio::post(self->io, [self, id, error, shared] {
                  self->finished(id, error, std::move(*shared));
                });
              });
          const auto found = find(id);
          if (found != live.end())
            found->cancel = std::move(cancel);
        }
        catch (const boost::system::system_error &e)
        {
          failed(e.code());
        }
        catch (...)
        {
          failed(boost::system::errc::make_error_code(boost::system::errc::io_error));
        }
        arm();
        return true;
      }

      // Connection attempt delay from the latest start, clipped to the
      // deadline; an attempt never starts at or after it.
      void arm()
      {
        const uint64_t id = ++timer_id;
        if (!more())
        {
          timer.cancel();
          return;
        }
        const auto now = steady::now();
        timer.expires_at(request.deadline - now <= request.attempt_delay
            ? request.deadline : now + request.attempt_delay);
        auto self = shared_from_this();
        timer.async_wait([self, id](const boost::system::error_code &error) {
          if (error || id != self->timer_id)
            return;
          self->delay_elapsed();
        });
      }

      void delay_elapsed()
      {
        if (waiter != 0 || !more())
          return;
        if (steady::now() >= request.deadline)
        {
          request.report->expired = true;
          return;
        }
        // Without a budget or at the cap the next attempt waits for a failure.
        if (!request.budget || extras >= request.extra_limit)
          return;
        if (request.budget->try_acquire())
        {
          ++extras;
          launch();
          settle();
          return;
        }
        auto self = shared_from_this();
        waiter = request.budget->wait([self](uint64_t id) {
          boost::asio::post(self->io, [self, id] { self->granted(id); });
        });
      }

      // A unit passed on from the budget's queue; this dial owns it now.
      void granted(uint64_t id)
      {
        if (waiter == id)
          waiter = 0;
        ++extras;
        if (!done)
          launch();
        settle();
      }

      void finished(uint64_t id, const boost::system::error_code &error,
          boost::asio::ip::tcp::socket socket)
      {
        const auto found = find(id);
        if (done || found == live.end())
        {
          boost::system::error_code ignored;
          socket.close(ignored);
          return;
        }
        const auto address = found->address;
        live.erase(found);
        auto &counters = family(address);
        if (error == boost::asio::error::timed_out && steady::now() >= request.deadline)
          request.report->expired = true; // the attempt ran into the deadline
        if (!error)
        {
          ++counters.connected;
          request.report->connected = true;
          request.report->winner = address;
          finish({}, std::move(socket));
          return;
        }
        boost::system::error_code ignored;
        socket.close(ignored);
        if (error == boost::asio::error::connection_refused)
          ++counters.refused;
        else if (unreachable_connect_error(error))
          ++counters.unreachable;
        else if (error == boost::asio::error::timed_out)
          ++counters.timed_out;
        else
          ++counters.failed;
        if (connect_error_rank(error) > connect_error_rank(request.report->failure))
          request.report->failure = error;
        // The failed attempt's unit starts the next candidate at once.
        if (!launch() && live.empty())
        {
          fail();
          return;
        }
        settle();
      }

      void fail()
      {
        boost::system::error_code error = request.report->failure;
        if (!error)
        {
          if (steady::now() >= request.deadline)
            error = boost::asio::error::timed_out;
          else if (request.report->prohibited)
            error = boost::asio::error::access_denied;
          else
            error = boost::asio::error::host_not_found;
        }
        finish(error, boost::asio::ip::tcp::socket(io));
      }

      // Every live attempt is counted abandoned and cancelled.
      void stop()
      {
        ++timer_id;
        timer.cancel();
        leave_budget();
        auto losers = std::move(live);
        live.clear();
        for (auto &loser : losers)
        {
          ++family(loser.address).abandoned;
          if (loser.cancel)
            loser.cancel();
        }
        settle();
      }

      void finish(const boost::system::error_code &error, boost::asio::ip::tcp::socket socket)
      {
        done = true;
        stop();
        auto handler = std::move(complete);
        complete = {};
        if (handler)
          handler(error, std::move(socket));
      }
    };
    if (!request.connect)
      request.connect = epose_network_v2::connect_function(&connect_epose_address);
    const auto pending = std::make_shared<dial>(std::move(request), io, std::move(complete));
    if (!pending->launch())
      pending->fail();
    return [pending] {
      if (pending->done)
        return;
      pending->done = true;
      pending->complete = {};
      pending->stop();
    };
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

namespace detail
{
  struct receipt_job;
  struct http_exchange;

  struct epose_receipt_job_state
  {
    mutable std::mutex mutex;
    std::condition_variable done_cv;
    bool done = false;
    epose_receipt_exchange_v2 exchange{};
    std::atomic<bool> cancel_requested{false};
    std::weak_ptr<epose_receipt_client_state> client;
    std::weak_ptr<receipt_job> runner; // worker thread only

    void complete(epose_receipt_exchange_v2 value)
    {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (done)
          return;
        exchange = std::move(value);
        done = true;
      }
      done_cv.notify_all();
    }
  };

  struct epose_receipt_client_state
  {
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work;
    const epose_network_v2 network;
    const size_t capacity;
    mutable std::mutex mutex;
    size_t in_flight = 0;
    bool stopped = false;
    // Worker thread only from here to jobs.
    // The resolver slot: busy until its lookup completes, even if cancelled.
    bool resolving = false;
    std::string resolving_host;
    bool lookup_cancelled = false;
    epose_network_v2::cancel_resolve lookup_cancel;
    using lookup_resume = std::function<void(const boost::system::error_code &,
        const std::vector<boost::asio::ip::address> &)>;
    // Exchanges waiting for the pending lookup, its starter included; the last
    // one to leave cancels it.
    std::vector<std::pair<uint64_t, lookup_resume>> lookup_waiters;
    uint64_t next_waiter = 0;
    struct resolved_host
    {
      uint64_t epoch = 0;
      std::vector<boost::asio::ip::address> addresses; // filtered, resolver order
    };
    // Last successful resolution per host, all from resolved_epoch; see
    // EPOSE_V2_RESOLVED_HOST_ENTRIES.
    std::map<std::string, resolved_host> resolved_hosts;
    uint64_t resolved_epoch = 0;
    std::vector<std::weak_ptr<receipt_job>> jobs;
    const std::shared_ptr<epose_dial_budget_v2> dial_budget;
    // Last winning family per host (EPOSE_V2_DIAL_PREFERENCE_ENTRIES);
    // preference_age orders the hosts oldest recorded first.
    struct family_preference
    {
      bool v6 = false;
      uint64_t recorded = 0;
    };
    std::map<std::string, family_preference> preferences;
    std::map<uint64_t, std::string> preference_age;
    uint64_t preference_clock = 0;
    const size_t preference_entries;

    epose_receipt_client_state(epose_network_v2 hooks, size_t limit, size_t extra_attempts,
        size_t preference_bound)
      : work(boost::asio::make_work_guard(io)), network(std::move(hooks)), capacity(limit),
        dial_budget(std::make_shared<epose_dial_budget_v2>(extra_attempts)),
        preference_entries(preference_bound) {}

    bool prefers_v6(const std::string &host) const
    {
      const auto found = preferences.find(host);
      return found != preferences.end() && found->second.v6;
    }

    void prefer(const std::string &host, bool v6)
    {
      const auto found = preferences.find(host);
      if (found != preferences.end())
        preference_age.erase(found->second.recorded);
      else if (preferences.size() >= preference_entries && !preference_age.empty())
      {
        preferences.erase(preference_age.begin()->second);
        preference_age.erase(preference_age.begin());
      }
      auto &entry = preferences[host];
      entry.v6 = v6;
      entry.recorded = ++preference_clock;
      preference_age[entry.recorded] = host;
    }

    void release()
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (in_flight != 0)
        --in_flight;
    }

    void remember(const std::string &host, uint64_t epoch,
        std::vector<boost::asio::ip::address> addresses)
    {
      if (addresses.empty())
        return;
      if (!resolved_hosts.empty() && epoch < resolved_epoch)
        return; // no request of an earlier epoch can use it any more
      if (resolved_hosts.empty() || epoch > resolved_epoch)
      {
        // remembered() matches the epoch exactly: earlier entries are dead.
        resolved_hosts.clear();
        resolved_epoch = epoch;
      }
      const auto found = resolved_hosts.find(host);
      if (found == resolved_hosts.end()
          && resolved_hosts.size() >= EPOSE_V2_RESOLVED_HOST_ENTRIES)
        return;
      auto &entry = resolved_hosts[host];
      entry.epoch = epoch;
      entry.addresses = std::move(addresses);
    }

    uint64_t wait_for_lookup(lookup_resume resume)
    {
      const uint64_t id = ++next_waiter;
      lookup_waiters.emplace_back(id, std::move(resume));
      return id;
    }

    // Detaches one waiter. Returns the lookup's cancel when it was the last
    // waiter of a lookup still pending; the caller runs it after finishing.
    epose_network_v2::cancel_resolve leave_lookup(uint64_t id)
    {
      lookup_waiters.erase(std::remove_if(lookup_waiters.begin(), lookup_waiters.end(),
          [id](const std::pair<uint64_t, lookup_resume> &waiter) { return waiter.first == id; }),
          lookup_waiters.end());
      if (!resolving || lookup_cancelled || !lookup_waiters.empty())
        return {};
      lookup_cancelled = true;
      auto cancel = std::move(lookup_cancel);
      lookup_cancel = {};
      return cancel;
    }

    const std::vector<boost::asio::ip::address> *remembered(
        const std::string &host, uint64_t epoch) const
    {
      const auto found = resolved_hosts.find(host);
      if (found == resolved_hosts.end() || found->second.epoch != epoch)
        return nullptr;
      return &found->second.addresses;
    }
  };

  // Drops addresses the dial policy forbids, preserving resolver order.
  std::vector<boost::asio::ip::address> dial_order(std::vector<boost::asio::ip::address> found)
  {
    found.erase(std::remove_if(found.begin(), found.end(),
        [](const boost::asio::ip::address &address) { return !public_epose_dial_address(address); }),
        found.end());
    return found;
  }

  // One HTTP/1.1 POST over a fresh connection, on the worker thread. Late
  // completions fail the phase and stage id check and are ignored.
  struct http_exchange : std::enable_shared_from_this<http_exchange>
  {
    using done_handler = std::function<void(receipt_step_result_v2, std::string)>;
    enum class phase { idle, dns, connect, send, recv, done };

    epose_receipt_client_state &client;
    const std::string host;
    const uint16_t port;
    const std::string uri;
    const std::string body;
    const size_t max_body;
    const epose_stage_deadlines_v2 deadlines;
    const uint64_t epoch; // service epoch of the request; tags remembered resolutions
    done_handler done;

    phase state = phase::idle;
    uint64_t stage_id = 0;
    steady::time_point deadline{};
    boost::asio::steady_timer timer;
    epose_network_v2::cancel_operation cancel_op;
    std::shared_ptr<bool> dns_completed; // this exchange's lookup resume ran
    uint64_t lookup_waiter = 0;          // id in client.lookup_waiters, 0 = none
    std::vector<boost::asio::ip::address> addresses;
    // Connect attempts of the owning job, shared by its exchanges.
    std::shared_ptr<qwertycoin::epose::receipt_dial_counters_v2> dial_counters =
        std::make_shared<qwertycoin::epose::receipt_dial_counters_v2>();
    std::shared_ptr<epose_dial_report_v2> dial_report; // of the current dial

    // dial_order, counting each removed address as prohibited.
    std::vector<boost::asio::ip::address> checked(std::vector<boost::asio::ip::address> found)
    {
      for (const auto &address : found)
        if (!public_epose_dial_address(address))
          ++(address.is_v6() ? dial_counters->ipv6 : dial_counters->ipv4).prohibited;
      return dial_order(std::move(found));
    }
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    std::string request;
    std::vector<char> chunk;
    std::string received;
    bool headers_done = false;
    bool has_length = false;
    uint64_t content_length = 0;
    // Receive bounds; see EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S.
    steady::time_point recv_started{};
    steady::time_point recv_limit{};     // total bound, already clipped to the round
    uint64_t recv_bytes = 0;             // response bytes so far, headers included
    // (time, recv_bytes) samples from the first byte on, at least
    // (recv_idle + recv_rate_window) / 16 apart, so at most about 17 are kept.
    std::deque<std::pair<steady::time_point, uint64_t>> recv_samples;

    http_exchange(epose_receipt_client_state &owner, std::string target_host,
        uint16_t target_port, std::string target_uri, std::string payload,
        size_t limit, const epose_stage_deadlines_v2 &stage_deadlines, uint64_t service_epoch)
      : client(owner), host(std::move(target_host)), port(target_port),
        uri(std::move(target_uri)), body(std::move(payload)), max_body(limit),
        deadlines(stage_deadlines), epoch(service_epoch), timer(owner.io) {}

    void arm(steady::time_point at)
    {
      deadline = at;
      const uint64_t id = ++stage_id;
      timer.expires_at(at);
      auto self = shared_from_this();
      timer.async_wait([self, id](const boost::system::error_code &error) {
        if (error || id != self->stage_id)
          return;
        self->timed_out();
      });
    }

    void finish(receipt_step_result_v2 result, std::string payload = {})
    {
      if (state == phase::done)
        return;
      state = phase::done;
      ++stage_id;
      timer.cancel();
      cancel_op = {};
      // Only the last job to give up on the lookup cancels it, after finishing.
      const auto cancel_lookup = leave_lookup();
      if (socket)
      {
        boost::system::error_code ignored;
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        socket->close(ignored);
      }
      auto handler = std::move(done);
      done = {};
      if (handler)
        boost::asio::post(client.io, [handler, result, payload] { handler(result, payload); });
      if (cancel_lookup)
        cancel_lookup();
    }

    void start()
    {
      if (steady::now() >= deadlines.round_deadline)
      {
        finish({receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::deadline_expired});
        return;
      }
      // DNS and every dial attempt share this deadline; no slot wait is charged.
      arm(deadlines.stage(deadlines.connect));
      boost::system::error_code error;
      const auto literal = boost::asio::ip::make_address(host, error);
      if (!error)
      {
        addresses = checked({literal});
        if (addresses.empty())
        {
          finish({receipt_attempt_stage_v2::network_connect,
              receipt_failure_reason_v2::address_prohibited});
          return;
        }
        begin_connect();
        return;
      }
      if (client.resolving)
      {
        // Never queue behind the pending lookup of another host: it may be a
        // getaddrinfo that does not return until long after its own deadline.
        const auto *known = client.remembered(host, epoch);
        if (known)
        {
          addresses = *known;
          begin_connect();
          return;
        }
        if (client.resolving_host == host && !client.lookup_cancelled)
        {
          // Attach to this host's lookup, under this job's own deadline.
          state = phase::dns;
          attach_to_lookup();
          return;
        }
        finish({receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::dns_slot_busy});
        return;
      }
      start_dns();
    }

    void attach_to_lookup()
    {
      const auto token = std::make_shared<bool>(false);
      dns_completed = token;
      auto self = shared_from_this();
      auto *owner = &client;
      lookup_waiter = client.wait_for_lookup([self, token, owner](
          const boost::system::error_code &error,
          const std::vector<boost::asio::ip::address> &found) {
        boost::asio::post(owner->io, [self, token, error, found] {
          self->resolved(token, error, found);
        });
      });
    }

    // Leaves the pending lookup; returns its cancel if this was its last waiter.
    epose_network_v2::cancel_resolve leave_lookup()
    {
      if (lookup_waiter == 0)
        return {};
      const uint64_t id = lookup_waiter;
      lookup_waiter = 0;
      return client.leave_lookup(id);
    }

    void start_dns()
    {
      if (steady::now() >= deadline)
      {
        finish(expired(receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::dns_timeout));
        return;
      }
      state = phase::dns;
      client.resolving = true;
      client.resolving_host = host;
      client.lookup_cancelled = false;
      client.lookup_cancel = {};
      attach_to_lookup();
      const auto lookup = std::make_shared<bool>(false);
      auto self = shared_from_this();
      auto *owner = &client;
      const epose_network_v2::resolve_handler complete =
          [self, lookup, owner](boost::system::error_code error,
              std::vector<boost::asio::ip::address> found) {
        if (*lookup)
          return;
        *lookup = true;
        owner->resolving = false;
        owner->resolving_host.clear();
        owner->lookup_cancel = {};
        // Remember the answer even when every waiter already gave up.
        if (!error)
          owner->remember(self->host, self->epoch, dial_order(found));
        auto waiters = std::move(owner->lookup_waiters);
        owner->lookup_waiters.clear();
        for (auto &waiter : waiters)
          waiter.second(error, found);
      };
      try
      {
        auto cancel = client.network.resolve
            ? client.network.resolve(host, client.io, complete)
            : resolve_host(host, client.io, complete);
        if (!*lookup)
          client.lookup_cancel = std::move(cancel);
      }
      catch (...)
      {
        if (!*lookup)
        {
          *lookup = true;
          client.resolving = false;
          client.resolving_host.clear();
          client.lookup_waiters.clear();
        }
        lookup_waiter = 0;
        finish({receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::transport_failure});
      }
    }

    void resolved(const std::shared_ptr<bool> &token, const boost::system::error_code &error,
        std::vector<boost::asio::ip::address> found)
    {
      if (state != phase::dns || token != dns_completed)
        return;
      lookup_waiter = 0; // the completion already emptied the waiter list
      cancel_op = {};
      if (steady::now() >= deadline)
      {
        finish(expired(receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::dns_timeout));
        return;
      }
      if (error || found.empty())
      {
        finish({receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::dns_resolution_failed});
        return;
      }
      found = checked(std::move(found));
      if (found.empty())
      {
        finish({receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::address_prohibited});
        return;
      }
      addresses = std::move(found);
      begin_connect();
    }

    void begin_connect()
    {
      state = phase::connect;
      addresses = epose_happy_eyeballs_order_v2(addresses, client.prefers_v6(host));
      const uint64_t id = stage_id;
      auto self = shared_from_this();
      auto *owner = &client;
      const epose_network_v2::dialer dial = client.network.dial
          ? client.network.dial
          : epose_network_v2::dialer(&happy_eyeballs_epose_dial_v2);
      epose_network_v2::dial_request request{};
      request.addresses = addresses;
      request.port = port;
      request.deadline = deadline;
      request.attempt_delay = deadlines.connect_attempt_delay;
      request.connect = client.network.connect
          ? client.network.connect
          : epose_network_v2::connect_function(&connect_epose_address);
      request.budget = client.dial_budget;
      dial_report = std::make_shared<epose_dial_report_v2>();
      request.report = dial_report;
      request.counters = dial_counters;
      const epose_network_v2::connect_handler complete =
          [self, id, owner](boost::system::error_code error, boost::asio::ip::tcp::socket connected) {
        const auto shared = std::make_shared<boost::asio::ip::tcp::socket>(std::move(connected));
        boost::asio::post(owner->io, [self, id, error, shared] {
          self->connected(id, error, shared);
        });
      };
      try
      {
        auto cancel = dial(std::move(request), client.io, complete);
        if (state == phase::connect && id == stage_id)
          cancel_op = std::move(cancel);
      }
      catch (const boost::system::system_error &e)
      {
        finish({receipt_attempt_stage_v2::network_connect, classify_connect_error(e.code())});
      }
      catch (...)
      {
        finish({receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::transport_failure});
      }
    }

    void connected(uint64_t id, const boost::system::error_code &error,
        const std::shared_ptr<boost::asio::ip::tcp::socket> &connected_socket)
    {
      if (state != phase::connect || id != stage_id)
      {
        boost::system::error_code ignored;
        connected_socket->close(ignored);
        return;
      }
      cancel_op = {};
      // A failure keeps its own classification even when handled after the
      // deadline; only a connection that arrives too late is a timeout.
      if (error)
      {
        if (error == boost::asio::error::access_denied
            && dial_report->prohibited && !dial_report->failure)
        {
          finish({receipt_attempt_stage_v2::network_connect,
              receipt_failure_reason_v2::address_prohibited});
          return;
        }
        const auto reason = classify_connect_error(error);
        // As in timed_out(): a dial the deadline cut short ran out of time.
        finish(reason == receipt_failure_reason_v2::connect_timeout
                || dial_report->expired
            ? expired(receipt_attempt_stage_v2::network_connect, reason)
            : receipt_step_result_v2{receipt_attempt_stage_v2::network_connect, reason});
        return;
      }
      if (steady::now() >= deadline)
      {
        boost::system::error_code ignored;
        connected_socket->close(ignored);
        finish(expired(receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::connect_timeout));
        return;
      }
      socket = connected_socket;
      remember_family();
      begin_send();
    }

    // Records the winning family when the candidates offered both.
    void remember_family()
    {
      if (!dial_report->connected)
        return;
      const bool v4 = std::any_of(addresses.begin(), addresses.end(),
          [](const boost::asio::ip::address &address) { return address.is_v4(); });
      const bool v6 = std::any_of(addresses.begin(), addresses.end(),
          [](const boost::asio::ip::address &address) { return address.is_v6(); });
      if (v4 && v6)
        client.prefer(host, dial_report->winner.is_v6());
    }

    void begin_send()
    {
      if (steady::now() >= deadlines.round_deadline)
      {
        finish({receipt_attempt_stage_v2::network_send,
            receipt_failure_reason_v2::deadline_expired});
        return;
      }
      state = phase::send;
      arm(deadlines.stage(deadlines.send));
      const bool v6_literal = host.find(':') != std::string::npos;
      request = "POST " + uri + " HTTP/1.1\r\nHost: "
          + (v6_literal ? "[" + host + "]" : host) + ":" + std::to_string(port)
          + "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: "
          + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
      const uint64_t id = stage_id;
      auto self = shared_from_this();
      boost::asio::async_write(*socket, boost::asio::buffer(request),
          [self, id](const boost::system::error_code &error, size_t) {
        if (self->state != phase::send || id != self->stage_id)
          return;
        if (error)
        {
          self->finish({receipt_attempt_stage_v2::network_send,
              receipt_failure_reason_v2::transport_failure});
          return;
        }
        self->begin_recv();
      });
    }

    void begin_recv()
    {
      if (steady::now() >= deadlines.round_deadline)
      {
        finish({receipt_attempt_stage_v2::network_recv,
            receipt_failure_reason_v2::deadline_expired});
        return;
      }
      state = phase::recv;
      recv_started = steady::now();
      recv_limit = recv_bound(max_body + EPOSE_V2_MAX_RESPONSE_HEADER_BYTES);
      arm_recv();
      chunk.resize(EPOSE_V2_READ_CHUNK_BYTES);
      read_more();
    }

    // recv_started + the total receive bound for a response of `bytes`,
    // clipped to the round deadline.
    steady::time_point recv_bound(uint64_t bytes) const
    {
      const uint64_t total = epose_receipt_recv_total_ms_v2(bytes,
          static_cast<uint64_t>(std::max<int64_t>(deadlines.recv_idle.count(), 0)),
          deadlines.recv_min_rate);
      if (deadlines.round_deadline <= recv_started)
        return deadlines.round_deadline;
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
          deadlines.round_deadline - recv_started);
      if (total >= static_cast<uint64_t>(left.count()))
        return deadlines.round_deadline;
      return recv_started + std::chrono::milliseconds(total);
    }

    // The inactivity bound, capped by the total receive bound.
    void arm_recv()
    {
      arm(std::min(deadlines.stage(deadlines.recv_idle), recv_limit));
    }

    // Progress re-arms the inactivity bound.
    void read_on(size_t progress)
    {
      if (progress != 0)
        arm_recv();
      read_more();
    }

    // The progress floor; see EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S. With
    // `backlog` (the read filled the chunk or the socket holds more) the
    // verdict waits for the next read, so queued bytes count. False when the
    // peer is below the floor.
    bool fast_enough(size_t bytes, bool backlog)
    {
      if (deadlines.recv_min_rate == 0 || bytes == 0)
        return true;
      const auto now = steady::now();
      if (recv_samples.empty())
        recv_samples.emplace_back(now, 0);
      recv_bytes += bytes;
      const auto window = deadlines.recv_rate_window;
      if (window.count() <= 0)
        return true;
      const auto span = window + std::max(deadlines.recv_idle, std::chrono::milliseconds(0));
      if (now - recv_samples.back().first >= span / 16)
        recv_samples.emplace_back(now, recv_bytes);
      const auto horizon = now - span;
      while (recv_samples.size() > 1 && recv_samples[1].first <= horizon)
        recv_samples.pop_front();
      if (recv_samples.front().first > horizon)
        return true; // less than one span of history yet
      const uint64_t window_ms = static_cast<uint64_t>(window.count());
      const uint64_t need = deadlines.recv_min_rate > std::numeric_limits<uint64_t>::max() / window_ms
          ? std::numeric_limits<uint64_t>::max() : deadlines.recv_min_rate * window_ms / 1000;
      return recv_bytes - recv_samples.front().second >= need || backlog;
    }

    void read_more()
    {
      const uint64_t id = stage_id;
      auto self = shared_from_this();
      socket->async_read_some(boost::asio::buffer(chunk),
          [self, id](const boost::system::error_code &error, size_t bytes) {
        self->on_read(id, error, bytes);
      });
    }

    receipt_step_result_v2 parse_head(const std::string &head)
    {
      const receipt_step_result_v2 malformed{receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed};
      const size_t status_end = std::min(head.find("\r\n"), head.size());
      const std::string status = head.substr(0, status_end);
      // HTTP/1.x SP 3DIGIT [SP reason]
      if (status.size() < 12 || status.compare(0, 7, "HTTP/1.") != 0
          || (status[7] != '0' && status[7] != '1') || status[8] != ' '
          || !std::isdigit(static_cast<unsigned char>(status[9]))
          || !std::isdigit(static_cast<unsigned char>(status[10]))
          || !std::isdigit(static_cast<unsigned char>(status[11]))
          || (status.size() > 12 && status[12] != ' '))
        return malformed;
      const int code = (status[9] - '0') * 100 + (status[10] - '0') * 10 + (status[11] - '0');
      size_t position = status_end + 2;
      while (position < head.size())
      {
        const size_t line_end = std::min(head.find("\r\n", position), head.size());
        const std::string line = head.substr(position, line_end - position);
        position = line_end + 2;
        const size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0)
          return malformed;
        const std::string name = lower_ascii(line.substr(0, colon));
        if (name.find_first_of(" \t") != std::string::npos)
          return malformed;
        const std::string value = trim_ows(line.substr(colon + 1));
        // No transfer coding, chunked included, is supported.
        if (name == "transfer-encoding")
          return malformed;
        if (name == "content-length")
        {
          if (value.empty() || value.size() > 19
              || value.find_first_not_of("0123456789") != std::string::npos)
            return malformed;
          const uint64_t length = std::stoull(value);
          if (has_length && length != content_length)
            return malformed;
          has_length = true;
          content_length = length;
        }
      }
      if (code != 200)
        return {receipt_attempt_stage_v2::http_exchange,
            receipt_failure_reason_v2::http_status_unsuccessful};
      if (has_length && content_length > max_body)
        return {receipt_attempt_stage_v2::response_decode,
            receipt_failure_reason_v2::response_oversized};
      return {};
    }

    void on_read(uint64_t id, const boost::system::error_code &error, size_t bytes)
    {
      if (state != phase::recv || id != stage_id)
        return;
      const bool total_empty = received.empty() && !headers_done;
      if (bytes != 0)
        received.append(chunk.data(), bytes);
      const bool eof = error == boost::asio::error::eof;
      if (error && !eof)
      {
        finish({receipt_attempt_stage_v2::network_recv,
            receipt_failure_reason_v2::transport_failure});
        return;
      }
      boost::system::error_code ignored;
      const bool backlog = bytes == chunk.size() || (!eof && socket->available(ignored) > 0);
      if (!fast_enough(bytes, backlog))
      {
        finish({receipt_attempt_stage_v2::network_recv,
            receipt_failure_reason_v2::recv_too_slow});
        return;
      }
      if (!headers_done)
      {
        const size_t end = received.find("\r\n\r\n");
        if (end == std::string::npos)
        {
          if (received.size() > EPOSE_V2_MAX_RESPONSE_HEADER_BYTES)
          {
            finish({receipt_attempt_stage_v2::response_decode,
                receipt_failure_reason_v2::response_malformed});
            return;
          }
          if (eof)
          {
            if (total_empty && bytes == 0)
              finish({receipt_attempt_stage_v2::network_recv,
                  receipt_failure_reason_v2::transport_failure});
            else
              finish({receipt_attempt_stage_v2::response_decode,
                  receipt_failure_reason_v2::response_malformed});
            return;
          }
          read_on(bytes);
          return;
        }
        if (end + 4 > EPOSE_V2_MAX_RESPONSE_HEADER_BYTES)
        {
          finish({receipt_attempt_stage_v2::response_decode,
              receipt_failure_reason_v2::response_malformed});
          return;
        }
        const auto head = parse_head(received.substr(0, end));
        if (!head.accepted())
        {
          finish(head);
          return;
        }
        received.erase(0, end + 4);
        headers_done = true;
        if (has_length)
        {
          // The declared size tightens the total bound; parse_head capped it.
          recv_limit = std::min(recv_limit, recv_bound(end + 4 + content_length));
          arm_recv();
        }
      }
      if (has_length)
      {
        if (received.size() >= content_length)
        {
          received.resize(static_cast<size_t>(content_length));
          finish({}, std::move(received));
          return;
        }
        if (eof)
        {
          finish({receipt_attempt_stage_v2::network_recv,
              receipt_failure_reason_v2::transport_failure});
          return;
        }
      }
      else
      {
        if (received.size() > max_body)
        {
          finish({receipt_attempt_stage_v2::response_decode,
              receipt_failure_reason_v2::response_oversized});
          return;
        }
        if (eof)
        {
          finish({}, std::move(received));
          return;
        }
      }
      read_on(bytes);
    }

    // A stage deadline that is the round deadline means the round ran out,
    // not that the peer was slow.
    receipt_step_result_v2 expired(receipt_attempt_stage_v2 stage,
        receipt_failure_reason_v2 timeout) const
    {
      return {stage, deadline >= deadlines.round_deadline
          ? receipt_failure_reason_v2::deadline_expired : timeout};
    }

    void timed_out()
    {
      switch (state)
      {
      case phase::dns:
        finish(expired(receipt_attempt_stage_v2::network_connect,
            receipt_failure_reason_v2::dns_timeout));
        break;
      case phase::connect:
      {
        // A failed attempt says more than a pending one; the round deadline
        // still wins over every reason.
        auto cancel = std::move(cancel_op);
        const auto failure = dial_report->failure;
        finish(expired(receipt_attempt_stage_v2::network_connect, failure
            ? classify_connect_error(failure) : receipt_failure_reason_v2::connect_timeout));
        if (cancel)
          cancel();
        break;
      }
      case phase::send:
        finish(expired(receipt_attempt_stage_v2::network_send,
            receipt_failure_reason_v2::send_timeout));
        break;
      case phase::recv:
        finish(expired(receipt_attempt_stage_v2::network_recv,
            deadline >= recv_limit ? receipt_failure_reason_v2::recv_too_slow
                : receipt_failure_reason_v2::recv_timeout));
        break;
      default:
        break;
      }
    }

    void abort()
    {
      if (state == phase::done)
        return;
      auto cancel = state == phase::connect ? std::move(cancel_op)
          : epose_network_v2::cancel_operation{};
      finish({receipt_attempt_stage_v2::none, receipt_failure_reason_v2::cancelled});
      if (cancel)
        cancel();
    }
  };

  struct receipt_job : std::enable_shared_from_this<receipt_job>
  {
    using step = void (receipt_job::*)(receipt_step_result_v2, std::string);

    epose_receipt_client_state &client;
    const std::shared_ptr<epose_receipt_job_state> handle;
    const epose_receipt_request_v2 request;
    epose_receipt_exchange_v2 out{};
    // The most informative discovery failure so far; empty until a URL was
    // tried, so a real descriptor_unavailable differs from "nothing usable".
    boost::optional<receipt_step_result_v2> best;
    size_t next_url = 0;
    std::shared_ptr<http_exchange> current;
    std::shared_ptr<qwertycoin::epose::receipt_dial_counters_v2> dial =
        std::make_shared<qwertycoin::epose::receipt_dial_counters_v2>();
    bool finished = false;

    receipt_job(epose_receipt_client_state &owner,
        std::shared_ptr<epose_receipt_job_state> state, epose_receipt_request_v2 value)
      : client(owner), handle(std::move(state)), request(std::move(value)) {}

    void finish(receipt_step_result_v2 result)
    {
      if (finished)
        return;
      finished = true;
      auto exchange = std::move(current);
      current.reset();
      if (exchange)
        exchange->abort(); // counts its abandoned attempts first
      out.dial = *dial;
      out.result = result;
      if (!result.accepted())
        out.response = {};
      client.release();
      handle->complete(std::move(out));
    }

    void cancel()
    {
      finish({receipt_attempt_stage_v2::none, receipt_failure_reason_v2::cancelled});
    }

    template<class F>
    void guarded(F &&f)
    {
      try
      {
        f();
      }
      catch (...)
      {
        finish({receipt_attempt_stage_v2::none,
            receipt_failure_reason_v2::internal_execution_failure});
      }
    }

    void exchange(const std::string &host, uint16_t port, const char *uri,
        std::string body, size_t limit, step next)
    {
      const auto pending = std::make_shared<http_exchange>(client, host, port, uri,
          std::move(body), limit, request.deadlines, request.challenge.epoch);
      pending->dial_counters = dial;
      auto self = shared_from_this();
      pending->done = [self, next](receipt_step_result_v2 result, std::string payload) {
        if (self->finished)
          return;
        self->current.reset();
        self->guarded([&] { ((*self).*next)(result, std::move(payload)); });
      };
      current = pending;
      pending->start();
    }

    void run()
    {
      guarded([this] {
        handle->runner = shared_from_this();
        client.jobs.erase(std::remove_if(client.jobs.begin(), client.jobs.end(),
            [](const std::weak_ptr<receipt_job> &job) { return job.expired(); }),
            client.jobs.end());
        client.jobs.push_back(shared_from_this());
        if (handle->cancel_requested.load())
        {
          cancel();
          return;
        }
        if (!request.endpoint_known)
        {
          next_lookup();
          return;
        }
        out.endpoint = request.endpoint;
        if (request.bind_to_challenge)
        {
          if (request.endpoint.service_public_key != request.challenge.subject_public_key)
          {
            finish({receipt_attempt_stage_v2::descriptor_validation,
                receipt_failure_reason_v2::response_identity_mismatch});
            return;
          }
          if (qwertycoin::epose::hash_endpoint_descriptor_v2(
                  request.parameters.nettype, request.parameters.genesis_hash,
                  request.parameters.parameter_set_hash, request.endpoint)
              != request.challenge.endpoint_descriptor_hash)
          {
            finish({receipt_attempt_stage_v2::descriptor_validation,
                receipt_failure_reason_v2::descriptor_commitment_mismatch});
            return;
          }
        }
        if (request.descriptor_only)
        {
          finish({});
          return;
        }
        start_service();
      });
    }

    void next_lookup()
    {
      while (next_url < request.discovery.size())
      {
        const std::string &url = request.discovery[next_url++];
        std::string host;
        uint16_t port = 0;
        if (!parse_epose_discovery_url(url, host, port))
          continue; // not an attempt: best is unchanged
        cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::request lookup{};
        lookup.descriptor_hash =
            epee::string_tools::pod_to_hex(request.challenge.endpoint_descriptor_hash);
        std::string body;
        if (!epee::serialization::store_t_to_json(lookup, body))
        {
          best = receipt_step_result_v2{receipt_attempt_stage_v2::http_exchange,
              receipt_failure_reason_v2::internal_execution_failure};
          continue;
        }
        exchange(host, port, "/get_epose_service_endpoint_v2", std::move(body),
            EPOSE_V2_MAX_DESCRIPTOR_RESPONSE_BYTES, &receipt_job::looked_up);
        return;
      }
      finish(best ? *best : receipt_step_result_v2{receipt_attempt_stage_v2::descriptor_lookup,
          receipt_failure_reason_v2::descriptor_unavailable});
    }

    void looked_up(receipt_step_result_v2 result, std::string payload)
    {
      qwertycoin::epose::endpoint_descriptor_v2 candidate{};
      if (result.accepted())
        result = decode_descriptor(payload, request.parameters,
            request.challenge.endpoint_descriptor_hash, candidate);
      if (result.accepted() && request.bind_to_challenge
          && candidate.service_public_key != request.challenge.subject_public_key)
        result = {receipt_attempt_stage_v2::descriptor_validation,
            receipt_failure_reason_v2::response_identity_mismatch};
      if (!result.accepted())
      {
        // Any failure falls through to the next URL. dns_slot_busy says
        // nothing about the peer: it stands in only while nothing else was
        // seen, and any real failure replaces it. Among real failures the
        // latest wins, but descriptor_unavailable never replaces a more
        // specific one.
        const auto reason = result.reason;
        const bool busy = reason == receipt_failure_reason_v2::dns_slot_busy;
        if (!best || best->reason == receipt_failure_reason_v2::dns_slot_busy
            || (!busy && (reason != receipt_failure_reason_v2::descriptor_unavailable
                || best->reason == receipt_failure_reason_v2::descriptor_unavailable)))
          best = result;
        next_lookup();
        return;
      }
      out.endpoint = std::move(candidate);
      if (request.descriptor_only)
      {
        finish({});
        return;
      }
      start_service();
    }

    void start_service()
    {
      const auto &challenge = request.challenge;
      cryptonote::COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::request service{};
      service.version = challenge.version;
      service.service_kind = challenge.service_kind;
      service.epoch = challenge.epoch;
      service.round = challenge.round;
      service.snapshot_hash = epee::string_tools::pod_to_hex(challenge.snapshot_hash);
      service.anchor_hash = epee::string_tools::pod_to_hex(challenge.anchor_hash);
      service.subject_public_key = epee::string_tools::pod_to_hex(challenge.subject_public_key);
      service.verifier_public_key = epee::string_tools::pod_to_hex(challenge.verifier_public_key);
      service.endpoint_descriptor_hash = epee::string_tools::pod_to_hex(challenge.endpoint_descriptor_hash);
      service.nonce = epee::string_tools::pod_to_hex(challenge.nonce);
      service.requested_object_hash = epee::string_tools::pod_to_hex(challenge.requested_object_hash);
      std::string body;
      if (!epee::serialization::store_t_to_json(service, body))
      {
        finish({receipt_attempt_stage_v2::http_exchange,
            receipt_failure_reason_v2::internal_execution_failure});
        return;
      }
      exchange(out.endpoint.host, out.endpoint.port, "/epose_service_challenge_v2",
          std::move(body), EPOSE_V2_MAX_SERVICE_RESPONSE_BYTES, &receipt_job::served);
    }

    void served(receipt_step_result_v2 result, std::string payload)
    {
      if (result.accepted())
        result = decode_service_response(payload, out.response);
      finish(result);
    }
  };
} // namespace detail

  epose_receipt_job_v2::epose_receipt_job_v2(
      std::shared_ptr<detail::epose_receipt_job_state> state)
    : state_(std::move(state))
  {
  }

  bool epose_receipt_job_v2::ready() const
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->done;
  }

  epose_receipt_exchange_v2 epose_receipt_job_v2::wait() const
  {
    std::unique_lock<std::mutex> lock(state_->mutex);
    state_->done_cv.wait(lock, [this] { return state_->done; });
    return state_->exchange;
  }

  void epose_receipt_job_v2::cancel() const
  {
    state_->cancel_requested.store(true);
    if (ready())
      return;
    const auto client = state_->client.lock();
    if (!client)
      return;
    const auto state = state_;
    boost::asio::post(client->io, [state] {
      const auto runner = state->runner.lock();
      if (runner)
        runner->cancel();
    });
  }

  epose_receipt_client_v2::epose_receipt_client_v2(
      epose_network_v2 network, const size_t capacity, const size_t extra_attempts,
      const size_t preference_entries)
    : state_(std::make_shared<detail::epose_receipt_client_state>(
          std::move(network), capacity, extra_attempts, preference_entries))
  {
    auto *state = state_.get();
    worker_ = std::thread([state] {
      for (;;)
      {
        try
        {
          state->io.run();
          return;
        }
        catch (const std::exception &e)
        {
          MERROR("EPoSE-v2 receipt executor handler failed: " << e.what());
        }
        catch (...)
        {
          MERROR("EPoSE-v2 receipt executor handler failed");
        }
      }
    });
  }

  epose_receipt_client_v2::~epose_receipt_client_v2()
  {
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      state_->stopped = true;
    }
    auto *state = state_.get();
    boost::asio::post(state->io, [state] {
      const auto jobs = std::move(state->jobs);
      state->jobs.clear();
      for (const auto &weak : jobs)
      {
        const auto job = weak.lock();
        if (job)
          job->cancel();
      }
      state->work.reset();
      state->io.stop();
    });
    if (worker_.joinable())
      worker_.join();
  }

  epose_receipt_client_v2 &epose_receipt_client_v2::shared()
  {
    static auto *client = new epose_receipt_client_v2;
    return *client;
  }

  size_t epose_receipt_client_v2::capacity() const
  {
    return state_->capacity;
  }

  size_t epose_receipt_client_v2::in_flight() const
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->in_flight;
  }

  epose_receipt_job_v2 epose_receipt_client_v2::submit(epose_receipt_request_v2 request)
  {
    const auto handle = std::make_shared<detail::epose_receipt_job_state>();
    handle->client = state_;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->stopped)
    {
      epose_receipt_exchange_v2 cancelled{};
      cancelled.result = {receipt_attempt_stage_v2::none, receipt_failure_reason_v2::cancelled};
      handle->complete(std::move(cancelled));
    }
    else if (state_->in_flight >= state_->capacity)
    {
      // A local admission limit, never a transport outcome.
      epose_receipt_exchange_v2 exhausted{};
      exhausted.result = {receipt_attempt_stage_v2::local_lease,
          receipt_failure_reason_v2::lease_exhausted};
      handle->complete(std::move(exhausted));
    }
    else
    {
      const auto job = std::make_shared<detail::receipt_job>(*state_, handle, std::move(request));
      ++state_->in_flight;
      boost::asio::post(state_->io, [job] { job->run(); });
    }
    return epose_receipt_job_v2(handle);
  }

  epose_receipt_job_v2 epose_receipt_client_v2::fetch_descriptor(const std::string &url,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const crypto::hash &required_hash, const epose_stage_deadlines_v2 &deadlines)
  {
    epose_receipt_request_v2 request{};
    request.parameters = parameters;
    request.challenge.endpoint_descriptor_hash = required_hash;
    request.discovery = {url};
    request.bind_to_challenge = false;
    request.descriptor_only = true;
    request.deadlines = deadlines;
    return submit(std::move(request));
  }

  epose_receipt_job_v2 epose_receipt_client_v2::request_service(
      const qwertycoin::epose::service_challenge_v2 &challenge,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const epose_stage_deadlines_v2 &deadlines)
  {
    epose_receipt_request_v2 request{};
    request.parameters = parameters;
    request.challenge = challenge;
    request.endpoint_known = true;
    request.endpoint = endpoint;
    request.bind_to_challenge = false;
    request.deadlines = deadlines;
    return submit(std::move(request));
  }
}
