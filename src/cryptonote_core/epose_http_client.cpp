// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose_http_client.h"

#include <algorithm>
#include "net/net_parse_helpers.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/http_abstract_invoke.h"
#include "string_tools.h"

namespace cryptonote
{
namespace
{
  constexpr size_t EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES = 2 * 1024 * 1024;
  constexpr uint64_t EPOSE_V2_PROBE_TIMEOUT_MS = 5000;

  uint64_t epose_v2_steady_milliseconds()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  boost::system::error_code prohibited_address_error()
  {
    struct policy_category final : boost::system::error_category
    {
      const char *name() const noexcept override { return "epose_dial_policy"; }
      std::string message(int) const override { return "prohibited endpoint address"; }
    };
    static const policy_category category;
    return {1, category};
  }

  struct transport_slot
  {
    std::mutex mutex;
    epee::net_utils::http::http_simple_client client;
    bool resolving = false;
  };

  transport_slot &transport()
  {
    // Deliberately process-lifetime: Asio's resolver service joins its worker
    // during io_context destruction, and libc getaddrinfo is not interruptible.
    // There is exactly one executor / resolver worker and at most one unresolved
    // operation, including cancelled operations. Never enqueue behind it.
    static auto *slot = new transport_slot;
    return *slot;
  }

  boost::asio::io_context &timer_context(boost::asio::steady_timer &timer)
  {
#if BOOST_VERSION >= 107000
    return static_cast<boost::asio::io_context &>(timer.get_executor().context());
#else
    return timer.get_io_context();
#endif
  }

  epose_network_v2::cancel_resolve resolve_host(const std::string &host,
      boost::asio::io_context &io, epose_network_v2::resolve_handler complete)
  {
    const auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(io);
    resolver->async_resolve(host, "0",
        [resolver, complete = std::move(complete)](const boost::system::error_code &error,
            boost::asio::ip::tcp::resolver::results_type results) {
      std::vector<boost::asio::ip::address> addresses;
      for (const auto &result : results)
        addresses.push_back(result.endpoint().address());
      complete(error, std::move(addresses));
    });
    return [resolver] { resolver->cancel(); };
  }

  std::vector<boost::asio::ip::address> resolve_addresses(const std::string &host,
      const decltype(epose_network_v2::resolve) &resolve, boost::asio::steady_timer &timeout)
  {
    auto &io = timer_context(timeout);
    const auto deadline = timeout.expiry();
    // Drain late completions before checking admission. The persistent HTTP
    // client's handlers remain alive; no handlers refer to a returned caller.
    io.restart();
    io.poll();
    if (transport().resolving || std::chrono::steady_clock::now() >= deadline)
      throw boost::system::system_error(boost::asio::error::timed_out);
    struct lookup
    {
      boost::asio::steady_timer timer;
      epose_network_v2::cancel_resolve cancel;
      std::vector<boost::asio::ip::address> addresses;
      boost::system::error_code error;
      bool done = false;
      bool completed = false;
      explicit lookup(boost::asio::io_context &io) : timer(io) {}
    };
    const auto pending = std::make_shared<lookup>(io);
    pending->timer.expires_at(deadline);
    pending->timer.async_wait([pending](const boost::system::error_code &error) {
      if (error || pending->done) return;
      pending->done = true;
      pending->error = boost::asio::error::timed_out;
      auto cancel = std::move(pending->cancel);
      if (cancel) cancel();
    });
    transport().resolving = true;
    const auto complete = [pending, deadline](boost::system::error_code error,
        std::vector<boost::asio::ip::address> addresses) {
      if (pending->completed) return;
      pending->completed = true;
      transport().resolving = false;
      if (pending->done) return; // Includes completions after the caller returned.
      pending->done = true;
      pending->cancel = {};
      pending->timer.cancel();
      pending->error = std::chrono::steady_clock::now() >= deadline
          ? boost::asio::error::timed_out : error;
      pending->addresses = std::move(addresses);
    };
    try
    {
      auto cancel = resolve ? resolve(host, io, complete) : resolve_host(host, io, complete);
      if (!pending->done) pending->cancel = std::move(cancel);
    }
    catch (...)
    {
      transport().resolving = false;
      pending->done = true;
      pending->completed = true;
      pending->timer.cancel();
      throw;
    }
    while (!pending->done)
    {
      io.restart();
      io.run_one();
    }
    if (pending->error == boost::asio::error::timed_out)
      throw boost::system::system_error(pending->error);
    if (pending->error || pending->addresses.empty())
      throw boost::system::system_error(boost::asio::error::host_not_found);
    return std::move(pending->addresses);
  }

