// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>

#include "cryptonote_core/epose_http_client.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/http_abstract_invoke.h"
#include "string_tools.h"

namespace
{
  using namespace qwertycoin::epose;
  using namespace cryptonote;
  using boost::asio::ip::make_address;
  using boost::asio::ip::tcp;

  template<class Resolve>
  auto async_answers(Resolve resolve)
  {
    return [resolve](const std::string &host, boost::asio::io_context &io,
        epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
      boost::asio::post(io, [resolve, host, complete] {
        boost::system::error_code error;
        auto addresses = resolve(host, error);
        complete(error, std::move(addresses));
      });
      return [] {};
    };
  }

  consensus_parameters_v2 parameters()
  {
    consensus_parameters_v2 out{};
    out.nettype = TESTNET;
    out.genesis_hash = crypto::cn_fast_hash("genesis", 7);
    out.parameter_set_hash = crypto::cn_fast_hash("parameters", 10);
    return out;
  }

  endpoint_descriptor_v2 signed_descriptor(const std::string &host = "2a01:4f8:201:6016::547")
  {
    endpoint_descriptor_v2 out{};
    crypto::secret_key secret{};
    crypto::generate_keys(out.service_public_key, secret);
    out.host = host;
    boost::system::error_code error;
    const auto ip = make_address(host, error);
    out.transport = error ? endpoint_transport_v2::dns : ip.is_v4()
        ? endpoint_transport_v2::tcp_ipv4 : endpoint_transport_v2::tcp_ipv6;
    out.port = 8198;
    out.service_kind = 1;
    out.service_version = 1;
    out.sequence = 7;
    out.expiry_epoch = 3;
    const auto p = parameters();
    EXPECT_TRUE(sign_endpoint_descriptor_v2(p.nettype, p.genesis_hash,
        p.parameter_set_hash, out, secret));
    return out;
  }

  crypto::hash commitment(const endpoint_descriptor_v2 &descriptor)
  {
    const auto p = parameters();
    return hash_endpoint_descriptor_v2(p.nettype, p.genesis_hash, p.parameter_set_hash, descriptor);
  }

  std::string discovery_json(const endpoint_descriptor_v2 &d)
  {
    COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response out{};
    out.status = CORE_RPC_STATUS_OK;
    out.ready = true;
    out.version = d.version;
    out.transport = static_cast<uint8_t>(d.transport);
    out.host = d.host;
    out.port = d.port;
    out.service_kind = d.service_kind;
    out.service_version = d.service_version;
    out.sequence = d.sequence;
    out.expiry_epoch = d.expiry_epoch;
    out.service_public_key = epee::string_tools::pod_to_hex(d.service_public_key);
    out.signature = epee::string_tools::pod_to_hex(d.signature);
    out.descriptor_hash = epee::string_tools::pod_to_hex(commitment(d));
    std::string json;
    EXPECT_TRUE(epee::serialization::store_t_to_json(out, json));
    return json;
  }

  // A real loopback HTTP server. Only the test connector redirects to it;
  // production still checks the public DNS answer and pins that exact endpoint.
  class discovery_server
  {
    boost::asio::io_context io;
    tcp::acceptor acceptor{io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0)};
    std::thread worker;
    std::string reply;
    std::atomic<unsigned> requests{0};
    std::atomic<bool> request_valid{false};
    std::string expected_hash;

    void accept()
    {
      const auto socket = std::make_shared<tcp::socket>(io);
      acceptor.async_accept(*socket, [this, socket](const boost::system::error_code &error) {
        if (error) return;
        accept();
        const auto data = std::make_shared<boost::asio::streambuf>();
        // Read the first byte so TLS autodetection gets an immediate EOF and
        // reconnects with HTTP. Both connections must use the pinned address.
        boost::asio::async_read(*socket, *data, boost::asio::transfer_exactly(1),
            [this, socket, data](const boost::system::error_code &error, size_t) {
          if (error || *boost::asio::buffers_begin(data->data()) != 'P') return;
          boost::asio::async_read_until(*socket, *data, "\r\n\r\n",
              [this, socket, data](const boost::system::error_code &error, size_t header_size) {
            if (error) return;
            const std::string buffered(boost::asio::buffers_begin(data->data()),
                boost::asio::buffers_end(data->data()));
            const auto length_pos = buffered.find("Content-Length: ");
            if (length_pos == std::string::npos) return;
            const auto length = std::stoul(buffered.substr(length_pos + 16));
            const auto finish = [this, socket, data](const boost::system::error_code &error, size_t) {
              if (error) return;
              const std::string request(boost::asio::buffers_begin(data->data()),
                  boost::asio::buffers_end(data->data()));
              request_valid = request.find("POST /get_epose_service_endpoint_v2 HTTP/1.1") == 0
                  && request.find(expected_hash) != std::string::npos;
              ++requests;
              boost::asio::async_write(*socket, boost::asio::buffer(reply),
                  [socket](const boost::system::error_code &, size_t) {});
            };
            if (data->size() >= header_size + length)
              finish({}, 0);
            else
              boost::asio::async_read(*socket, *data,
                  boost::asio::transfer_exactly(header_size + length - data->size()), finish);
          });
        });
      });
    }

