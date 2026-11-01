// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <mutex>

#include "net/http_client.h"
#include "epose/diagnostics_v2.h"
#include "epose/coordinator_v2.h"
#include "epose/resource_policy_v2.h"

namespace cryptonote
{
  // One nonblocking lease on a process-lifetime HTTP executor. Keeping the
  // executor alive lets a cancelled system getaddrinfo finish after the caller
  // returns without blocking io_context destruction. Concurrent leases fail
  // locally; the receipt producer already permits only one outstanding job.
  class epose_http_client
  {
    std::unique_lock<std::mutex> lease_;
    epee::net_utils::direct_connect connector_;
  public:
    epose_http_client();
    ~epose_http_client();
    void set_connector(epee::net_utils::direct_connect connector) { connector_ = std::move(connector); }
    const epee::net_utils::direct_connect &connector() const { return connector_; }
    bool available() const { return lease_.owns_lock(); }
    epee::net_utils::http::http_simple_client &client();
  };

  // Network dependencies for deterministic transport tests. Production uses the
  // system resolver and a direct socket connection to the checked endpoint.
  struct epose_network_v2
  {
    using resolve_handler = std::function<void(boost::system::error_code,
        std::vector<boost::asio::ip::address>)>;
    using cancel_resolve = std::function<void()>;
    // Must return promptly and invoke completion at most once on io (possibly
    // after cancellation). Cancellation must also return promptly. Late
    // completions must own their state; io outlives the caller. A pending lookup
    // occupies the sole resolver slot until completion, even after cancellation.
    // Throwing must leave no outstanding operation or future completion: the
    // caller releases admission on exception. Synchronous completion is allowed.
    std::function<cancel_resolve(const std::string &, boost::asio::io_context &,
        resolve_handler)> resolve;
    std::function<boost::unique_future<boost::asio::ip::tcp::socket>(
        const boost::asio::ip::tcp::endpoint &, boost::asio::steady_timer &)> connect;
  };

  bool public_epose_dial_address(const boost::asio::ip::address &address);
  boost::unique_future<boost::asio::ip::tcp::socket> connect_epose_address(
      const boost::asio::ip::tcp::endpoint &endpoint, boost::asio::steady_timer &timeout);
  qwertycoin::epose::receipt_step_result_v2 prepare_epose_http_client(
      epose_http_client &client,
      const std::string &host, uint16_t port, const epose_network_v2 &network = {});
  qwertycoin::epose::receipt_step_result_v2 fetch_epose_endpoint_descriptor(
      const std::string &url,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      const crypto::hash &required_hash,
      qwertycoin::epose::endpoint_descriptor_v2 &descriptor,
      const epose_network_v2 &network = {});
  qwertycoin::epose::receipt_step_result_v2 request_epose_service_response(
      const qwertycoin::epose::service_challenge_v2 &challenge,
      const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      qwertycoin::epose::canonical_service_response_v2 &service_response,
      const epose_network_v2 &network = {},
      uint8_t verifier_dial_capabilities = 0);
}