  boost::unique_future<boost::asio::ip::tcp::socket> connect_addresses(
      const std::vector<boost::asio::ip::address> &addresses, uint16_t port,
      const decltype(epose_network_v2::connect) &connect, boost::asio::steady_timer &timeout)
  {
#if BOOST_VERSION >= 107000
    auto &io = static_cast<boost::asio::io_context &>(timeout.get_executor().context());
#else
    auto &io = timeout.get_io_context();
#endif
    const auto deadline = timeout.expiry();
    for (size_t i = 0; i < addresses.size(); ++i)
    {
      if (std::chrono::steady_clock::now() >= deadline)
        throw boost::system::system_error(boost::asio::error::timed_out);
      try
      {
        const boost::asio::ip::tcp::endpoint endpoint(addresses[i], port);
        auto pending = connect ? connect(endpoint, timeout) : connect_epose_address(endpoint, timeout);
        // The blocking HTTP client normally drives this same executor. Consume
        // each failed future here so the next checked endpoint can be attempted
        // without restarting the deadline or resolving the hostname again.
        while (!pending.is_ready())
        {
          io.restart();
          io.run_one();
        }
        if (std::chrono::steady_clock::now() >= deadline)
          throw boost::system::system_error(boost::asio::error::timed_out);
        if (pending.has_exception())
          pending.get();
        // try_connect runs one handler before checking the returned future.
        boost::asio::post(io, [] {});
        return pending;
      }
      catch (const boost::system::system_error &)
      {
        if (std::chrono::steady_clock::now() >= deadline)
          throw boost::system::system_error(boost::asio::error::timed_out);
        if (i + 1 == addresses.size())
          throw;
      }
    }
    throw boost::system::system_error(boost::asio::error::host_not_found);
  }
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

  boost::unique_future<boost::asio::ip::tcp::socket> connect_epose_address(
      const boost::asio::ip::tcp::endpoint &endpoint, boost::asio::steady_timer &timeout)
  {
    struct connection
    {
      boost::promise<boost::asio::ip::tcp::socket> result;
      boost::asio::ip::tcp::socket socket;
      boost::asio::steady_timer deadline;
      explicit connection(boost::asio::steady_timer &timer)
        : socket(MONERO_GET_EXECUTOR(timer)), deadline(MONERO_GET_EXECUTOR(timer))
      { deadline.expires_at(timer.expiry()); }
    };
    const auto pending = std::make_shared<connection>(timeout);
    // The HTTP client's deadline actor can reset its timer and cancel waits.
    // Own a wait at the same expiry so it cannot suppress socket cancellation.
    pending->deadline.async_wait([pending](const boost::system::error_code &error) {
      if (!error)
      {
        boost::system::error_code ignored;
        pending->socket.close(ignored);
      }
    });
    // Endpoint overload: no resolver, including on HTTP reconnect / SSL fallback.
    pending->socket.async_connect(endpoint, [pending](const boost::system::error_code &error) {
      pending->deadline.cancel();
      if (error)
        pending->result.set_exception(boost::system::system_error(error));
      else
        pending->result.set_value(std::move(pending->socket));
    });
    return pending->result.get_future();
  }

  epose_http_client::epose_http_client()
    : lease_(transport().mutex, std::try_to_lock) {}

  epose_http_client::~epose_http_client()
  {
    if (available())
    {
      transport().client.disconnect();
      // Drop injected functions and per-request addresses before returning.
      transport().client.set_connector(epee::net_utils::direct_connect{});
    }
  }

  epee::net_utils::http::http_simple_client &epose_http_client::client()
  {
    if (!available()) throw boost::system::system_error(boost::asio::error::timed_out);
    return transport().client;
  }