  public:
    explicit discovery_server(const endpoint_descriptor_v2 &descriptor)
      : expected_hash(epee::string_tools::pod_to_hex(commitment(descriptor)))
    {
      const auto body = discovery_json(descriptor);
      reply = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
          + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
      accept();
      worker = std::thread([this] { io.run(); });
    }
    ~discovery_server() { io.stop(); worker.join(); }
    tcp::endpoint endpoint() const { return acceptor.local_endpoint(); }
    unsigned request_count() const { return requests.load(); }
    bool valid_request() const { return request_valid.load(); }
  };

  epose_network_v2 server_network(discovery_server &server, unsigned &resolves, unsigned &connects)
  {
    epose_network_v2 network;
    network.resolve = async_answers([&resolves](const std::string &host, boost::system::error_code &) {
      EXPECT_EQ("directory.example.org", host);
      ++resolves;
      // A second lookup would rebind to loopback. It must never happen.
      return std::vector<boost::asio::ip::address>{make_address(resolves == 1 ? "8.8.8.8" : "127.0.0.1")};
    });
    network.connect = [&server, &connects](const tcp::endpoint &endpoint, boost::asio::steady_timer &timer) {
      ++connects;
      EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
      EXPECT_EQ(8197, endpoint.port());
      return connect_epose_address(server.endpoint(), timer);
    };
    return network;
  }

  const std::vector<std::string> prohibited_v4{
      "0.0.0.0", "0.1.2.3", "10.0.0.1", "10.255.255.255", "100.64.0.0", "100.127.255.255",
      "127.0.0.1", "127.255.255.255", "169.254.169.254", "172.16.0.0", "172.31.255.255",
      "192.0.0.1", "192.0.2.1", "192.88.99.1", "192.168.0.1", "198.18.0.1", "198.19.255.255",
      "198.51.100.1", "203.0.113.1", "224.0.0.1", "239.255.255.255", "240.0.0.1", "255.255.255.255"};
}

TEST(epose_http_client, epoch46_third_party_discovery_http)
{
  const auto expected = signed_descriptor();
  discovery_server server(expected);
  unsigned resolves = 0, connects = 0;
  const auto network = server_network(server, resolves, connects);
  endpoint_descriptor_v2 actual{};
  const auto result = fetch_epose_endpoint_descriptor("http://directory.example.org:8197",
      parameters(), commitment(expected), actual, network);
  EXPECT_EQ(1, server.request_count());
  EXPECT_TRUE(server.valid_request());
  ASSERT_TRUE(result.accepted()) << to_string(result.reason);
  EXPECT_EQ(expected.host, actual.host);
  EXPECT_EQ(expected.port, actual.port);
  EXPECT_EQ(expected.signature, actual.signature);
  EXPECT_EQ(commitment(expected), commitment(actual));
  EXPECT_NE("directory.example.org", actual.host);
  EXPECT_NE(8197, actual.port);
  EXPECT_EQ(1, resolves);
  EXPECT_EQ(2, connects); // TLS autodetection reconnect uses the same pinned IP.
}

TEST(epose_http_client, discovery_rejects_bad_signature_and_commitment)
{
  for (bool bad_signature : {false, true})
  {
    auto d = signed_descriptor();
    auto required = commitment(d);
    if (bad_signature) d.signature = {};
    else required = crypto::null_hash;
    discovery_server server(d);
    unsigned resolves = 0, connects = 0;
    endpoint_descriptor_v2 actual{};
    const auto result = fetch_epose_endpoint_descriptor("http://directory.example.org:8197",
        parameters(), required, actual, server_network(server, resolves, connects));
    EXPECT_EQ(bad_signature ? receipt_failure_reason_v2::descriptor_invalid
        : receipt_failure_reason_v2::descriptor_commitment_mismatch, result.reason);
    EXPECT_TRUE(actual.host.empty());
    EXPECT_EQ(1, server.request_count());
  }
}

