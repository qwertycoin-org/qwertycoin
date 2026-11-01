#pragma once

#include "epose/resource_policy_v2.h"
#include "net/net_helper.h"

namespace qwertycoin
{
namespace epose
{
  inline epee::net_utils::direct_connect service_probe_connector_v2(
      const endpoint_descriptor_v2 &endpoint, uint8_t verifier_dial_capabilities,
      bool allow_fakechain_loopback = false)
  {
    epee::net_utils::direct_connect connector{};
    connector.allowed_families = permitted_dial_families_v2(
        endpoint, verifier_dial_capabilities);
    connector.max_resolved_endpoints = 16;
    boost::system::error_code host_error;
    const auto literal_host = boost::asio::ip::make_address(endpoint.host, host_error);
    const bool pin_loopback = allow_fakechain_loopback && !host_error && literal_host.is_loopback();
    connector.endpoint_allowed = [literal_host, port = endpoint.port,
                                  pin_loopback](const boost::asio::ip::tcp::endpoint &target) {
      if (pin_loopback)
        return target.address() == literal_host && target.port() == port;
      return public_probe_address_v2(target.address().to_string());
    };
    return connector;
  }
}
}