  qwertycoin::epose::receipt_step_result_v2 prepare_epose_http_client(
      epose_http_client &lease,
      const std::string &host, uint16_t port, const epose_network_v2 &network)
  {
    using namespace qwertycoin::epose;
    if (!lease.available())
      return {receipt_attempt_stage_v2::network_connect, receipt_failure_reason_v2::transport_timeout};
    boost::system::error_code error;
    const auto literal = boost::asio::ip::make_address(host, error);
    std::vector<boost::asio::ip::address> addresses;
    if (!error)
    {
      if (!public_epose_dial_address(literal))
        return {receipt_attempt_stage_v2::network_connect, receipt_failure_reason_v2::address_prohibited};
      addresses.push_back(literal);
    }
    auto &client = lease.client();
    client.set_server(host, std::to_string(port), boost::none);
    client.set_connector([addresses = std::move(addresses), host, port, network,
        deadline = std::chrono::steady_clock::time_point::max()](
        const std::string &, const std::string &, boost::asio::steady_timer &timeout) mutable {
      // DNS, address fallback and reconnect dials share the original deadline.
      deadline = std::min(deadline, timeout.expiry());
      timeout.expires_at(deadline);
      if (addresses.empty())
      {
        addresses = resolve_addresses(host, network.resolve, timeout);
        addresses.erase(std::remove_if(addresses.begin(), addresses.end(),
            [](const boost::asio::ip::address &address) { return !public_epose_dial_address(address); }),
            addresses.end());
        if (addresses.empty())
          throw boost::system::system_error(prohibited_address_error());
        // Preserve resolver order within each family and the IPv4 preference.
        std::stable_partition(addresses.begin(), addresses.end(),
            [](const boost::asio::ip::address &address) { return address.is_v4(); });
      }
      return connect_addresses(addresses, port, network.connect, timeout);
    });
    return {};
  }