TEST(epose_http_client, public_address_classifier)
{
  for (const auto &host : prohibited_v4)
  {
    SCOPED_TRACE(host);
    EXPECT_FALSE(public_epose_dial_address(make_address(host)));
    EXPECT_FALSE(public_epose_dial_address(make_address("::ffff:" + host)));
    EXPECT_FALSE(public_epose_dial_address(make_address("64:ff9b::" + host)));
    EXPECT_FALSE(public_epose_dial_address(make_address("64:ff9b:1::" + host)));
  }
  for (const char *host : {"::", "::1", "::127.0.0.1", "fc00::1", "fdff::1", "fe80::1", "fec0::1",
      "ff02::1", "2001:db8::1", "3fff::1", "3fff:fff:ffff::1", "100::1", "100:0:0:1::1",
      "2001:2::1", "2001::1", "2002:7f00:1::", "2001:4860::1%1",
      "64:ff9b:1:1::8.8.8.8", "64:ff9b:1:0:1::8.8.8.8", "64:ff9b:1::1:8.8.8.8",
      "64:ff9b:1:ffff:ffff:ffff:808:808", "64:ff9b:2::8.8.8.8"})
    EXPECT_FALSE(public_epose_dial_address(make_address(host))) << host;
  for (const char *host : {"1.1.1.1", "8.8.8.8", "100.63.255.255", "100.128.0.0", "172.15.255.255",
      "172.32.0.0", "192.0.1.255", "192.0.3.0", "198.17.255.255", "198.20.0.0", "223.255.255.255",
      "::ffff:8.8.8.8", "64:ff9b::8.8.8.8", "64:ff9b:1::8.8.8.8",
      "2001:4860:4860::8888", "2a01:4f8:201:6016::547", "3fff:1000::1"})
    EXPECT_TRUE(public_epose_dial_address(make_address(host))) << host;
}

TEST(epose_http_client, discovery_and_verifier_filter_nonpublic_dns_before_connect)
{
  std::vector<std::string> hosts = prohibited_v4;
  hosts.insert(hosts.end(), {"::1", "fc00::1", "fe80::1", "ff02::1", "2001:db8::1", "::ffff:127.0.0.1",
      "64:ff9b::169.254.169.254", "64:ff9b:1::a00:1"});
  for (const auto &host : hosts)
  {
    SCOPED_TRACE(host);
    // All-private, public first, and public last must use the same filtering.
    for (unsigned mixed : {0u, 1u, 2u})
    {
      unsigned resolves = 0, connects = 0;
      epose_network_v2 network;
      network.resolve = async_answers([&](const std::string &, boost::system::error_code &) {
        ++resolves;
        std::vector<boost::asio::ip::address> out{make_address(host), make_address("fd00::1")};
        if (mixed == 1) out.insert(out.begin(), make_address("8.8.8.8"));
        if (mixed == 2) out.push_back(make_address("8.8.8.8"));
        return out;
      });
      network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
        ++connects;
        EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
        throw boost::system::system_error(boost::asio::error::connection_refused);
      };
      endpoint_descriptor_v2 discovered{};
      auto result = fetch_epose_endpoint_descriptor("http://directory.example.org:8197",
          parameters(), crypto::null_hash, discovered, network);
      EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
      EXPECT_EQ(mixed ? receipt_failure_reason_v2::connection_refused
          : receipt_failure_reason_v2::address_prohibited, result.reason);
      canonical_service_response_v2 response{};
      result = request_epose_service_response({}, signed_descriptor("subject.example.org"), parameters(), response, network);
      EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
      EXPECT_EQ(mixed ? receipt_failure_reason_v2::connection_refused
          : receipt_failure_reason_v2::address_prohibited, result.reason);
      EXPECT_EQ(2, resolves);
      EXPECT_EQ(mixed ? 2u : 0u, connects);
    }
  }
}

