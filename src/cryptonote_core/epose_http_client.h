// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include "net/http_client.h"
#include "epose/diagnostics_v2.h"
#include "epose/coordinator_v2.h"
#include "epose/resource_policy_v2.h"

namespace cryptonote
{
  // Network dependencies for deterministic transport tests. Production uses the
  // system resolver and a direct socket connection to the checked endpoint.
  struct epose_network_v2
  {
    std::function<std::vector<boost::asio::ip::address>(
        const std::string &, boost::system::error_code &)> resolve;
    std::function<boost::unique_future<boost::asio::ip::tcp::socket>(
        const boost::asio::ip::tcp::endpoint &, boost::asio::steady_timer &)> connect;
  };

  bool public_epose_dial_address(const boost::asio::ip::address &address);
  boost::unique_future<boost::asio::ip::tcp::socket> connect_epose_address(
      const boost::asio::ip::tcp::endpoint &endpoint, boost::asio::steady_timer &timeout);
  qwertycoin::epose::receipt_step_result_v2 prepare_epose_http_client(
      epee::net_utils::http::http_simple_client &client,
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
      const epose_network_v2 &network = {});
}