  template<class Request, class Response>
  qwertycoin::epose::receipt_step_result_v2 invoke_epose_http_json(
      const boost::string_ref uri,
      const Request &request,
      Response &response,
      epee::net_utils::http::http_simple_client &client,
      const size_t max_response_bytes)
  {
    using namespace qwertycoin::epose;
    std::string body;
    if (!epee::serialization::store_t_to_json(request, body))
      return {receipt_attempt_stage_v2::http_exchange,
          receipt_failure_reason_v2::internal_execution_failure};
    epee::net_utils::http::fields_list headers;
    headers.emplace_back("Content-Type", "application/json; charset=utf-8");
    const epee::net_utils::http::http_response_info *http_response = nullptr;
    client.set_response_body_limit(max_response_bytes);
    const uint64_t started_ms = epose_v2_steady_milliseconds();
    if (!client.invoke(uri, "POST", body,
            std::chrono::milliseconds(EPOSE_V2_PROBE_TIMEOUT_MS),
            &http_response, std::move(headers)))
    {
      if (client.response_body_limit_exceeded())
        return {receipt_attempt_stage_v2::response_decode,
            receipt_failure_reason_v2::response_oversized};
      const uint64_t ended_ms = epose_v2_steady_milliseconds();
      const uint64_t duration_ms = ended_ms >= started_ms
          ? ended_ms - started_ms : 0;
      const boost::system::error_code &error = client.get_last_error();
      return {error ? receipt_attempt_stage_v2::network_connect
                    : receipt_attempt_stage_v2::http_exchange,
          error == prohibited_address_error()
              ? receipt_failure_reason_v2::address_prohibited
              : classify_transport_failure_v2(error, duration_ms, EPOSE_V2_PROBE_TIMEOUT_MS)};
    }
    if (http_response == nullptr)
      return {receipt_attempt_stage_v2::http_exchange,
          receipt_failure_reason_v2::internal_execution_failure};
    if (http_response->m_response_code != 200)
      return {receipt_attempt_stage_v2::http_exchange,
          receipt_failure_reason_v2::http_status_unsuccessful};
    if (http_response->m_body.size() > max_response_bytes)
      return {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_oversized};
    if (!epee::serialization::load_t_from_json(response, http_response->m_body))
      return {receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed};
    return {};
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

  qwertycoin::epose::receipt_step_result_v2 fetch_epose_endpoint_descriptor(
      const std::string &url,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const crypto::hash &required_hash,
      qwertycoin::epose::endpoint_descriptor_v2 &descriptor,
      const epose_network_v2 &network)
  {
    descriptor = {};
    std::string host;
    uint16_t port = 0;
    if (!parse_epose_discovery_url(url, host, port))
      return {qwertycoin::epose::receipt_attempt_stage_v2::descriptor_lookup,
          qwertycoin::epose::receipt_failure_reason_v2::descriptor_unavailable};
    epose_http_client client;
    const auto dial = prepare_epose_http_client(client, host, port, network);
    if (!dial.accepted())
      return dial;
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::request request{};
    request.descriptor_hash = epee::string_tools::pod_to_hex(required_hash);
    cryptonote::COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response response{};
    const auto http_result = invoke_epose_http_json(
        "/get_epose_service_endpoint_v2", request, response, client.client(), 4096);
    if (!http_result.accepted())
      return http_result;
    if (response.status != CORE_RPC_STATUS_OK || !response.ready)
      return {qwertycoin::epose::receipt_attempt_stage_v2::descriptor_lookup,
          qwertycoin::epose::receipt_failure_reason_v2::descriptor_unavailable};
    if (response.version > std::numeric_limits<uint8_t>::max()
        || response.transport > std::numeric_limits<uint8_t>::max()
        || response.port > std::numeric_limits<uint16_t>::max()
        || response.service_kind > std::numeric_limits<uint8_t>::max()
        || response.service_version > std::numeric_limits<uint8_t>::max())
      return {qwertycoin::epose::receipt_attempt_stage_v2::descriptor_validation,
          qwertycoin::epose::receipt_failure_reason_v2::descriptor_invalid};
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
      return {qwertycoin::epose::receipt_attempt_stage_v2::descriptor_validation,
          qwertycoin::epose::receipt_failure_reason_v2::descriptor_invalid};
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
      return {qwertycoin::epose::receipt_attempt_stage_v2::descriptor_validation,
          qwertycoin::epose::receipt_failure_reason_v2::descriptor_commitment_mismatch};
    }
    // Discovery is an authenticated directory: the signed endpoint may belong
    // to a third party. The next request dials descriptor.host/descriptor.port.
    if (qwertycoin::epose::validate_discovered_endpoint_v2(
            parameters.nettype, parameters.genesis_hash,
            parameters.parameter_set_hash, descriptor, required_hash)
        != qwertycoin::epose::resource_status_v2::accepted)
    {
      descriptor = {};
      return {qwertycoin::epose::receipt_attempt_stage_v2::descriptor_validation,
          qwertycoin::epose::receipt_failure_reason_v2::descriptor_invalid};
    }
    return {};
  }

  qwertycoin::epose::receipt_step_result_v2 request_epose_service_response(
      const qwertycoin::epose::service_challenge_v2 &challenge,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      qwertycoin::epose::canonical_service_response_v2 &service_response,
      const epose_network_v2 &network)
  {
    service_response = {};
    epose_http_client client;
    const auto dial = prepare_epose_http_client(
        client, endpoint.host, endpoint.port, network);
    if (!dial.accepted())
      return dial;
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
    const size_t max_http_response =
        EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES * 2 + 4096;
    const auto http_result = invoke_epose_http_json(
        "/epose_service_challenge_v2", request, response, client.client(),
        max_http_response);
    if (!http_result.accepted())
      return http_result;
    if (response.status != CORE_RPC_STATUS_OK)
      return {qwertycoin::epose::receipt_attempt_stage_v2::http_exchange,
          qwertycoin::epose::receipt_failure_reason_v2::remote_challenge_rejected};
    if (response.block_blob.size() % 2 != 0
        || response.block_blob.size() / 2
            > EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES)
      return {qwertycoin::epose::receipt_attempt_stage_v2::response_decode,
          qwertycoin::epose::receipt_failure_reason_v2::response_oversized};
    if (!epee::string_tools::parse_hexstr_to_binbuff(
            response.block_blob, service_response.block_blob)
        || !epee::string_tools::hex_to_pod(
            response.subject_signature, service_response.subject_signature))
    {
      service_response = {};
      return {qwertycoin::epose::receipt_attempt_stage_v2::response_decode,
          qwertycoin::epose::receipt_failure_reason_v2::response_malformed};
    }
    return {};
  }
}