TEST(epose_http_client, literal_addresses_cannot_bypass_dial_policy)
{
  for (const char *host : {"127.0.0.1", "192.0.2.1", "::1", "::ffff:10.0.0.1", "64:ff9b::169.254.169.254"})
  {
    SCOPED_TRACE(host);
    epose_network_v2 network;
    network.resolve = async_answers([](const std::string &, boost::system::error_code &) {
      ADD_FAILURE() << "Literal IP must not be resolved";
      return std::vector<boost::asio::ip::address>{make_address("8.8.8.8")};
    });
    network.connect = [](const tcp::endpoint &, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
      ADD_FAILURE() << "Prohibited literal must not be connected";
      throw boost::system::system_error(boost::asio::error::connection_refused);
    };
    endpoint_descriptor_v2 d{};
    const auto url_host = make_address(host).is_v6() ? "[" + std::string(host) + "]" : host;
    const auto fetched = fetch_epose_endpoint_descriptor("http://" + url_host + ":8197", parameters(), {}, d, network);
    EXPECT_EQ(receipt_failure_reason_v2::address_prohibited, fetched.reason);
    canonical_service_response_v2 response{};
    const auto probed = request_epose_service_response({}, signed_descriptor(host), parameters(), response, network);
    EXPECT_EQ(receipt_failure_reason_v2::address_prohibited, probed.reason);
  }
}

TEST(epose_http_client, cached_and_cold_dns_descriptors_obey_same_dial_policy)
{
  const auto descriptor = signed_descriptor("subject.example.org");
  const auto p = parameters();
  // On-chain/descriptor acceptance remains unchanged for DNS and special IPs
  // accepted by the existing validator; only local connection attempts change.
  ASSERT_EQ(resource_status_v2::accepted, validate_endpoint_descriptor_v2(
      p.nettype, p.genesis_hash, p.parameter_set_hash, descriptor));
  endpoint_descriptor_cache_v2 cache(2, 4);
  ASSERT_EQ(resource_status_v2::accepted, cache.admit(p.nettype, p.genesis_hash, p.parameter_set_hash, 1, descriptor));
  endpoint_descriptor_v2 warm{}, cold{};
  ASSERT_TRUE(cache.find(commitment(descriptor), 1, warm));
  discovery_server server(descriptor);
  unsigned resolves = 0, connects = 0;
  ASSERT_TRUE(fetch_epose_endpoint_descriptor("http://directory.example.org:8197", p,
      commitment(descriptor), cold, server_network(server, resolves, connects)).accepted());
  epose_network_v2 network;
  network.resolve = async_answers([](const std::string &, boost::system::error_code &) {
    return std::vector<boost::asio::ip::address>{make_address("169.254.169.254")};
  });
  network.connect = [](const tcp::endpoint &, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
    ADD_FAILURE() << "Neither warm nor cold descriptor may connect to metadata service";
    throw boost::system::system_error(boost::asio::error::connection_refused);
  };
  for (const auto &d : {warm, cold})
  {
    canonical_service_response_v2 response{};
    EXPECT_EQ(receipt_failure_reason_v2::address_prohibited,
        request_epose_service_response({}, d, p, response, network).reason);
  }
  for (const char *host : {"192.0.2.1", "64:ff9b::7f00:1"})
  {
    const auto d = signed_descriptor(host);
    EXPECT_EQ(resource_status_v2::accepted, validate_endpoint_descriptor_v2(
        p.nettype, p.genesis_hash, p.parameter_set_hash, d));
    EXPECT_FALSE(public_epose_dial_address(make_address(host)));
  }
}

TEST(epose_http_client, dns_failure_and_empty_answer_are_classified)
{
  for (bool empty : {false, true})
  {
    epose_network_v2 network;
    network.resolve = async_answers([empty](const std::string &, boost::system::error_code &error) {
      if (!empty) error = boost::asio::error::host_not_found;
      return std::vector<boost::asio::ip::address>{};
    });
    endpoint_descriptor_v2 descriptor{};
    EXPECT_EQ(receipt_failure_reason_v2::dns_resolution_failed,
        fetch_epose_endpoint_descriptor("http://directory.example.org:8197", parameters(), {}, descriptor, network).reason);
    canonical_service_response_v2 response{};
    EXPECT_EQ(receipt_failure_reason_v2::dns_resolution_failed,
        request_epose_service_response({}, signed_descriptor("subject.example.org"), parameters(), response, network).reason);
  }
}

TEST(epose_http_client, verifier_connects_only_to_pinned_dns_answer)
{
  unsigned resolves = 0, connects = 0;
  epose_network_v2 network;
  network.resolve = async_answers([&](const std::string &, boost::system::error_code &) {
    ++resolves;
    return std::vector<boost::asio::ip::address>{make_address(resolves == 1 ? "8.8.8.8" : "127.0.0.1")};
  });
  network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
    ++connects;
    EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
    EXPECT_EQ(8198, endpoint.port());
    throw boost::system::system_error(boost::asio::error::connection_refused);
  };
  canonical_service_response_v2 response{};
  const auto result = request_epose_service_response({}, signed_descriptor("subject.example.org"), parameters(), response, network);
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, result.reason);
  EXPECT_EQ(1, resolves);
  EXPECT_EQ(1, connects);
}

TEST(epose_http_client, system_resolver_rejects_localhost_before_connect)
{
  epose_network_v2 network;
  network.connect = [](const tcp::endpoint &, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
    ADD_FAILURE() << "System resolver must not permit loopback";
    throw boost::system::system_error(boost::asio::error::connection_refused);
  };
  endpoint_descriptor_v2 descriptor{};
  EXPECT_EQ(receipt_failure_reason_v2::address_prohibited,
      fetch_epose_endpoint_descriptor("http://localhost:8197", parameters(), {}, descriptor, network).reason);
  canonical_service_response_v2 response{};
  EXPECT_EQ(receipt_failure_reason_v2::address_prohibited,
      request_epose_service_response({}, signed_descriptor("localhost"), parameters(), response, network).reason);
}


TEST(epose_http_client, dns64_public_and_private_answers)
{
  for (const std::string prefix : {"64:ff9b::", "64:ff9b:1::"})
  {
    SCOPED_TRACE(prefix);
    for (const std::string v4 : {"95.216.221.239", "202.61.202.161", "159.195.216.239",
        "159.195.194.92", "148.251.24.156", "10.0.0.1", "172.31.254.30", "192.168.1.1"})
    {
      SCOPED_TRACE(v4);
      const bool allowed = public_epose_dial_address(make_address(v4));
      EXPECT_EQ(allowed, public_epose_dial_address(make_address(prefix + v4)));
      // AAAA only, AAAA then A, and A then AAAA (CT 547 with and without CLAT).
      for (unsigned layout : {0u, 1u, 2u})
      {
        unsigned resolves = 0;
        std::vector<boost::asio::ip::address> dialed;
        epose_network_v2 network;
        network.resolve = async_answers([&](const std::string &, boost::system::error_code &) {
          ++resolves;
          std::vector<boost::asio::ip::address> answers{make_address(prefix + v4)};
          if (layout == 1) answers.push_back(make_address(v4));
          if (layout == 2) answers.insert(answers.begin(), make_address(v4));
          return answers;
        });
        network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
          dialed.push_back(endpoint.address());
          throw boost::system::system_error(boost::asio::error::connection_refused);
        };
        endpoint_descriptor_v2 discovered{};
        const auto fetched = fetch_epose_endpoint_descriptor("http://directory.example.org:8197",
            parameters(), {}, discovered, network);
        canonical_service_response_v2 response{};
        const auto probed = request_epose_service_response({}, signed_descriptor("subject.example.org"),
            parameters(), response, network);
        const auto expected = allowed ? receipt_failure_reason_v2::connection_refused
            : receipt_failure_reason_v2::address_prohibited;
        EXPECT_EQ(expected, fetched.reason);
        EXPECT_EQ(expected, probed.reason);
        EXPECT_EQ(2u, resolves);
        std::vector<boost::asio::ip::address> expected_dials;
        if (allowed)
          for (unsigned attempt = 0; attempt < 2; ++attempt)
          {
            if (layout) expected_dials.push_back(make_address(v4));
            expected_dials.push_back(make_address(prefix + v4));
          }
        EXPECT_EQ(expected_dials, dialed);
      }
    }
  }
}

TEST(epose_http_client, fallback_to_second_checked_address_completes_http)
{
  const auto descriptor = signed_descriptor();
  discovery_server server(descriptor);
  for (const char *second : {"1.1.1.1", "64:ff9b:1::5fd8:ddef", "64:ff9b::5fd8:ddef"})
  {
    SCOPED_TRACE(second);
    unsigned resolves = 0;
    std::vector<boost::asio::ip::address> dialed;
    std::vector<std::chrono::steady_clock::time_point> deadlines;
    epose_network_v2 network;
    network.resolve = async_answers([&](const std::string &, boost::system::error_code &) {
      ++resolves;
      return std::vector<boost::asio::ip::address>{make_address("10.0.0.1"),
          make_address("8.8.8.8"), make_address("fd00::1"), make_address(second)};
    });
    network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &timeout) {
      dialed.push_back(endpoint.address());
      deadlines.push_back(timeout.expiry());
      EXPECT_EQ(8197, endpoint.port());
      if (endpoint.address() == make_address("8.8.8.8"))
      {
        // Asynchronous failure exercises the same future path as a real socket.
        auto result = std::make_shared<boost::promise<tcp::socket>>();
        auto delay = std::make_shared<boost::asio::steady_timer>(MONERO_GET_EXECUTOR(timeout));
        delay->expires_after(std::chrono::milliseconds(10));
        delay->async_wait([result, delay](const boost::system::error_code &) {
          result->set_exception(boost::system::system_error(boost::asio::error::connection_refused));
        });
        return result->get_future();
      }
      EXPECT_EQ(make_address(second), endpoint.address());
      return connect_epose_address(server.endpoint(), timeout);
    };
    endpoint_descriptor_v2 actual{};
    const auto result = fetch_epose_endpoint_descriptor("http://directory.example.org:8197",
        parameters(), commitment(descriptor), actual, network);
    EXPECT_TRUE(result.accepted()) << to_string(result.reason);
    EXPECT_EQ(commitment(descriptor), commitment(actual));
    EXPECT_EQ(1u, resolves);
    const std::vector<boost::asio::ip::address> expected{
        make_address("8.8.8.8"), make_address(second),
        make_address("8.8.8.8"), make_address(second)};
    EXPECT_EQ(expected, dialed); // Also covers TLS autodetection reconnect.
    ASSERT_EQ(4u, deadlines.size());
    for (const auto deadline : deadlines) EXPECT_EQ(deadlines.front(), deadline);
  }
  EXPECT_EQ(3u, server.request_count());
  EXPECT_TRUE(server.valid_request());
}

TEST(epose_http_client, fallback_preserves_resolver_order_with_ipv4_first)
{
  for (bool asynchronous : {false, true})
  {
    unsigned resolves = 0;
    std::vector<boost::asio::ip::address> dialed;
    epose_network_v2 network;
    network.resolve = async_answers([&](const std::string &, boost::system::error_code &) {
      ++resolves;
      return std::vector<boost::asio::ip::address>{make_address("2606:4700::1111"),
          make_address("8.8.8.8"), make_address("fd00::1"), make_address("64:ff9b:1::5fd8:ddef"),
          make_address("1.1.1.1"), make_address("127.0.0.1")};
    });
    network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &timeout) -> boost::unique_future<tcp::socket> {
      dialed.push_back(endpoint.address());
      const boost::system::system_error error(dialed.size() == 4
          ? boost::asio::error::connection_refused : boost::asio::error::network_unreachable);
      if (!asynchronous) throw error;
      auto result = std::make_shared<boost::promise<tcp::socket>>();
      boost::asio::post(MONERO_GET_EXECUTOR(timeout), [result, error] { result->set_exception(error); });
      return result->get_future();
    };
    canonical_service_response_v2 response{};
    const auto result = request_epose_service_response({}, signed_descriptor("subject.example.org"),
        parameters(), response, network);
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused, result.reason);
    EXPECT_EQ(1u, resolves);
    const std::vector<boost::asio::ip::address> expected{make_address("8.8.8.8"),
        make_address("1.1.1.1"), make_address("2606:4700::1111"), make_address("64:ff9b:1::5fd8:ddef")};
    EXPECT_EQ(expected, dialed);
  }
}

TEST(epose_http_client, fallback_stops_at_shared_deadline)
{
  unsigned connects = 0;
  std::chrono::steady_clock::time_point started;
  epose_network_v2 network;
  network.resolve = async_answers([](const std::string &, boost::system::error_code &) {
    return std::vector<boost::asio::ip::address>{make_address("8.8.8.8"), make_address("1.1.1.1")};
  });
  network.connect = [&](const tcp::endpoint &, boost::asio::steady_timer &timeout) {
    ++connects;
    started = std::chrono::steady_clock::now();
    auto result = std::make_shared<boost::promise<tcp::socket>>();
    timeout.async_wait([result](const boost::system::error_code &) {
      result->set_exception(boost::system::system_error(boost::asio::error::operation_aborted));
    });
    return result->get_future();
  };
  epose_http_client client;
  ASSERT_TRUE(prepare_epose_http_client(client, "subject.example.org", 8198, network).accepted());
  EXPECT_FALSE(client.client().invoke("/", "POST", "{}", std::chrono::milliseconds(30)));
  const auto elapsed = std::chrono::steady_clock::now() - started;
  EXPECT_EQ(1u, connects);
  EXPECT_EQ(boost::asio::error::timed_out, client.client().get_last_error());
  EXPECT_GE(elapsed, std::chrono::milliseconds(25));
  EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST(epose_http_client, unresolved_lookup_is_bounded_and_does_not_queue_replacements)
{
  epose_network_v2::resolve_handler late;
  boost::asio::io_context *executor = nullptr;
  unsigned resolves = 0, cancels = 0, connects = 0;
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    executor = &io;
    ++resolves;
    late = std::move(complete); // Simulate libc remaining blocked after cancel.
    return [&cancels] { ++cancels; };
  };
  network.connect = [&](const tcp::endpoint &, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
    ++connects;
    throw boost::system::system_error(boost::asio::error::connection_refused);
  };
  std::chrono::steady_clock::duration elapsed{};
  for (unsigned i = 0; i < 100; ++i)
  {
    epose_http_client client;
    ASSERT_TRUE(prepare_epose_http_client(client, "subject.example.org", 8198, network).accepted());
    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.client().connect(std::chrono::milliseconds(30)));
    elapsed += std::chrono::steady_clock::now() - started;
    EXPECT_EQ(boost::asio::error::timed_out, client.client().get_last_error());
  }
  EXPECT_GE(elapsed, std::chrono::milliseconds(25));
  EXPECT_LT(elapsed, std::chrono::seconds(2));
  EXPECT_EQ(1u, resolves);
  EXPECT_EQ(1u, cancels);
  EXPECT_EQ(0u, connects);
  // The original lease and its injected functions are gone. Completion only
  // releases admission; it must not dial or change the finished result.
  ASSERT_TRUE(bool(late));
  ASSERT_NE(nullptr, executor);
  boost::asio::post(*executor, [late] { late({}, {make_address("8.8.8.8")}); });
  epose_http_client drain;
  ASSERT_TRUE(drain.available());
  executor->restart();
  executor->poll();
  EXPECT_EQ(0u, connects);
}

TEST(epose_http_client, late_completion_after_return_is_ignored_and_capacity_recovers)
{
  epose_network_v2::resolve_handler late;
  boost::asio::io_context *executor = nullptr;
  {
    epose_network_v2 network;
    network.resolve = [&](const std::string &, boost::asio::io_context &io,
        epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
      executor = &io;
      late = std::move(complete);
      return [] {};
    };
    epose_http_client client;
    ASSERT_TRUE(prepare_epose_http_client(client, "subject.example.org", 8198, network).accepted());
    EXPECT_FALSE(client.client().connect(std::chrono::milliseconds(20)));
    EXPECT_EQ(boost::asio::error::timed_out, client.client().get_last_error());
  }
  ASSERT_NE(nullptr, executor);
  boost::asio::post(*executor, [late] { late({}, {make_address("127.0.0.1")}); });
  // A fresh request drains the late completion and can resolve normally.
  epose_network_v2 network;
  unsigned resolves = 0, connects = 0;
  network.resolve = async_answers([&](const std::string &, boost::system::error_code &) {
    ++resolves;
    return std::vector<boost::asio::ip::address>{make_address("8.8.8.8")};
  });
  network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
    ++connects;
    EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
    throw boost::system::system_error(boost::asio::error::connection_refused);
  };
  canonical_service_response_v2 response{};
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused,
      request_epose_service_response({}, signed_descriptor("subject.example.org"), parameters(), response, network).reason);
  EXPECT_EQ(1u, resolves);
  EXPECT_EQ(1u, connects);
}

TEST(epose_http_client, dns_and_connect_share_deadline_and_executor)
{
  boost::asio::io_context *executor = nullptr;
  std::chrono::steady_clock::time_point deadline;
  unsigned connects = 0;
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    executor = &io;
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
    const auto delay = std::make_shared<boost::asio::steady_timer>(io);
    delay->expires_after(std::chrono::milliseconds(30));
    delay->async_wait([delay, complete](const boost::system::error_code &error) {
      complete(error, {make_address("8.8.8.8"), make_address("1.1.1.1")});
    });
    return [delay] { delay->cancel(); };
  };
  network.connect = [&](const tcp::endpoint &, boost::asio::steady_timer &timeout) {
    ++connects;
#if BOOST_VERSION >= 107000
    EXPECT_EQ(executor, &timeout.get_executor().context());
#else
    EXPECT_EQ(executor, &timeout.get_io_context());
#endif
    EXPECT_LE(timeout.expiry(), deadline);
    EXPECT_LT(timeout.expiry() - std::chrono::steady_clock::now(), std::chrono::milliseconds(60));
    const auto result = std::make_shared<boost::promise<tcp::socket>>();
    timeout.async_wait([result](const boost::system::error_code &) {
      result->set_exception(boost::system::system_error(boost::asio::error::timed_out));
    });
    return result->get_future();
  };
  epose_http_client client;
  ASSERT_TRUE(prepare_epose_http_client(client, "subject.example.org", 8198, network).accepted());
  const auto started = std::chrono::steady_clock::now();
  EXPECT_FALSE(client.client().connect(std::chrono::milliseconds(80)));
  EXPECT_EQ(boost::asio::error::timed_out, client.client().get_last_error());
  EXPECT_EQ(1u, connects);
  const auto elapsed = std::chrono::steady_clock::now() - started;
  EXPECT_GE(elapsed, std::chrono::milliseconds(75));
  EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST(epose_http_client, public_literals_bypass_resolver)
{
  for (const char *host : {"8.8.8.8", "2606:4700::1111"})
  {
    epose_network_v2 network;
    network.resolve = [](const std::string &, boost::asio::io_context &,
        epose_network_v2::resolve_handler) -> epose_network_v2::cancel_resolve {
      ADD_FAILURE() << "literal IP must not resolve";
      return [] {};
    };
    unsigned connects = 0;
    network.connect = [&](const tcp::endpoint &endpoint, boost::asio::steady_timer &) -> boost::unique_future<tcp::socket> {
      ++connects;
      EXPECT_EQ(make_address(host), endpoint.address());
      throw boost::system::system_error(boost::asio::error::connection_refused);
    };
    canonical_service_response_v2 response{};
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused,
        request_epose_service_response({}, signed_descriptor(host), parameters(), response, network).reason);
    EXPECT_EQ(1u, connects);
  }
}

TEST(epose_http_client, dns_deadline_classified_for_discovery_and_service)
{
  unsigned cancels = 0;
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    // Never finishes until cancelled. Exercise the real five-second budget in
    // both public callers, including late delivery of operation_aborted.
    return [&io, complete, &cancels] {
      ++cancels;
      boost::asio::post(io, [complete] { complete(boost::asio::error::operation_aborted, {}); });
    };
  };
  endpoint_descriptor_v2 descriptor{};
  auto started = std::chrono::steady_clock::now();
  auto result = fetch_epose_endpoint_descriptor("http://directory.example.org:8197", parameters(), {}, descriptor, network);
  EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
  EXPECT_EQ(receipt_failure_reason_v2::transport_timeout, result.reason);
  EXPECT_EQ(1u, cancels);
  EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(4900));
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(10));
  canonical_service_response_v2 response{};
  started = std::chrono::steady_clock::now();
  result = request_epose_service_response({}, signed_descriptor("subject.example.org"), parameters(), response, network);
  EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
  EXPECT_EQ(receipt_failure_reason_v2::transport_timeout, result.reason);
  EXPECT_EQ(2u, cancels);
  EXPECT_GE(std::chrono::steady_clock::now() - started, std::chrono::milliseconds(4900));
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(10));
}

TEST(epose_http_client, concurrent_client_lease_fails_without_waiting)
{
  epose_http_client held;
  ASSERT_TRUE(held.available());
  std::thread contender([] {
    epose_http_client other;
    EXPECT_FALSE(other.available());
    const auto result = prepare_epose_http_client(other, "subject.example.org", 8198);
    EXPECT_EQ(receipt_failure_reason_v2::transport_timeout, result.reason);
    EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
  });
  contender.join();
}
