// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <set>
#include <thread>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "cryptonote_core/epose_http_client.h"
#include "epose/compiled_profile_v2.h"
#include "epose/membership_v2.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/portable_storage_template_helper.h"
#include "string_tools.h"

namespace
{
  using namespace qwertycoin::epose;
  using namespace cryptonote;
  using boost::asio::ip::make_address;
  using boost::asio::ip::tcp;
  using steady = std::chrono::steady_clock;
  using ms = std::chrono::milliseconds;
  using address_list = std::vector<boost::asio::ip::address>;

  constexpr uint64_t T_MS = uint64_t(DIFFICULTY_TARGET_V2) * 1000;

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

  // A resolver completion the test holds back, simulating libc staying
  // blocked after cancel. deliver() completes it on the client's worker.
  struct held_lookup
  {
    std::mutex mutex;
    boost::asio::io_context *io = nullptr;
    epose_network_v2::resolve_handler late;

    void hold(boost::asio::io_context &context, epose_network_v2::resolve_handler complete)
    {
      std::lock_guard<std::mutex> lock(mutex);
      io = &context;
      late = std::move(complete);
    }
    bool held()
    {
      std::lock_guard<std::mutex> lock(mutex);
      return io && late;
    }
    void deliver(boost::system::error_code error, address_list answers)
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!io || !late)
        return;
      boost::asio::post(*io, [complete = std::move(late), error, answers] { complete(error, answers); });
      late = {};
    }
  };

  // Declare after the client: delivers a still-held lookup while the client's
  // worker is alive, so the late completion never outlives its executor.
  struct deliver_on_exit
  {
    held_lookup &lookup;
    ~deliver_on_exit() { lookup.deliver(boost::asio::error::operation_aborted, {}); }
  };

  struct hang_counts
  {
    std::atomic<unsigned> started{0};
    std::atomic<unsigned> cancelled{0};
  };

  // A connect that never completes on its own (dead peer, SYN black hole).
  epose_network_v2::cancel_operation hang(hang_counts &counts, epose_network_v2::connect_handler complete)
  {
    ++counts.started;
    const auto held = std::make_shared<epose_network_v2::connect_handler>(std::move(complete));
    return [&counts, held] {
      ++counts.cancelled;
      *held = {};
    };
  }

  epose_network_v2::cancel_operation refuse()
  {
    throw boost::system::system_error(boost::asio::error::connection_refused);
  }

  epose_stage_deadlines_v2 caps(ms connect, ms send = ms(EPOSE_V2_SEND_TIMEOUT_MS),
      ms recv_idle = ms(EPOSE_V2_RECV_IDLE_TIMEOUT_MS))
  {
    epose_stage_deadlines_v2 out{};
    out.connect = connect;
    out.send = send;
    out.recv_idle = recv_idle;
    return out;
  }

  consensus_parameters_v2 parameters()
  {
    consensus_parameters_v2 out{};
    out.nettype = TESTNET;
    out.genesis_hash = crypto::cn_fast_hash("genesis", 7);
    out.parameter_set_hash = crypto::cn_fast_hash("parameters", 10);
    return out;
  }

  crypto::public_key key(unsigned i)
  {
    const crypto::hash h = crypto::cn_fast_hash(&i, sizeof(i));
    crypto::public_key out{};
    static_assert(sizeof(out) == sizeof(h), "key size");
    memcpy(&out, &h, sizeof(out));
    return out;
  }

  service_challenge_v2 challenge()
  {
    service_challenge_v2 out{};
    out.epoch = 46;
    out.round = 1;
    out.nonce = crypto::cn_fast_hash("nonce", 5);
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

  std::string http_ok(const std::string &body)
  {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
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

  crypto::signature service_signature()
  {
    crypto::signature out{};
    memset(&out, 0x5a, sizeof(out));
    return out;
  }

  std::string service_json(const std::string &block_blob_hex = "00ff")
  {
    COMMAND_RPC_EPOSE_SERVICE_CHALLENGE_V2::response out{};
    out.status = CORE_RPC_STATUS_OK;
    out.block_blob = block_blob_hex;
    out.subject_signature = epee::string_tools::pod_to_hex(service_signature());
    std::string json;
    EXPECT_TRUE(epee::serialization::store_t_to_json(out, json));
    return json;
  }

  // A real loopback HTTP server. Only test connect hooks redirect to it;
  // production still checks the public answer and pins that exact endpoint.
  class http_fixture
  {
  public:
    enum class mode
    {
      reply,      // read the whole request, send reply, close
      silent,     // read the whole request, never answer
      never_read, // accept with a small receive buffer, never read
      trickle     // read the whole request, send reply in timed chunks
    };

    struct trickle_plan
    {
      size_t chunk = 64 * 1024;
      ms gap{0};
      size_t stall_after = 0; // stop sending (and hold the socket) here; 0 = never
      // After the chunk that reaches pause_at, wait `pause` instead of `gap`
      // once, then go on; 0 = never.
      size_t pause_at = 0;
      ms pause{0};
    };

  private:
    boost::asio::io_context io;
    tcp::acceptor acceptor{io};
    std::thread worker;
    const mode behaviour;
    const std::string reply;
    const trickle_plan plan;
    std::atomic<unsigned> requests{0};
    std::mutex mutex;
    std::string last;
    std::vector<std::shared_ptr<tcp::socket>> held; // fixture thread only

    void serve(const std::shared_ptr<tcp::socket> &socket)
    {
      if (behaviour == mode::never_read)
      {
        held.push_back(socket);
        return;
      }
      const auto data = std::make_shared<boost::asio::streambuf>();
      boost::asio::async_read_until(*socket, *data, "\r\n\r\n",
          [this, socket, data](const boost::system::error_code &error, size_t header_size) {
        if (error) return;
        const std::string buffered(boost::asio::buffers_begin(data->data()),
            boost::asio::buffers_end(data->data()));
        size_t length = 0;
        const auto length_pos = buffered.find("Content-Length: ");
        if (length_pos != std::string::npos && length_pos < header_size)
          length = std::stoul(buffered.substr(length_pos + 16));
        const auto finish = [this, socket, data](const boost::system::error_code &error, size_t) {
          if (error) return;
          {
            std::lock_guard<std::mutex> lock(mutex);
            last.assign(boost::asio::buffers_begin(data->data()), boost::asio::buffers_end(data->data()));
          }
          ++requests;
          if (behaviour == mode::silent)
          {
            held.push_back(socket);
            return;
          }
          const auto out = std::make_shared<std::string>(reply);
          if (behaviour == mode::trickle)
          {
            send_chunks(socket, out, 0);
            return;
          }
          boost::asio::async_write(*socket, boost::asio::buffer(*out),
              [socket, out](const boost::system::error_code &, size_t) {});
        };
        if (data->size() >= header_size + length)
          finish({}, 0);
        else
          boost::asio::async_read(*socket, *data,
              boost::asio::transfer_exactly(header_size + length - data->size()), finish);
      });
    }

    void send_chunks(const std::shared_ptr<tcp::socket> &socket,
        const std::shared_ptr<std::string> &out, size_t offset)
    {
      const size_t end = plan.stall_after != 0
          ? std::min(plan.stall_after, out->size()) : out->size();
      if (offset >= end)
      {
        if (end < out->size())
          held.push_back(socket); // stalled: open, silent
        return;
      }
      const size_t n = std::min(plan.chunk, end - offset);
      boost::asio::async_write(*socket, boost::asio::buffer(out->data() + offset, n),
          [this, socket, out, offset, n](const boost::system::error_code &error, size_t) {
        if (error) return;
        const auto timer = std::make_shared<boost::asio::steady_timer>(io);
        const bool pause = plan.pause_at != 0 && offset < plan.pause_at && offset + n >= plan.pause_at;
        timer->expires_after(pause ? plan.pause : plan.gap);
        timer->async_wait([this, socket, out, offset, n, timer](const boost::system::error_code &e) {
          if (e) return;
          send_chunks(socket, out, offset + n);
        });
      });
    }

    void accept()
    {
      const auto socket = std::make_shared<tcp::socket>(io);
      acceptor.async_accept(*socket, [this, socket](const boost::system::error_code &error) {
        if (error) return;
        accept();
        serve(socket);
      });
    }

  public:
    explicit http_fixture(mode m, std::string response = {})
      : http_fixture(m, std::move(response), trickle_plan())
    {
    }

    http_fixture(mode m, std::string response, trickle_plan pacing)
      : behaviour(m), reply(std::move(response)), plan(pacing)
    {
      const tcp::endpoint local(boost::asio::ip::address_v4::loopback(), 0);
      acceptor.open(local.protocol());
      if (m == mode::never_read)
        acceptor.set_option(boost::asio::socket_base::receive_buffer_size(4096));
      acceptor.bind(local);
      acceptor.listen();
      accept();
      worker = std::thread([this] { io.run(); });
    }
    ~http_fixture() { io.stop(); worker.join(); }
    tcp::endpoint endpoint() const { return acceptor.local_endpoint(); }
    unsigned request_count() const { return requests.load(); }
    std::string last_request()
    {
      std::lock_guard<std::mutex> lock(mutex);
      return last;
    }
  };

  // Connect to a fixture instead of the public endpoint the client chose.
  epose_network_v2::connect_function redirect(const tcp::endpoint &target)
  {
    return [target](const tcp::endpoint &, boost::asio::io_context &io, steady::time_point deadline,
        epose_network_v2::connect_handler complete) {
      return connect_epose_address(target, io, deadline, std::move(complete));
    };
  }

  // A port nobody listens on: the kernel answers SYN with RST.
  tcp::endpoint closed_port()
  {
    boost::asio::io_context io;
    tcp::acceptor probe(io, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
    const auto out = probe.local_endpoint();
    probe.close();
    return out;
  }

  // Fill the send queue against a peer that never reads. The receiver may
  // reopen a small window after the first would_block, so the queue counts as
  // full only once writes stayed blocked for 500 ms.
  void fill_send_queue(tcp::socket &socket)
  {
    constexpr unsigned stable = 25; // x 20 ms
    boost::system::error_code ignored;
    socket.set_option(boost::asio::socket_base::send_buffer_size(4096), ignored);
    socket.non_blocking(true, ignored);
    const std::vector<char> junk(64 * 1024, 'x');
    const auto until = steady::now() + std::chrono::seconds(5);
    unsigned blocked = 0;
    while (blocked < stable && steady::now() < until)
    {
      boost::system::error_code error;
      const size_t written = socket.write_some(boost::asio::buffer(junk), error);
      if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
      {
        ++blocked;
        std::this_thread::sleep_for(ms(20));
        continue;
      }
      if (error)
        break;
      if (written)
        blocked = 0;
    }
    EXPECT_EQ(stable, blocked);
    socket.non_blocking(false, ignored);
  }

  epose_network_v2 server_network(http_fixture &server, std::atomic<unsigned> &resolves,
      std::atomic<unsigned> &connects)
  {
    epose_network_v2 network;
    network.resolve = async_answers([&resolves](const std::string &host, boost::system::error_code &) {
      EXPECT_EQ("directory.example.org", host);
      ++resolves;
      // A second lookup would rebind to loopback. It must never happen.
      return address_list{make_address(resolves == 1 ? "8.8.8.8" : "127.0.0.1")};
    });
    const auto target = server.endpoint();
    network.connect = [target, &connects](const tcp::endpoint &endpoint, boost::asio::io_context &io,
        steady::time_point deadline, epose_network_v2::connect_handler complete) {
      ++connects;
      EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
      EXPECT_EQ(8197, endpoint.port());
      return connect_epose_address(target, io, deadline, std::move(complete));
    };
    return network;
  }

  bool same(const receipt_step_result_v2 &result, receipt_attempt_stage_v2 stage,
      receipt_failure_reason_v2 reason)
  {
    return result.stage == stage && result.reason == reason;
  }

  std::string describe(const receipt_step_result_v2 &result)
  {
    return std::string(to_string(result.stage)) + "/" + to_string(result.reason);
  }

  // A descriptor lookup that is not bound to the challenge's subject.
  epose_receipt_request_v2 discovery_only(std::vector<std::string> urls)
  {
    epose_receipt_request_v2 out{};
    out.parameters = parameters();
    out.challenge = challenge();
    out.discovery = std::move(urls);
    out.bind_to_challenge = false;
    out.descriptor_only = true;
    out.deadlines = caps(ms(1000));
    return out;
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
  http_fixture server(http_fixture::mode::reply, http_ok(discovery_json(expected)));
  std::atomic<unsigned> resolves{0}, connects{0};
  epose_receipt_client_v2 client(server_network(server, resolves, connects));
  const auto exchange = client.fetch_descriptor("http://directory.example.org:8197",
      parameters(), commitment(expected)).wait();
  EXPECT_EQ(1u, server.request_count());
  const auto request = server.last_request();
  EXPECT_EQ(0u, request.find("POST /get_epose_service_endpoint_v2 HTTP/1.1\r\n"));
  EXPECT_NE(std::string::npos, request.find(epee::string_tools::pod_to_hex(commitment(expected))));
  ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  const auto &actual = exchange.endpoint;
  EXPECT_EQ(expected.host, actual.host);
  EXPECT_EQ(expected.port, actual.port);
  EXPECT_EQ(expected.signature, actual.signature);
  EXPECT_EQ(commitment(expected), commitment(actual));
  EXPECT_NE("directory.example.org", actual.host);
  EXPECT_NE(8197, actual.port);
  EXPECT_EQ(1u, resolves.load());
  EXPECT_EQ(1u, connects.load()); // Plain HTTP: one connect to the pinned IP.
}

TEST(epose_http_client, discovery_rejects_bad_signature_and_commitment)
{
  for (bool bad_signature : {false, true})
  {
    auto d = signed_descriptor();
    auto required = commitment(d);
    if (bad_signature) d.signature = {};
    else required = crypto::null_hash;
    http_fixture server(http_fixture::mode::reply, http_ok(discovery_json(d)));
    std::atomic<unsigned> resolves{0}, connects{0};
    epose_receipt_client_v2 client(server_network(server, resolves, connects));
    const auto exchange = client.fetch_descriptor("http://directory.example.org:8197",
        parameters(), required).wait();
    EXPECT_EQ(bad_signature ? receipt_failure_reason_v2::descriptor_invalid
        : receipt_failure_reason_v2::descriptor_commitment_mismatch, exchange.result.reason);
    EXPECT_TRUE(exchange.endpoint.host.empty());
    EXPECT_EQ(1u, server.request_count());
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
      std::atomic<unsigned> resolves{0}, connects{0};
      epose_network_v2 network;
      network.resolve = async_answers([&resolves, host, mixed](const std::string &, boost::system::error_code &) {
        ++resolves;
        address_list out{make_address(host), make_address("fd00::1")};
        if (mixed == 1) out.insert(out.begin(), make_address("8.8.8.8"));
        if (mixed == 2) out.push_back(make_address("8.8.8.8"));
        return out;
      });
      network.connect = [&connects](const tcp::endpoint &endpoint, boost::asio::io_context &,
          steady::time_point, epose_network_v2::connect_handler) {
        ++connects;
        EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
        return refuse();
      };
      epose_receipt_client_v2 client(network);
      const auto expected = mixed ? receipt_failure_reason_v2::connection_refused
          : receipt_failure_reason_v2::address_prohibited;
      auto result = client.fetch_descriptor("http://directory.example.org:8197",
          parameters(), crypto::null_hash).wait().result;
      EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
      EXPECT_EQ(expected, result.reason);
      result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
          parameters()).wait().result;
      EXPECT_EQ(receipt_attempt_stage_v2::network_connect, result.stage);
      EXPECT_EQ(expected, result.reason);
      EXPECT_EQ(2u, resolves.load());
      EXPECT_EQ(mixed ? 2u : 0u, connects.load());
    }
  }
}

TEST(epose_http_client, literal_addresses_cannot_bypass_dial_policy)
{
  epose_network_v2 network;
  network.resolve = async_answers([](const std::string &, boost::system::error_code &) {
    ADD_FAILURE() << "Literal IP must not be resolved";
    return address_list{make_address("8.8.8.8")};
  });
  network.connect = [](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ADD_FAILURE() << "Prohibited literal must not be connected";
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  for (const char *host : {"127.0.0.1", "192.0.2.1", "::1", "::ffff:10.0.0.1", "64:ff9b::169.254.169.254"})
  {
    SCOPED_TRACE(host);
    const std::string url_host = make_address(host).is_v6() ? "[" + std::string(host) + "]" : host;
    EXPECT_EQ(receipt_failure_reason_v2::address_prohibited, client.fetch_descriptor(
        "http://" + url_host + ":8197", parameters(), {}).wait().result.reason);
    EXPECT_EQ(receipt_failure_reason_v2::address_prohibited, client.request_service(
        challenge(), signed_descriptor(host), parameters()).wait().result.reason);
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
  {
    http_fixture server(http_fixture::mode::reply, http_ok(discovery_json(descriptor)));
    std::atomic<unsigned> resolves{0}, connects{0};
    epose_receipt_client_v2 discovery(server_network(server, resolves, connects));
    const auto fetched = discovery.fetch_descriptor("http://directory.example.org:8197", p,
        commitment(descriptor)).wait();
    ASSERT_TRUE(fetched.result.accepted()) << describe(fetched.result);
    cold = fetched.endpoint;
  }
  epose_network_v2 network;
  network.resolve = async_answers([](const std::string &, boost::system::error_code &) {
    return address_list{make_address("169.254.169.254")};
  });
  network.connect = [](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ADD_FAILURE() << "Neither warm nor cold descriptor may connect to metadata service";
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  for (const auto &d : {warm, cold})
    EXPECT_EQ(receipt_failure_reason_v2::address_prohibited,
        client.request_service(challenge(), d, p).wait().result.reason);
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
      return address_list{};
    });
    epose_receipt_client_v2 client(network);
    EXPECT_EQ(receipt_failure_reason_v2::dns_resolution_failed, client.fetch_descriptor(
        "http://directory.example.org:8197", parameters(), {}).wait().result.reason);
    EXPECT_EQ(receipt_failure_reason_v2::dns_resolution_failed, client.request_service(
        challenge(), signed_descriptor("subject.example.org"), parameters()).wait().result.reason);
  }
}

TEST(epose_http_client, verifier_connects_only_to_pinned_dns_answer)
{
  std::atomic<unsigned> resolves{0}, connects{0};
  epose_network_v2 network;
  network.resolve = async_answers([&resolves](const std::string &, boost::system::error_code &) {
    ++resolves;
    return address_list{make_address(resolves == 1 ? "8.8.8.8" : "127.0.0.1")};
  });
  network.connect = [&connects](const tcp::endpoint &endpoint, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ++connects;
    EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
    EXPECT_EQ(8198, endpoint.port());
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  const auto result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
      parameters()).wait().result;
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, result.reason);
  EXPECT_EQ(1u, resolves.load());
  EXPECT_EQ(1u, connects.load());
}

TEST(epose_http_client, system_resolver_rejects_localhost_before_connect)
{
  epose_network_v2 network;
  network.connect = [](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ADD_FAILURE() << "System resolver must not permit loopback";
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  EXPECT_EQ(receipt_failure_reason_v2::address_prohibited, client.fetch_descriptor(
      "http://localhost:8197", parameters(), {}).wait().result.reason);
  EXPECT_EQ(receipt_failure_reason_v2::address_prohibited, client.request_service(
      challenge(), signed_descriptor("localhost"), parameters()).wait().result.reason);
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
        std::atomic<unsigned> resolves{0};
        address_list dialed;
        epose_network_v2 network;
        network.resolve = async_answers([&resolves, layout, prefix, v4](const std::string &,
            boost::system::error_code &) {
          ++resolves;
          address_list answers{make_address(prefix + v4)};
          if (layout == 1) answers.push_back(make_address(v4));
          if (layout == 2) answers.insert(answers.begin(), make_address(v4));
          return answers;
        });
        network.connect = [&dialed](const tcp::endpoint &endpoint, boost::asio::io_context &,
            steady::time_point, epose_network_v2::connect_handler) {
          dialed.push_back(endpoint.address());
          return refuse();
        };
        epose_receipt_client_v2 client(network);
        const auto fetched = client.fetch_descriptor("http://directory.example.org:8197",
            parameters(), {}).wait().result;
        const auto probed = client.request_service(challenge(), signed_descriptor("subject.example.org"),
            parameters()).wait().result;
        const auto expected = allowed ? receipt_failure_reason_v2::connection_refused
            : receipt_failure_reason_v2::address_prohibited;
        EXPECT_EQ(expected, fetched.reason);
        EXPECT_EQ(expected, probed.reason);
        EXPECT_EQ(2u, resolves.load());
        address_list expected_dials;
        if (allowed)
          for (unsigned attempt = 0; attempt < 2; ++attempt) // discovery, then service
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
  http_fixture server(http_fixture::mode::reply, http_ok(discovery_json(descriptor)));
  for (const char *second : {"1.1.1.1", "64:ff9b:1::5fd8:ddef", "64:ff9b::5fd8:ddef"})
  {
    SCOPED_TRACE(second);
    std::atomic<unsigned> resolves{0};
    address_list dialed;
    std::vector<steady::time_point> deadlines;
    epose_network_v2 network;
    network.resolve = async_answers([&resolves, second](const std::string &, boost::system::error_code &) {
      ++resolves;
      return address_list{make_address("10.0.0.1"),
          make_address("8.8.8.8"), make_address("fd00::1"), make_address(second)};
    });
    const auto target = server.endpoint();
    network.connect = [&dialed, &deadlines, second, target](const tcp::endpoint &endpoint,
        boost::asio::io_context &io, steady::time_point deadline,
        epose_network_v2::connect_handler complete) -> epose_network_v2::cancel_operation {
      dialed.push_back(endpoint.address());
      deadlines.push_back(deadline);
      EXPECT_EQ(8197, endpoint.port());
      if (endpoint.address() == make_address("8.8.8.8"))
      {
        // Asynchronous failure exercises the same path as a real socket.
        const auto delay = std::make_shared<boost::asio::steady_timer>(io);
        delay->expires_after(ms(10));
        delay->async_wait([delay, complete, &io](const boost::system::error_code &error) {
          if (error) return;
          complete(boost::asio::error::connection_refused, tcp::socket(io));
        });
        return [delay] { delay->cancel(); };
      }
      EXPECT_EQ(make_address(second), endpoint.address());
      return connect_epose_address(target, io, deadline, std::move(complete));
    };
    epose_receipt_client_v2 client(network);
    const auto exchange = client.fetch_descriptor("http://directory.example.org:8197",
        parameters(), commitment(descriptor)).wait();
    EXPECT_TRUE(exchange.result.accepted()) << describe(exchange.result);
    EXPECT_EQ(commitment(descriptor), commitment(exchange.endpoint));
    EXPECT_EQ(1u, resolves.load());
    const address_list expected{make_address("8.8.8.8"), make_address(second)};
    EXPECT_EQ(expected, dialed);
    ASSERT_EQ(2u, deadlines.size());
    for (const auto deadline : deadlines) EXPECT_EQ(deadlines.front(), deadline);
  }
  EXPECT_EQ(3u, server.request_count());
}

TEST(epose_http_client, fallback_preserves_resolver_order_with_ipv4_first)
{
  for (bool asynchronous : {false, true})
  {
    std::atomic<unsigned> resolves{0};
    address_list dialed;
    epose_network_v2 network;
    network.resolve = async_answers([&resolves](const std::string &, boost::system::error_code &) {
      ++resolves;
      return address_list{make_address("2606:4700::1111"),
          make_address("8.8.8.8"), make_address("fd00::1"), make_address("64:ff9b:1::5fd8:ddef"),
          make_address("1.1.1.1"), make_address("127.0.0.1")};
    });
    network.connect = [&dialed, asynchronous](const tcp::endpoint &endpoint, boost::asio::io_context &io,
        steady::time_point, epose_network_v2::connect_handler complete) -> epose_network_v2::cancel_operation {
      dialed.push_back(endpoint.address());
      const boost::system::error_code error = dialed.size() == 4
          ? boost::asio::error::connection_refused : boost::asio::error::network_unreachable;
      if (!asynchronous) throw boost::system::system_error(error);
      boost::asio::post(io, [complete, error, &io] { complete(error, tcp::socket(io)); });
      return [] {};
    };
    epose_receipt_client_v2 client(network);
    const auto result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
        parameters()).wait().result;
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused, result.reason);
    EXPECT_EQ(1u, resolves.load());
    const address_list expected{make_address("8.8.8.8"),
        make_address("1.1.1.1"), make_address("2606:4700::1111"), make_address("64:ff9b:1::5fd8:ddef")};
    EXPECT_EQ(expected, dialed);
  }
}

TEST(epose_http_client, fallback_stops_at_shared_deadline)
{
  hang_counts hung;
  epose_network_v2 network;
  network.resolve = async_answers([](const std::string &, boost::system::error_code &) {
    return address_list{make_address("8.8.8.8"), make_address("1.1.1.1")};
  });
  network.connect = [&hung](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler complete) { return hang(hung, std::move(complete)); };
  epose_receipt_client_v2 client(network);
  const auto started = steady::now();
  const auto result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
      parameters(), caps(ms(30))).wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::connect_timeout)) << describe(result);
  EXPECT_EQ(1u, hung.started.load());
  EXPECT_EQ(1u, hung.cancelled.load());
  EXPECT_GE(elapsed, ms(25));
  EXPECT_LT(elapsed, std::chrono::seconds(1));
}

TEST(epose_http_client, unresolved_lookup_is_bounded_and_does_not_queue_replacements)
{
  held_lookup lookup;
  std::atomic<unsigned> resolves{0}, cancels{0}, connects{0};
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    if (++resolves == 1)
    {
      lookup.hold(io, std::move(complete)); // libc stays blocked after cancel
      return [&cancels] { ++cancels; };
    }
    boost::asio::post(io, [complete] { complete({}, {make_address("8.8.8.8")}); });
    return [] {};
  };
  network.connect = [&connects](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ++connects;
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  auto started = steady::now();
  auto result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
      parameters(), caps(ms(30))).wait().result;
  EXPECT_GE(steady::now() - started, ms(25));
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::dns_timeout)) << describe(result);
  // The stuck lookup still holds the slot. Replacements neither queue nor
  // spend their connect cap: they fail at once as a local outcome.
  for (unsigned i = 0; i < 2; ++i)
  {
    started = steady::now();
    result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
        parameters(), caps(ms(30))).wait().result;
    EXPECT_LT(steady::now() - started, ms(25));
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::dns_slot_busy)) << describe(result);
  }
  EXPECT_EQ(1u, resolves.load());
  EXPECT_EQ(1u, cancels.load());
  EXPECT_EQ(0u, connects.load());
  // The finished jobs are gone. The late answer only frees the resolver slot;
  // it must not dial. The next lookup then runs normally.
  ASSERT_TRUE(lookup.held());
  lookup.deliver({}, {make_address("8.8.8.8")});
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, client.request_service(challenge(),
      signed_descriptor("subject.example.org"), parameters(), caps(ms(1000))).wait().result.reason);
  EXPECT_EQ(2u, resolves.load());
  EXPECT_EQ(1u, connects.load());
}

TEST(epose_http_client, late_completion_after_return_is_ignored_and_capacity_recovers)
{
  held_lookup lookup;
  std::atomic<unsigned> resolves{0}, connects{0};
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    if (++resolves == 1)
    {
      lookup.hold(io, std::move(complete));
      return [] {};
    }
    boost::asio::post(io, [complete] { complete({}, {make_address("8.8.8.8")}); });
    return [] {};
  };
  network.connect = [&connects](const tcp::endpoint &endpoint, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ++connects;
    EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  EXPECT_EQ(receipt_failure_reason_v2::dns_timeout, client.request_service(challenge(),
      signed_descriptor("subject.example.org"), parameters(), caps(ms(20))).wait().result.reason);
  ASSERT_TRUE(lookup.held());
  lookup.deliver({}, {make_address("127.0.0.1")});
  // A fresh request runs after the late completion and resolves normally.
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, client.request_service(challenge(),
      signed_descriptor("subject.example.org"), parameters()).wait().result.reason);
  EXPECT_EQ(2u, resolves.load());
  EXPECT_EQ(1u, connects.load());
}

TEST(epose_http_client, dns_and_connect_share_deadline_and_executor)
{
  boost::asio::io_context *executor = nullptr;
  steady::time_point dialed_deadline, resolving;
  std::atomic<unsigned> connects{0};
  hang_counts hung;
  epose_network_v2 network;
  network.resolve = [&executor, &resolving](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    executor = &io;
    resolving = steady::now();
    const auto delay = std::make_shared<boost::asio::steady_timer>(io);
    delay->expires_after(ms(30));
    delay->async_wait([delay, complete](const boost::system::error_code &error) {
      complete(error, {make_address("8.8.8.8"), make_address("1.1.1.1")});
    });
    return [delay] { delay->cancel(); };
  };
  network.connect = [&](const tcp::endpoint &, boost::asio::io_context &io, steady::time_point deadline,
      epose_network_v2::connect_handler complete) {
    ++connects;
    EXPECT_EQ(executor, &io);
    dialed_deadline = deadline;
    EXPECT_LT(deadline - steady::now(), ms(60));
    return hang(hung, std::move(complete));
  };
  epose_receipt_client_v2 client(network);
  const auto started = steady::now();
  const auto result = client.request_service(challenge(), signed_descriptor("subject.example.org"),
      parameters(), caps(ms(80))).wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::connect_timeout)) << describe(result);
  EXPECT_EQ(1u, connects.load());
  // One connect deadline, fixed when the job starts, covers DNS and connect.
  EXPECT_GE(dialed_deadline, started + ms(80));
  EXPECT_LE(dialed_deadline, resolving + ms(80));
  const auto elapsed = steady::now() - started;
  EXPECT_GE(elapsed, ms(75));
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
    std::atomic<unsigned> connects{0};
    network.connect = [&connects, host](const tcp::endpoint &endpoint, boost::asio::io_context &,
        steady::time_point, epose_network_v2::connect_handler) {
      ++connects;
      EXPECT_EQ(make_address(host), endpoint.address());
      return refuse();
    };
    epose_receipt_client_v2 client(network);
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused,
        client.request_service(challenge(), signed_descriptor(host), parameters()).wait().result.reason);
    EXPECT_EQ(1u, connects.load());
  }
}

TEST(epose_http_client, dns_deadline_classified_for_discovery_and_service)
{
  std::atomic<unsigned> cancels{0};
  epose_network_v2 network;
  network.resolve = [&cancels](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    // Never finishes until cancelled. Exercise the real connect cap in both
    // public callers, including late delivery of operation_aborted.
    return [&io, complete, &cancels] {
      ++cancels;
      boost::asio::post(io, [complete] { complete(boost::asio::error::operation_aborted, {}); });
    };
  };
  epose_receipt_client_v2 client(network);
  const ms cap(EPOSE_V2_CONNECT_TIMEOUT_MS);
  auto started = steady::now();
  auto result = client.fetch_descriptor("http://directory.example.org:8197", parameters(), {}).wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::dns_timeout)) << describe(result);
  EXPECT_NE(receipt_failure_reason_v2::transport_timeout, result.reason);
  EXPECT_EQ(1u, cancels.load());
  EXPECT_GE(steady::now() - started, cap - ms(50));
  // Keep generous scheduler/CI load headroom while still proving that the
  // resolver deadline returns instead of waiting indefinitely.
  EXPECT_LT(steady::now() - started, std::chrono::seconds(20));
  started = steady::now();
  result = client.request_service(challenge(), signed_descriptor("subject.example.org"), parameters()).wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::dns_timeout)) << describe(result);
  EXPECT_NE(receipt_failure_reason_v2::transport_timeout, result.reason);
  EXPECT_EQ(2u, cancels.load());
  EXPECT_GE(steady::now() - started, cap - ms(50));
  EXPECT_LT(steady::now() - started, std::chrono::seconds(20));
}

TEST(epose_http_client, stuck_lookup_fails_only_its_own_peer)
{
  // One hostname whose lookup hangs, hostname peers resolved earlier in this
  // epoch, a hostname never resolved, a stale-epoch resolution and literals.
  http_fixture server(http_fixture::mode::reply, http_ok(service_json()));
  held_lookup lookup;
  std::atomic<unsigned> resolves{0}, cancels{0};
  std::mutex dialed_mutex;
  std::vector<boost::asio::ip::address> dialed;
  epose_network_v2 network;
  network.resolve = [&](const std::string &host, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    ++resolves;
    if (host == "hung.example.org")
    {
      lookup.hold(io, std::move(complete));
      return [&cancels] { ++cancels; };
    }
    const auto answer = host == "alpha.example.org" ? "1.1.1.1"
        : host == "beta.example.org" ? "9.9.9.9" : "8.8.8.8";
    boost::asio::post(io, [complete, answer] { complete({}, {make_address(answer)}); });
    return [] {};
  };
  const auto target = server.endpoint();
  network.connect = [&, target](const tcp::endpoint &endpoint, boost::asio::io_context &io,
      steady::time_point deadline, epose_network_v2::connect_handler complete) {
    {
      std::lock_guard<std::mutex> lock(dialed_mutex);
      dialed.push_back(endpoint.address());
    }
    return connect_epose_address(target, io, deadline, std::move(complete));
  };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  auto epoch45 = challenge();
  epoch45.epoch = 45;

  // Prior resolutions: gamma only in epoch 45 (evicted by the epoch-46 ones),
  // then alpha and beta in epoch 46.
  ASSERT_TRUE(client.request_service(epoch45, signed_descriptor("gamma.example.org"), parameters(),
      caps(ms(1000))).wait().result.accepted());
  for (const char *host : {"alpha.example.org", "beta.example.org"})
    ASSERT_TRUE(client.request_service(challenge(), signed_descriptor(host), parameters(),
        caps(ms(1000))).wait().result.accepted()) << host;
  EXPECT_EQ(3u, resolves.load());

  const ms hung_cap(300);
  const auto started = steady::now();
  auto hung = client.request_service(challenge(), signed_descriptor("hung.example.org"),
      parameters(), caps(hung_cap));
  std::vector<epose_receipt_job_v2> remembered;
  for (const char *host : {"alpha.example.org", "beta.example.org"})
    remembered.push_back(client.request_service(challenge(), signed_descriptor(host),
        parameters(), caps(ms(1000))));
  std::vector<epose_receipt_job_v2> literals;
  for (const char *host : {"8.8.4.4", "2606:4700::1111"})
    literals.push_back(client.request_service(challenge(), signed_descriptor(host),
        parameters(), caps(ms(1000))));
  auto unknown = client.request_service(challenge(), signed_descriptor("delta.example.org"),
      parameters(), caps(ms(1000)));
  auto stale = client.request_service(challenge(), signed_descriptor("gamma.example.org"),
      parameters(), caps(ms(1000)));

  for (const auto &job : remembered)
  {
    const auto exchange = job.wait();
    EXPECT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  }
  for (const auto &job : literals)
  {
    const auto exchange = job.wait();
    EXPECT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  }
  // No remembered resolution (or only one from another epoch): a local
  // dns_slot_busy at once, never a peer dns_timeout.
  for (auto *job : {&unknown, &stale})
  {
    const auto result = job->wait().result;
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::dns_slot_busy)) << describe(result);
  }
  EXPECT_LT(steady::now() - started, hung_cap);
  EXPECT_FALSE(hung.ready());
  EXPECT_EQ(4u, resolves.load()); // nothing queued behind the stuck lookup

  // Only the hung peer fails, on its own connect cap, as dns_timeout.
  const auto result = hung.wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::dns_timeout)) << describe(result);
  EXPECT_GE(steady::now() - started, hung_cap - ms(10));
  EXPECT_EQ(1u, cancels.load());
  {
    std::lock_guard<std::mutex> lock(dialed_mutex);
    // 3 priming dials, then alpha and beta from memory and the 2 literals.
    ASSERT_EQ(7u, dialed.size());
    std::vector<boost::asio::ip::address> later(dialed.begin() + 3, dialed.end());
    std::sort(later.begin(), later.end());
    std::vector<boost::asio::ip::address> expected{make_address("1.1.1.1"), make_address("9.9.9.9"),
        make_address("8.8.4.4"), make_address("2606:4700::1111")};
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(expected, later);
  }

  // A late answer refreshes the memory; the slot is free for the next lookup.
  lookup.deliver({}, {make_address("4.4.4.4")});
  EXPECT_TRUE(client.request_service(challenge(), signed_descriptor("delta.example.org"),
      parameters(), caps(ms(1000))).wait().result.accepted());
  EXPECT_EQ(5u, resolves.load());
  EXPECT_EQ(0u, client.in_flight());
}

TEST(epose_http_client, late_answer_is_remembered_for_its_epoch)
{
  held_lookup lookup;
  std::atomic<unsigned> resolves{0};
  std::atomic<unsigned> connects{0};
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    ++resolves;
    lookup.hold(io, std::move(complete));
    return [] {};
  };
  network.connect = [&connects](const tcp::endpoint &endpoint, boost::asio::io_context &,
      steady::time_point, epose_network_v2::connect_handler) {
    ++connects;
    EXPECT_EQ(make_address("1.1.1.1"), endpoint.address());
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  EXPECT_EQ(receipt_failure_reason_v2::dns_timeout, client.request_service(challenge(),
      signed_descriptor("slow.example.org"), parameters(), caps(ms(20))).wait().result.reason);
  ASSERT_TRUE(lookup.held());
  // Busy and nothing remembered yet.
  EXPECT_EQ(receipt_failure_reason_v2::dns_slot_busy, client.request_service(challenge(),
      signed_descriptor("slow.example.org"), parameters(), caps(ms(200))).wait().result.reason);
  lookup.deliver({}, {make_address("1.1.1.1")});
  // The slot is free again: a fresh lookup holds it, and the late answer from
  // the first lookup lets the same host dial meanwhile.
  auto holder = client.request_service(challenge(), signed_descriptor("other.example.org"),
      parameters(), caps(ms(5000)));
  const auto deadline = steady::now() + std::chrono::seconds(2);
  while (resolves.load() < 2 && steady::now() < deadline)
    std::this_thread::sleep_for(ms(1));
  ASSERT_EQ(2u, resolves.load());
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, client.request_service(challenge(),
      signed_descriptor("slow.example.org"), parameters(), caps(ms(200))).wait().result.reason);
  EXPECT_EQ(1u, connects.load());
  holder.cancel();
  EXPECT_EQ(receipt_failure_reason_v2::cancelled, holder.wait().result.reason);
}

TEST(epose_http_client, literal_peers_run_while_dns_slot_is_stuck)
{
  held_lookup lookup;
  std::atomic<unsigned> resolves{0};
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    ++resolves;
    lookup.hold(io, std::move(complete));
    return [] {};
  };
  network.connect = [](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) { return refuse(); };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  auto stuck = client.request_service(challenge(), signed_descriptor("stuck.example.org"),
      parameters(), caps(ms(5000)));
  const auto started = steady::now();
  const auto waiting = client.request_service(challenge(), signed_descriptor("queued.example.org"),
      parameters(), caps(ms(5000))).wait().result;
  EXPECT_TRUE(same(waiting, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::dns_slot_busy)) << describe(waiting);
  for (const char *host : {"8.8.8.8", "1.1.1.1", "2606:4700::1111"})
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused, client.request_service(challenge(),
        signed_descriptor(host), parameters(), caps(ms(5000))).wait().result.reason) << host;
  EXPECT_LT(steady::now() - started, std::chrono::seconds(1));
  EXPECT_FALSE(stuck.ready());
  EXPECT_EQ(1u, resolves.load()); // the second hostname never queues on the resolver
  stuck.cancel();
  EXPECT_EQ(receipt_failure_reason_v2::cancelled, stuck.wait().result.reason);
  EXPECT_EQ(1u, resolves.load());
}

TEST(epose_http_client, synchronous_resolver_completion_releases_capacity)
{
  std::atomic<unsigned> resolves{0}, connects{0}, cancels{0};
  epose_network_v2 network;
  network.resolve = [&](const std::string &, boost::asio::io_context &,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    ++resolves;
    complete({}, {make_address("8.8.8.8")});
    return [&cancels] { ++cancels; };
  };
  network.connect = [&connects](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ++connects;
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  for (unsigned i = 0; i < 2; ++i)
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused, client.request_service(challenge(),
        signed_descriptor("healthy.example.org"), parameters(), caps(ms(100))).wait().result.reason);
  EXPECT_EQ(2u, resolves.load());
  EXPECT_EQ(2u, connects.load());
  EXPECT_EQ(0u, cancels.load());
}

TEST(epose_http_client, resolver_success_after_deadline_before_timer_is_rejected)
{
  epose_network_v2 network;
  network.resolve = [](const std::string &, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    boost::asio::post(io, [complete] {
      // Occupy the executor across the deadline, then complete before the
      // ready timeout handler gets its turn.
      std::this_thread::sleep_for(ms(60));
      complete({}, {make_address("8.8.8.8")});
    });
    return [] {};
  };
  network.connect = [](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler) {
    ADD_FAILURE() << "expired lookup must not connect";
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  const auto result = client.request_service(challenge(), signed_descriptor("slow.example.org"),
      parameters(), caps(ms(20))).wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::dns_timeout)) << describe(result);
}

TEST(epose_http_client, parallel_jobs_match_mainnet_committee_size)
{
  crypto::hash genesis{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(MAINNET_FINAL_GENESIS_HASH_V2, genesis));
  consensus_parameters_v2 compiled{};
  ASSERT_TRUE(compiled_consensus_parameters_v2(MAINNET, genesis, compiled));
  EXPECT_EQ(compiled.committee.committee_size, EPOSE_V2_RECEIPT_PARALLEL_JOBS);
  EXPECT_EQ(EPOSE_VERIFIER_COMMITTEE_SIZE, EPOSE_V2_RECEIPT_PARALLEL_JOBS);
  epose_receipt_client_v2 client;
  EXPECT_EQ(EPOSE_V2_RECEIPT_PARALLEL_JOBS, client.capacity());
  EXPECT_EQ(0u, client.in_flight());
  EXPECT_EQ(EPOSE_V2_RECEIPT_PARALLEL_JOBS, epose_receipt_client_v2::shared().capacity());
}

TEST(epose_http_client, round_budget_and_stage_deadlines)
{
  EXPECT_EQ(0u, epose_receipt_round_budget_ms_v2(11, 10));
  EXPECT_EQ(T_MS, epose_receipt_round_budget_ms_v2(10, 10));
  EXPECT_EQ(3 * T_MS, epose_receipt_round_budget_ms_v2(10, 12));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
      epose_receipt_round_budget_ms_v2(0, std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
      epose_receipt_round_budget_ms_v2(1, std::numeric_limits<uint64_t>::max()));

  epose_stage_deadlines_v2 d{};
  EXPECT_EQ(ms(EPOSE_V2_CONNECT_TIMEOUT_MS), d.connect);
  EXPECT_EQ(ms(EPOSE_V2_SEND_TIMEOUT_MS), d.send);
  EXPECT_EQ(ms(EPOSE_V2_RECV_IDLE_TIMEOUT_MS), d.recv_idle);
  EXPECT_EQ(EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S, d.recv_min_rate);
  EXPECT_EQ(ms(EPOSE_V2_RECV_RATE_WINDOW_MS), d.recv_rate_window);
  auto before = steady::now();
  d.round_deadline = before + std::chrono::seconds(10);
  auto at = d.stage(ms(100));
  EXPECT_GE(at, before + ms(100));
  EXPECT_LE(at, steady::now() + ms(100));
  // Clipped to the round deadline.
  d.round_deadline = steady::now() + ms(50);
  EXPECT_EQ(d.round_deadline, d.stage(ms(1000)));
  d.round_deadline = steady::now() - ms(1);
  EXPECT_EQ(d.round_deadline, d.stage(ms(1000)));
  // The default round deadline never clips.
  d = {};
  before = steady::now();
  at = d.stage(ms(100));
  EXPECT_GE(at, before + ms(100));
  EXPECT_LE(at, steady::now() + ms(100));
}

TEST(epose_http_client, jitter_window_follows_round_timing)
{
  EXPECT_EQ(0u, epose_receipt_jitter_window_ms_v2(5, 4));
  EXPECT_EQ(T_MS / 100, epose_receipt_jitter_window_ms_v2(10, 10));
  EXPECT_EQ(50 * T_MS / 100, epose_receipt_jitter_window_ms_v2(1, 50));
  EXPECT_EQ(T_MS, epose_receipt_jitter_window_ms_v2(1, 100));
  EXPECT_EQ(T_MS, epose_receipt_jitter_window_ms_v2(0, 1000000));
  EXPECT_EQ(T_MS, epose_receipt_jitter_window_ms_v2(0, std::numeric_limits<uint64_t>::max()));

  crypto::hash genesis{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(MAINNET_FINAL_GENESIS_HASH_V2, genesis));
  consensus_parameters_v2 compiled{};
  ASSERT_TRUE(compiled_consensus_parameters_v2(MAINNET, genesis, compiled));
  const uint64_t stages = EPOSE_V2_CONNECT_TIMEOUT_MS + EPOSE_V2_SEND_TIMEOUT_MS + EPOSE_V2_RECV_IDLE_TIMEOUT_MS;
  ASSERT_GT(compiled.committee.round_count, 0u);
  for (uint64_t round = 0; round < compiled.committee.round_count; ++round)
  {
    SCOPED_TRACE(round);
    uint64_t first = 0, last = 0;
    ASSERT_TRUE(receipt_inclusion_window_v2(compiled.timing, compiled.committee, 2, round, first, last));
    const uint64_t window = epose_receipt_jitter_window_ms_v2(first, last);
    const uint64_t budget = epose_receipt_round_budget_ms_v2(first, last);
    EXPECT_GT(window, 0u);
    EXPECT_LE(window, T_MS);
    EXPECT_LE(window * 100, budget); // at most 1% of the round
    EXPECT_LT(window + stages, budget);
  }
}

TEST(epose_http_client, jitter_offset_is_deterministic_bounded_and_per_peer)
{
  const auto local = key(0);
  const uint64_t window = epose_receipt_jitter_window_ms_v2(1, 200);
  ASSERT_EQ(T_MS, window);
  EXPECT_EQ(0u, epose_receipt_start_offset_ms_v2(local, key(1), 46, 1, 0));
  EXPECT_EQ(0u, epose_receipt_start_offset_ms_v2(local, key(1), 46, 1, 1));

  std::set<uint64_t> offsets;
  uint64_t low = window, high = 0;
  for (unsigned i = 1; i <= 256; ++i)
  {
    const auto peer = key(i);
    const uint64_t offset = epose_receipt_start_offset_ms_v2(local, peer, 46, 1, window);
    EXPECT_EQ(offset, epose_receipt_start_offset_ms_v2(local, peer, 46, 1, window));
    EXPECT_LT(offset, window);
    offsets.insert(offset);
    low = std::min(low, offset);
    high = std::max(high, offset);

    // Documented formula.
    std::string data("QWC_EPOSE_RECEIPT_JITTER_V2");
    data.append(reinterpret_cast<const char *>(&local), sizeof(local));
    data.append(reinterpret_cast<const char *>(&peer), sizeof(peer));
    for (const uint64_t value : {uint64_t(46), uint64_t(1)})
      for (int b = 0; b < 8; ++b)
        data.push_back(static_cast<char>((value >> (8 * b)) & 0xff));
    const crypto::hash digest = crypto::cn_fast_hash(data.data(), data.size());
    uint64_t prefix = 0;
    for (int b = 0; b < 8; ++b)
      prefix |= uint64_t(static_cast<uint8_t>(digest.data[b])) << (8 * b);
    EXPECT_EQ(prefix % window, offset);
  }
  // Different peers get different offsets, spread across the window.
  EXPECT_GE(offsets.size(), 250u);
  EXPECT_LT(low, window / 10);
  EXPECT_GT(high, window * 9 / 10);
  // The start also changes per round, per epoch and per local identity.
  const auto base = epose_receipt_start_offset_ms_v2(local, key(1), 46, 1, window);
  EXPECT_NE(base, epose_receipt_start_offset_ms_v2(local, key(1), 46, 2, window));
  EXPECT_NE(base, epose_receipt_start_offset_ms_v2(local, key(1), 47, 1, window));
  EXPECT_NE(base, epose_receipt_start_offset_ms_v2(key(999), key(1), 46, 1, window));
}

TEST(epose_http_client, dead_peer_does_not_delay_live_peers)
{
  http_fixture server(http_fixture::mode::reply, http_ok(service_json()));
  const ms connect_cap(1000);
  // Production capacity (dead + N - 1 live) and N live peers beside the dead one.
  for (const size_t capacity : {EPOSE_V2_RECEIPT_PARALLEL_JOBS, EPOSE_V2_RECEIPT_PARALLEL_JOBS + 1})
  {
    SCOPED_TRACE(capacity);
    hang_counts hung;
    const auto dead_address = make_address("8.8.4.4");
    const auto target = server.endpoint();
    const unsigned served_before = server.request_count();
    epose_network_v2 network;
    network.connect = [&hung, dead_address, target](const tcp::endpoint &endpoint,
        boost::asio::io_context &io, steady::time_point deadline,
        epose_network_v2::connect_handler complete) {
      if (endpoint.address() == dead_address)
        return hang(hung, std::move(complete));
      return connect_epose_address(target, io, deadline, std::move(complete));
    };
    epose_receipt_client_v2 client(network, capacity);
    const auto started = steady::now();
    auto dead = client.request_service(challenge(), signed_descriptor("8.8.4.4"), parameters(), caps(connect_cap));
    std::vector<epose_receipt_job_v2> live;
    for (size_t i = 0; i + 1 < capacity; ++i)
      live.push_back(client.request_service(challenge(),
          signed_descriptor("1.1.1." + std::to_string(i + 1)), parameters(), caps(connect_cap)));
    for (const auto &job : live)
    {
      const auto exchange = job.wait();
      ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
      EXPECT_EQ(std::string("\x00\xff", 2), exchange.response.block_blob);
      EXPECT_EQ(service_signature(), exchange.response.subject_signature);
    }
    const auto live_elapsed = steady::now() - started;
    EXPECT_LT(live_elapsed, connect_cap);
    EXPECT_FALSE(dead.ready());
    const auto result = dead.wait().result;
    const auto total = steady::now() - started;
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::connect_timeout)) << describe(result);
    EXPECT_GE(total, connect_cap - ms(50));
    EXPECT_LT(total, 2 * connect_cap); // not N x timeout
    EXPECT_EQ(capacity - 1, server.request_count() - served_before);
    EXPECT_EQ(1u, hung.cancelled.load());
    EXPECT_EQ(0u, client.in_flight());
  }
  const auto request = server.last_request();
  EXPECT_EQ(0u, request.find("POST /epose_service_challenge_v2 HTTP/1.1\r\n"));
  EXPECT_NE(std::string::npos, request.find(epee::string_tools::pod_to_hex(challenge().nonce)));
}

TEST(epose_http_client, classifies_connection_refused)
{
  epose_network_v2 network;
  network.connect = redirect(closed_port());
  epose_receipt_client_v2 client(network);
  const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"), parameters()).wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::connection_refused)) << describe(result);
}

TEST(epose_http_client, classifies_connect_timeout)
{
  hang_counts hung;
  epose_network_v2 network;
  network.connect = [&hung](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
      epose_network_v2::connect_handler complete) { return hang(hung, std::move(complete)); };
  epose_receipt_client_v2 client(network);
  const auto started = steady::now();
  const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), caps(ms(200))).wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
      receipt_failure_reason_v2::connect_timeout)) << describe(result);
  EXPECT_NE(receipt_failure_reason_v2::transport_timeout, result.reason);
  EXPECT_GE(elapsed, ms(190));
  EXPECT_LT(elapsed, std::chrono::seconds(2));
  EXPECT_EQ(1u, hung.cancelled.load());
}

TEST(epose_http_client, classifies_send_timeout)
{
  http_fixture server(http_fixture::mode::never_read);
  const auto target = server.endpoint();
  epose_network_v2 network;
  network.connect = [target](const tcp::endpoint &, boost::asio::io_context &io, steady::time_point deadline,
      epose_network_v2::connect_handler complete) {
    return connect_epose_address(target, io, deadline,
        [complete](boost::system::error_code error, tcp::socket socket) {
      if (!error)
        fill_send_queue(socket);
      complete(error, std::move(socket));
    });
  };
  epose_receipt_client_v2 client(network);
  const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), caps(ms(5000), ms(200))).wait().result;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_send,
      receipt_failure_reason_v2::send_timeout)) << describe(result);
  EXPECT_NE(receipt_failure_reason_v2::transport_timeout, result.reason);
}

TEST(epose_http_client, classifies_recv_timeout)
{
  http_fixture server(http_fixture::mode::silent);
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network);
  const auto started = steady::now();
  const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), caps(ms(1000), ms(1000), ms(200))).wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
      receipt_failure_reason_v2::recv_timeout)) << describe(result);
  EXPECT_NE(receipt_failure_reason_v2::transport_timeout, result.reason);
  EXPECT_EQ(1u, server.request_count());
  EXPECT_GE(elapsed, ms(190));
  EXPECT_LT(elapsed, std::chrono::seconds(2));
}

namespace
{
  // An HTTP service reply whose block blob is `blob_bytes` long.
  std::string service_reply_of(size_t blob_bytes)
  {
    std::string hex;
    hex.reserve(2 * blob_bytes);
    for (size_t i = 0; i < blob_bytes; ++i)
      hex += "ab";
    return http_ok(service_json(hex));
  }

  // A full-size service response: a 2 MiB block as 4 MiB of hex.
  std::string full_size_service_reply()
  {
    return service_reply_of(2 * 1024 * 1024);
  }
}

TEST(epose_http_client, slow_peer_delivers_full_response)
{
  const std::string reply = full_size_service_reply();
  ASSERT_GT(reply.size(), size_t(4 * 1024 * 1024));
  http_fixture::trickle_plan plan;
  plan.chunk = reply.size() / 16 + 1;
  plan.gap = ms(60); // each gap well inside the idle bound
  http_fixture server(http_fixture::mode::trickle, reply, plan);
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network);
  const ms idle(250);
  auto deadlines = caps(ms(1000), ms(1000), idle);
  const auto started = steady::now();
  deadlines.round_deadline = started + std::chrono::seconds(20);
  const auto exchange = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), deadlines).wait();
  const auto elapsed = steady::now() - started;
  ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  EXPECT_EQ(size_t(2 * 1024 * 1024), exchange.response.block_blob.size());
  // Longer in total than the idle bound: recv is not a fixed total cap.
  EXPECT_GT(elapsed, 3 * idle);
  EXPECT_LT(elapsed, std::chrono::seconds(20));
}

TEST(epose_http_client, stalled_peer_fails_recv_timeout_after_progress)
{
  const std::string reply = full_size_service_reply();
  http_fixture::trickle_plan plan;
  plan.chunk = reply.size() / 16 + 1;
  plan.gap = ms(60);
  plan.stall_after = reply.size() / 2;
  http_fixture server(http_fixture::mode::trickle, reply, plan);
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network);
  const ms idle(250);
  auto deadlines = caps(ms(1000), ms(1000), idle);
  const auto started = steady::now();
  deadlines.round_deadline = started + std::chrono::seconds(20);
  const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), deadlines).wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
      receipt_failure_reason_v2::recv_timeout)) << describe(result);
  // About 8 chunks x 60 ms of progress, then the idle bound.
  EXPECT_GT(elapsed, idle + ms(300));
  EXPECT_LT(elapsed, std::chrono::seconds(10));
}

TEST(epose_http_client, classifies_bad_responses)
{
  struct reply_case
  {
    std::string reply;
    receipt_attempt_stage_v2 stage;
    receipt_failure_reason_v2 reason;
  };
  const std::vector<reply_case> cases{
      {"garbage\r\n\r\n", receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed},
      {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
          receipt_attempt_stage_v2::response_decode, receipt_failure_reason_v2::response_malformed},
      {"HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nxy",
          receipt_attempt_stage_v2::response_decode, receipt_failure_reason_v2::response_malformed},
      {http_ok("nope!"), receipt_attempt_stage_v2::response_decode,
          receipt_failure_reason_v2::response_malformed},
      {"HTTP/1.1 200 OK\r\nContent-Length: 999999999\r\n\r\n",
          receipt_attempt_stage_v2::response_decode, receipt_failure_reason_v2::response_oversized},
      {"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n",
          receipt_attempt_stage_v2::http_exchange, receipt_failure_reason_v2::http_status_unsuccessful},
      {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort",
          receipt_attempt_stage_v2::network_recv, receipt_failure_reason_v2::transport_failure},
      {http_ok(service_json()), receipt_attempt_stage_v2::none, receipt_failure_reason_v2::none}};
  for (const auto &c : cases)
  {
    SCOPED_TRACE(c.reply.substr(0, 40));
    http_fixture server(http_fixture::mode::reply, c.reply);
    epose_network_v2 network;
    network.connect = redirect(server.endpoint());
    epose_receipt_client_v2 client(network);
    const auto exchange = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
        parameters(), caps(ms(1000), ms(1000), ms(1000))).wait();
    EXPECT_TRUE(same(exchange.result, c.stage, c.reason)) << describe(exchange.result);
    if (!exchange.result.accepted())
      EXPECT_TRUE(exchange.response.block_blob.empty());
    EXPECT_EQ(1u, server.request_count());
  }
}

TEST(epose_http_client, round_deadline_clips_every_stage)
{
  {
    hang_counts hung;
    epose_network_v2 network;
    network.connect = [&hung](const tcp::endpoint &, boost::asio::io_context &, steady::time_point,
        epose_network_v2::connect_handler complete) { return hang(hung, std::move(complete)); };
    epose_receipt_client_v2 client(network);
    auto deadlines = caps(ms(5000));
    const auto started = steady::now();
    deadlines.round_deadline = started + ms(100);
    const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
        parameters(), deadlines).wait().result;
    const auto elapsed = steady::now() - started;
    // The round ran out, not the peer's connect cap.
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::deadline_expired)) << describe(result);
    EXPECT_GE(elapsed, ms(95));
    EXPECT_LT(elapsed, std::chrono::seconds(2));

    deadlines.round_deadline = steady::now() - ms(1);
    const auto expired = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
        parameters(), deadlines).wait().result;
    EXPECT_TRUE(same(expired, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::deadline_expired)) << describe(expired);
    EXPECT_EQ(1u, hung.started.load());
  }
  {
    http_fixture server(http_fixture::mode::silent);
    epose_network_v2 network;
    network.connect = redirect(server.endpoint());
    epose_receipt_client_v2 client(network);
    auto deadlines = caps(ms(5000), ms(5000), ms(5000));
    const auto started = steady::now();
    deadlines.round_deadline = started + ms(300);
    const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
        parameters(), deadlines).wait().result;
    const auto elapsed = steady::now() - started;
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
        receipt_failure_reason_v2::deadline_expired)) << describe(result);
    EXPECT_GE(elapsed, ms(290));
    EXPECT_LT(elapsed, std::chrono::seconds(2));
  }
}

TEST(epose_http_client, lease_exhaustion_is_a_local_failure)
{
  hang_counts hung;
  std::atomic<unsigned> refused{0};
  epose_network_v2 network;
  network.connect = [&hung, &refused](const tcp::endpoint &endpoint, boost::asio::io_context &,
      steady::time_point, epose_network_v2::connect_handler complete) -> epose_network_v2::cancel_operation {
    if (endpoint.address() == make_address("8.8.4.4"))
      return hang(hung, std::move(complete));
    ++refused;
    return refuse();
  };
  {
    epose_receipt_client_v2 client(network, 1);
    auto held = client.request_service(challenge(), signed_descriptor("8.8.4.4"),
        parameters(), caps(ms(10000)));
    EXPECT_EQ(1u, client.in_flight());
    const auto started = steady::now();
    auto rejected = client.request_service(challenge(), signed_descriptor("8.8.8.8"), parameters());
    EXPECT_TRUE(rejected.ready()); // never waits for a slot
    const auto result = rejected.wait().result;
    EXPECT_LT(steady::now() - started, ms(100));
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::local_lease,
        receipt_failure_reason_v2::lease_exhausted)) << describe(result);
    EXPECT_NE(receipt_failure_reason_v2::transport_timeout, result.reason);
    EXPECT_NE(receipt_failure_reason_v2::connect_timeout, result.reason);
    EXPECT_NE(receipt_attempt_stage_v2::network_connect, result.stage);
    EXPECT_EQ(0u, refused.load());
    EXPECT_EQ("lease_exhausted", std::string(to_string(result.reason)));
    EXPECT_EQ("local_lease", std::string(to_string(result.stage)));

    held.cancel();
    const auto cancelled = held.wait().result;
    EXPECT_TRUE(same(cancelled, receipt_attempt_stage_v2::none,
        receipt_failure_reason_v2::cancelled)) << describe(cancelled);
    EXPECT_EQ(0u, client.in_flight());
    // The slot is back.
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused, client.request_service(challenge(),
        signed_descriptor("8.8.8.8"), parameters()).wait().result.reason);
    EXPECT_EQ(1u, refused.load());
  }
  // Production capacity: N jobs run, job N + 1 is a local lease failure, and
  // destroying the client cancels the jobs still in flight.
  std::vector<epose_receipt_job_v2> jobs;
  {
    epose_receipt_client_v2 client(network);
    for (size_t i = 0; i < EPOSE_V2_RECEIPT_PARALLEL_JOBS; ++i)
      jobs.push_back(client.request_service(challenge(), signed_descriptor("8.8.4.4"),
          parameters(), caps(ms(10000))));
    EXPECT_EQ(EPOSE_V2_RECEIPT_PARALLEL_JOBS, client.in_flight());
    const auto extra = client.request_service(challenge(), signed_descriptor("8.8.8.8"), parameters()).wait().result;
    EXPECT_TRUE(same(extra, receipt_attempt_stage_v2::local_lease,
        receipt_failure_reason_v2::lease_exhausted)) << describe(extra);
    EXPECT_NE(receipt_failure_reason_v2::transport_timeout, extra.reason);
  }
  for (const auto &job : jobs)
    EXPECT_EQ(receipt_failure_reason_v2::cancelled, job.wait().result.reason);
  EXPECT_EQ(1u, refused.load());
}

TEST(epose_http_client, recv_total_bound_arithmetic)
{
  constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(1000u, epose_receipt_recv_total_ms_v2(32768, 0, 32768));
  EXPECT_EQ(1250u, epose_receipt_recv_total_ms_v2(32768, 250, 32768));
  EXPECT_EQ(334u, epose_receipt_recv_total_ms_v2(1, 0, 3)); // rounded up
  EXPECT_EQ(0u, epose_receipt_recv_total_ms_v2(0, 0, 3));
  EXPECT_EQ(max, epose_receipt_recv_total_ms_v2(1, 0, 0)); // floor disabled
  EXPECT_EQ(max, epose_receipt_recv_total_ms_v2(max, 0, 1));
  EXPECT_EQ(max, epose_receipt_recv_total_ms_v2(1000, max, 1000));
  EXPECT_EQ(max / 1000 * 1000, epose_receipt_recv_total_ms_v2(max / 1000, 0, 1));

  // The largest service exchange at the floor, plus a descriptor lookup and
  // both connects and sends, stays under 2.5% of every compiled mainnet round.
  const uint64_t service = epose_receipt_recv_total_ms_v2(EPOSE_V2_MAX_SERVICE_RECV_BYTES,
      EPOSE_V2_RECV_IDLE_TIMEOUT_MS, EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S);
  const uint64_t descriptor = epose_receipt_recv_total_ms_v2(EPOSE_V2_MAX_DESCRIPTOR_RECV_BYTES,
      EPOSE_V2_RECV_IDLE_TIMEOUT_MS, EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S);
  EXPECT_EQ(518500u, service);
  EXPECT_EQ(6500u, descriptor);
  crypto::hash genesis{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(MAINNET_FINAL_GENESIS_HASH_V2, genesis));
  consensus_parameters_v2 compiled{};
  ASSERT_TRUE(compiled_consensus_parameters_v2(MAINNET, genesis, compiled));
  ASSERT_GT(compiled.committee.round_count, 0u);
  for (uint64_t round = 0; round < compiled.committee.round_count; ++round)
  {
    SCOPED_TRACE(round);
    uint64_t first = 0, last = 0;
    ASSERT_TRUE(receipt_inclusion_window_v2(compiled.timing, compiled.committee, 2, round, first, last));
    const uint64_t budget = epose_receipt_round_budget_ms_v2(first, last);
    EXPECT_LE(EPOSE_V2_CONNECT_TIMEOUT_MS + EPOSE_V2_SEND_TIMEOUT_MS + descriptor
        + EPOSE_V2_CONNECT_TIMEOUT_MS + EPOSE_V2_SEND_TIMEOUT_MS + service, budget / 40);
  }
}

namespace
{
  // One byte per tick: never idle long enough for the inactivity bound.
  http_fixture::trickle_plan byte_per_tick()
  {
    http_fixture::trickle_plan plan;
    plan.chunk = 1;
    plan.gap = ms(20);
    return plan;
  }

  epose_stage_deadlines_v2 floor_caps(uint64_t rate, ms window, steady::time_point round_deadline)
  {
    auto out = caps(ms(1000), ms(1000), ms(1000));
    out.recv_min_rate = rate;
    out.recv_rate_window = window;
    out.round_deadline = round_deadline;
    return out;
  }
}

TEST(epose_http_client, byte_per_tick_trickler_is_cut_off)
{
  const std::string reply = http_ok(service_json());
  ASSERT_GT(reply.size(), 200u); // > 4 s at one byte per 20 ms
  http_fixture server(http_fixture::mode::trickle, reply, byte_per_tick());
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network, 1);
  const auto started = steady::now();
  const auto round_deadline = started + std::chrono::seconds(30);
  const ms window(250);
  auto job = client.request_service(challenge(), signed_descriptor("8.8.8.8"), parameters(),
      floor_caps(EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S, window, round_deadline));
  EXPECT_EQ(1u, client.in_flight());
  const auto result = job.wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
      receipt_failure_reason_v2::recv_too_slow)) << describe(result);
  EXPECT_GE(elapsed, window);
  EXPECT_LT(elapsed, std::chrono::seconds(3)); // the inactivity bound never fired
  EXPECT_EQ(0u, client.in_flight());
  // The single slot is free again well before the round deadline.
  const auto next = client.request_service(challenge(), signed_descriptor("8.8.8.8"), parameters(),
      floor_caps(EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S, window, round_deadline)).wait().result;
  EXPECT_NE(receipt_failure_reason_v2::lease_exhausted, next.reason) << describe(next);
}

TEST(epose_http_client, trickler_hits_total_receive_bound)
{
  // Without the window check the total bound still ends the receive: the
  // declared Content-Length at the floor, plus the idle slack.
  const std::string reply = http_ok(service_json());
  http_fixture::trickle_plan plan;
  plan.chunk = 1;
  plan.gap = ms(10);
  http_fixture server(http_fixture::mode::trickle, reply, plan);
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network);
  constexpr uint64_t rate = 200; // bytes per second
  const ms idle(500);
  const auto bound = ms(epose_receipt_recv_total_ms_v2(reply.size(), idle.count(), rate));
  ASSERT_GT(plan.gap * reply.size(), bound + ms(500)); // the reply cannot finish in time
  const auto started = steady::now();
  auto deadlines = floor_caps(rate, ms(0), started + std::chrono::seconds(30));
  deadlines.recv_idle = idle;
  const auto result = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), deadlines).wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
      receipt_failure_reason_v2::recv_too_slow)) << describe(result);
  EXPECT_GE(elapsed, bound - ms(50));
  EXPECT_LT(elapsed, bound + ms(500));
  EXPECT_EQ(0u, client.in_flight());
}

TEST(epose_http_client, tricklers_cannot_starve_an_honest_subject)
{
  const std::string reply = http_ok(service_json());
  http_fixture trickler(http_fixture::mode::trickle, reply, byte_per_tick());
  http_fixture honest(http_fixture::mode::reply, reply);
  const auto slow = trickler.endpoint();
  const auto fast = honest.endpoint();
  const auto honest_address = make_address("1.1.1.1");
  epose_network_v2 network;
  network.connect = [slow, fast, honest_address](const tcp::endpoint &endpoint,
      boost::asio::io_context &io, steady::time_point deadline,
      epose_network_v2::connect_handler complete) {
    return connect_epose_address(endpoint.address() == honest_address ? fast : slow,
        io, deadline, std::move(complete));
  };
  constexpr size_t N = 3;
  epose_receipt_client_v2 client(network, N);
  const auto started = steady::now();
  const auto round_deadline = started + std::chrono::seconds(30);
  const auto deadlines = floor_caps(EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S, ms(250), round_deadline);
  std::vector<epose_receipt_job_v2> tricklers;
  for (size_t i = 0; i < N; ++i)
    tricklers.push_back(client.request_service(challenge(),
        signed_descriptor("8.8.4." + std::to_string(i + 1)), parameters(), deadlines));
  // Every lease is held by a trickler.
  const auto refused = client.request_service(challenge(), signed_descriptor("1.1.1.1"),
      parameters(), deadlines).wait().result;
  EXPECT_TRUE(same(refused, receipt_attempt_stage_v2::local_lease,
      receipt_failure_reason_v2::lease_exhausted)) << describe(refused);
  // The scheduler's retry: the honest subject gets a lease once the floor
  // cuts the tricklers off, long before the round deadline.
  epose_receipt_exchange_v2 exchange{};
  while (steady::now() < round_deadline)
  {
    exchange = client.request_service(challenge(), signed_descriptor("1.1.1.1"),
        parameters(), deadlines).wait();
    if (exchange.result.reason != receipt_failure_reason_v2::lease_exhausted)
      break;
    std::this_thread::sleep_for(ms(10));
  }
  ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  EXPECT_EQ(std::string("\x00\xff", 2), exchange.response.block_blob);
  EXPECT_LT(steady::now() - started, std::chrono::seconds(3));
  for (const auto &job : tricklers)
  {
    const auto result = job.wait().result;
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
        receipt_failure_reason_v2::recv_too_slow)) << describe(result);
  }
  EXPECT_EQ(1u, honest.request_count());
  EXPECT_EQ(0u, client.in_flight());
}

TEST(epose_http_client, slow_full_size_response_above_floor_succeeds)
{
  const std::string reply = full_size_service_reply();
  ASSERT_GT(reply.size(), size_t(4 * 1024 * 1024));
  http_fixture::trickle_plan plan;
  plan.chunk = 64 * 1024;
  plan.gap = ms(30); // about 2 MiB/s, 4x the floor below
  http_fixture server(http_fixture::mode::trickle, reply, plan);
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network);
  const auto started = steady::now();
  auto deadlines = floor_caps(512 * 1024, ms(250), started + std::chrono::seconds(30));
  const auto exchange = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
      parameters(), deadlines).wait();
  const auto elapsed = steady::now() - started;
  ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  EXPECT_EQ(size_t(2 * 1024 * 1024), exchange.response.block_blob.size());
  // Many windows long: the floor is a rate, not a total cap.
  EXPECT_GT(elapsed, 4 * deadlines.recv_rate_window);
}

TEST(epose_http_client, trickling_discovery_url_falls_through_to_the_next)
{
  const auto expected = signed_descriptor();
  const std::string reply = http_ok(discovery_json(expected));
  http_fixture trickler(http_fixture::mode::trickle, reply, byte_per_tick());
  http_fixture honest(http_fixture::mode::reply, reply);
  const auto slow = trickler.endpoint();
  const auto fast = honest.endpoint();
  epose_network_v2 network;
  network.connect = [slow, fast](const tcp::endpoint &endpoint, boost::asio::io_context &io,
      steady::time_point deadline, epose_network_v2::connect_handler complete) {
    return connect_epose_address(endpoint.port() == 8197 ? slow : fast, io, deadline,
        std::move(complete));
  };
  epose_receipt_client_v2 client(network);
  auto request = discovery_only({"http://8.8.8.8:8197", "http://8.8.4.4:8199"});
  request.challenge.endpoint_descriptor_hash = commitment(expected);
  const auto started = steady::now();
  request.deadlines = floor_caps(EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S, ms(250),
      started + std::chrono::seconds(30));
  const auto exchange = client.submit(request).wait();
  ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
  EXPECT_EQ(commitment(expected), commitment(exchange.endpoint));
  EXPECT_EQ(1u, trickler.request_count());
  EXPECT_EQ(1u, honest.request_count());
  EXPECT_LT(steady::now() - started, std::chrono::seconds(3));
}

TEST(epose_http_client, concurrent_jobs_for_one_host_cost_one_lookup)
{
  // The first round of an epoch: many subjects on one host, nothing cached.
  held_lookup lookup;
  std::atomic<unsigned> resolves{0}, cancels{0}, connects{0};
  epose_network_v2 network;
  network.resolve = [&](const std::string &host, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    EXPECT_EQ("shared.example.org", host);
    ++resolves;
    lookup.hold(io, std::move(complete));
    return [&cancels] { ++cancels; };
  };
  network.connect = [&connects](const tcp::endpoint &endpoint, boost::asio::io_context &,
      steady::time_point, epose_network_v2::connect_handler) {
    ++connects;
    EXPECT_EQ(make_address("8.8.8.8"), endpoint.address());
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  std::vector<epose_receipt_job_v2> jobs;
  // Service challenges and discovery lookups of the same host.
  for (size_t i = 0; i < EPOSE_V2_RECEIPT_PARALLEL_JOBS; ++i)
    jobs.push_back(i % 3 == 2
        ? client.fetch_descriptor("http://shared.example.org:8197", parameters(), crypto::hash{},
            caps(ms(5000)))
        : client.request_service(challenge(), signed_descriptor("shared.example.org"),
            parameters(), caps(ms(5000))));
  ASSERT_GE(jobs.size(), 3u);
  const auto until = steady::now() + std::chrono::seconds(2);
  while (!lookup.held() && steady::now() < until)
    std::this_thread::sleep_for(ms(1));
  ASSERT_TRUE(lookup.held());
  std::this_thread::sleep_for(ms(50));
  for (const auto &job : jobs)
    EXPECT_FALSE(job.ready()); // attached, not dns_slot_busy
  lookup.deliver({}, {make_address("8.8.8.8")});
  for (const auto &job : jobs)
    EXPECT_EQ(receipt_failure_reason_v2::connection_refused, job.wait().result.reason);
  EXPECT_EQ(1u, resolves.load());
  EXPECT_EQ(0u, cancels.load());
  EXPECT_EQ(EPOSE_V2_RECEIPT_PARALLEL_JOBS, connects.load());
}

namespace
{
  // Every lookup is held; connects are refused and counted.
  struct held_network
  {
    held_lookup lookup;
    std::atomic<unsigned> resolves{0}, cancels{0}, connects{0};

    epose_network_v2 hooks()
    {
      epose_network_v2 network;
      network.resolve = [this](const std::string &, boost::asio::io_context &io,
          epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
        ++resolves;
        lookup.hold(io, std::move(complete));
        return [this] { ++cancels; };
      };
      network.connect = [this](const tcp::endpoint &, boost::asio::io_context &,
          steady::time_point, epose_network_v2::connect_handler) {
        ++connects;
        return refuse();
      };
      return network;
    }

    void wait_held()
    {
      const auto until = steady::now() + std::chrono::seconds(2);
      while (!lookup.held() && steady::now() < until)
        std::this_thread::sleep_for(ms(1));
      ASSERT_TRUE(lookup.held());
    }
  };
}

TEST(epose_http_client, last_waiter_cancels_shared_lookup)
{
  // The owner gives up first, the waiter gives up first, or both give up.
  struct timing { ms owner, waiter; bool both_give_up; };
  for (const auto &t : {timing{ms(50), ms(5000), false}, timing{ms(5000), ms(50), false},
           timing{ms(40), ms(80), true}})
  {
    SCOPED_TRACE(t.owner.count());
    held_network net;
    epose_receipt_client_v2 client(net.hooks());
    deliver_on_exit cleanup{net.lookup};
    const auto descriptor = signed_descriptor("shared.example.org");
    auto owner = client.request_service(challenge(), descriptor, parameters(), caps(t.owner));
    net.wait_held();
    auto waiter = client.request_service(challenge(), descriptor, parameters(), caps(t.waiter));
    auto &first = t.owner < t.waiter ? owner : waiter;
    auto &second = t.owner < t.waiter ? waiter : owner;
    EXPECT_TRUE(same(first.wait().result, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::dns_timeout)) << describe(first.wait().result);
    if (!t.both_give_up)
    {
      // The other job still waits: the lookup stays alive for it.
      EXPECT_FALSE(second.ready());
      EXPECT_EQ(0u, net.cancels.load());
      net.lookup.deliver({}, {make_address("8.8.8.8")});
      EXPECT_EQ(receipt_failure_reason_v2::connection_refused, second.wait().result.reason);
      EXPECT_EQ(1u, net.connects.load());
    }
    else
    {
      EXPECT_EQ(receipt_failure_reason_v2::dns_timeout, second.wait().result.reason);
      EXPECT_EQ(1u, net.cancels.load());
      // A cancelled lookup takes no new waiters, even for its own host.
      EXPECT_EQ(receipt_failure_reason_v2::dns_slot_busy, client.request_service(challenge(),
          descriptor, parameters(), caps(ms(5000))).wait().result.reason);
      EXPECT_EQ(0u, net.connects.load());
    }
    EXPECT_EQ(1u, net.resolves.load());
  }
}

TEST(epose_http_client, resolutions_from_earlier_epochs_are_evicted)
{
  std::atomic<unsigned> resolves{0};
  held_lookup lookup;
  std::mutex dialed_mutex;
  std::vector<boost::asio::ip::address> dialed;
  epose_network_v2 network;
  network.resolve = [&](const std::string &host, boost::asio::io_context &io,
      epose_network_v2::resolve_handler complete) -> epose_network_v2::cancel_resolve {
    ++resolves;
    if (host == "held.example.org")
    {
      lookup.hold(io, std::move(complete));
      return [] {};
    }
    const auto answer = host == "old.example.org" ? "1.1.1.1"
        : host == "new.example.org" ? "9.9.9.9" : "8.8.8.8";
    boost::asio::post(io, [complete, answer] { complete({}, {make_address(answer)}); });
    return [] {};
  };
  network.connect = [&](const tcp::endpoint &endpoint, boost::asio::io_context &,
      steady::time_point, epose_network_v2::connect_handler) {
    {
      std::lock_guard<std::mutex> lock(dialed_mutex);
      dialed.push_back(endpoint.address());
    }
    return refuse();
  };
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{lookup};
  auto epoch47 = challenge();
  epoch47.epoch = 47;
  const auto refused = [&](const service_challenge_v2 &c, const char *host) {
    return client.request_service(c, signed_descriptor(host), parameters(),
        caps(ms(1000))).wait().result.reason;
  };
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, refused(challenge(), "old.example.org"));
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, refused(epoch47, "new.example.org"));
  // A late request of epoch 46 resolves, but its answer is never remembered.
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, refused(challenge(), "late.example.org"));
  EXPECT_EQ(3u, resolves.load());

  auto holder = client.request_service(epoch47, signed_descriptor("held.example.org"),
      parameters(), caps(ms(5000)));
  const auto until = steady::now() + std::chrono::seconds(2);
  while (!lookup.held() && steady::now() < until)
    std::this_thread::sleep_for(ms(1));
  ASSERT_TRUE(lookup.held());
  // Epoch 47 dials from memory; the epoch-46 entries are gone.
  EXPECT_EQ(receipt_failure_reason_v2::connection_refused, refused(epoch47, "new.example.org"));
  EXPECT_EQ(receipt_failure_reason_v2::dns_slot_busy, refused(challenge(), "old.example.org"));
  EXPECT_EQ(receipt_failure_reason_v2::dns_slot_busy, refused(challenge(), "late.example.org"));
  EXPECT_EQ(4u, resolves.load());
  {
    std::lock_guard<std::mutex> lock(dialed_mutex);
    const std::vector<boost::asio::ip::address> expected{make_address("1.1.1.1"),
        make_address("9.9.9.9"), make_address("8.8.8.8"), make_address("9.9.9.9")};
    EXPECT_EQ(expected, dialed);
  }
  holder.cancel();
  EXPECT_EQ(receipt_failure_reason_v2::cancelled, holder.wait().result.reason);
}

TEST(epose_http_client, busy_resolver_slot_never_hides_a_real_discovery_failure)
{
  held_network net;
  epose_receipt_client_v2 client(net.hooks());
  deliver_on_exit cleanup{net.lookup};
  // Another host's lookup holds the slot.
  auto holder = client.request_service(challenge(), signed_descriptor("held.example.org"),
      parameters(), caps(ms(10000)));
  net.wait_held();
  using urls = std::vector<std::string>;
  for (const auto &discovery : {urls{"http://8.8.4.4:8197", "http://other.example.org:8197"},
           urls{"http://other.example.org:8197", "http://8.8.4.4:8197"}})
  {
    SCOPED_TRACE(discovery.front());
    const auto result = client.submit(discovery_only(discovery)).wait().result;
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_connect,
        receipt_failure_reason_v2::connection_refused)) << describe(result);
  }
  EXPECT_EQ(2u, net.connects.load());
  EXPECT_EQ(1u, net.resolves.load());
  holder.cancel();
  EXPECT_EQ(receipt_failure_reason_v2::cancelled, holder.wait().result.reason);
}

TEST(epose_http_client, production_floor_values)
{
  EXPECT_EQ(8u * 1024, EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S);
  EXPECT_EQ(2000u, EPOSE_V2_RECV_RATE_WINDOW_MS);
  EXPECT_EQ(4000u, EPOSE_V2_RECV_IDLE_TIMEOUT_MS);
  // Four verifiers fetching at once need 262.2 kbit/s; fifteen fit in 1 Mbit/s.
  EXPECT_LE(4 * EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S * 8, 263u * 1000);
  EXPECT_LE(15 * EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S * 8, 1000u * 1000);
}

TEST(epose_http_client, stall_under_the_idle_bound_mid_body_succeeds)
{
  // An honest peer stalls 3 s mid-body (a TCP retransmission backoff), under
  // the 4 s idle bound, then resumes; at the production floor and a 4x higher
  // one. The reads after the stall see more than one span of history.
  struct pacing { uint64_t floor; size_t chunk; ms gap; };
  for (const auto &p : {pacing{EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S, 4 * 1024, ms(62)},   // 64 KiB/s
           pacing{32 * 1024, 8 * 1024, ms(31)}})                                       // 256 KiB/s
  {
    SCOPED_TRACE(p.floor);
    const size_t per_second = p.chunk * 1000 / static_cast<size_t>(p.gap.count());
    const std::string reply = service_reply_of(per_second * 9 / 4); // 4.5 s of hex
    http_fixture::trickle_plan plan;
    plan.chunk = p.chunk;
    plan.gap = p.gap;
    plan.pause_at = reply.size() * 7 / 9; // after 3.5 s
    plan.pause = ms(3000);
    http_fixture server(http_fixture::mode::trickle, reply, plan);
    epose_network_v2 network;
    network.connect = redirect(server.endpoint());
    epose_receipt_client_v2 client(network);
    epose_stage_deadlines_v2 deadlines{};
    ASSERT_EQ(ms(2000), deadlines.recv_rate_window);
    ASSERT_EQ(ms(4000), deadlines.recv_idle);
    ASSERT_LT(plan.pause, deadlines.recv_idle);
    deadlines.recv_min_rate = p.floor;
    const auto started = steady::now();
    deadlines.round_deadline = started + std::chrono::seconds(60);
    const auto exchange = client.request_service(challenge(), signed_descriptor("8.8.8.8"),
        parameters(), deadlines).wait();
    const auto elapsed = steady::now() - started;
    ASSERT_TRUE(exchange.result.accepted()) << describe(exchange.result);
    EXPECT_EQ(per_second * 9 / 4, exchange.response.block_blob.size());
    EXPECT_GT(elapsed, deadlines.recv_idle + deadlines.recv_rate_window);
    EXPECT_LT(elapsed, std::chrono::seconds(20));
    EXPECT_EQ(0u, client.in_flight());
  }
}

TEST(epose_http_client, trickler_is_cut_off_after_one_production_span)
{
  // Production deadlines and a full-size Content-Length, so only the floor
  // span (not the total bound) can end the receive.
  const std::string reply = full_size_service_reply();
  http_fixture::trickle_plan plan;
  plan.chunk = 1;
  plan.gap = ms(50);
  http_fixture server(http_fixture::mode::trickle, reply, plan);
  epose_network_v2 network;
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network, 1);
  epose_stage_deadlines_v2 deadlines{};
  const auto span = deadlines.recv_idle + deadlines.recv_rate_window;
  const auto started = steady::now();
  deadlines.round_deadline = started + std::chrono::seconds(120);
  auto job = client.request_service(challenge(), signed_descriptor("8.8.8.8"), parameters(),
      deadlines);
  EXPECT_EQ(1u, client.in_flight());
  const auto result = job.wait().result;
  const auto elapsed = steady::now() - started;
  EXPECT_TRUE(same(result, receipt_attempt_stage_v2::network_recv,
      receipt_failure_reason_v2::recv_too_slow)) << describe(result);
  EXPECT_GE(elapsed, span);
  EXPECT_LT(elapsed, span + std::chrono::seconds(2));
  EXPECT_EQ(0u, client.in_flight());
}

TEST(epose_http_client, busy_resolver_slot_never_hides_a_real_unavailable_descriptor)
{
  COMMAND_RPC_GET_EPOSE_SERVICE_ENDPOINT_V2::response not_ready{};
  not_ready.status = CORE_RPC_STATUS_OK;
  not_ready.ready = false;
  std::string json;
  ASSERT_TRUE(epee::serialization::store_t_to_json(not_ready, json));
  http_fixture server(http_fixture::mode::reply, http_ok(json));
  held_network net;
  auto network = net.hooks();
  network.connect = redirect(server.endpoint());
  epose_receipt_client_v2 client(network);
  deliver_on_exit cleanup{net.lookup};
  // Another host's lookup holds the slot.
  auto holder = client.request_service(challenge(), signed_descriptor("held.example.org"),
      parameters(), caps(ms(10000)));
  net.wait_held();
  using urls = std::vector<std::string>;
  for (const auto &discovery : {urls{"http://8.8.4.4:8197", "http://other.example.org:8197"},
           urls{"http://other.example.org:8197", "http://8.8.4.4:8197"}})
  {
    SCOPED_TRACE(discovery.front());
    const auto result = client.submit(discovery_only(discovery)).wait().result;
    EXPECT_TRUE(same(result, receipt_attempt_stage_v2::descriptor_lookup,
        receipt_failure_reason_v2::descriptor_unavailable)) << describe(result);
  }
  EXPECT_EQ(2u, server.request_count());
  EXPECT_EQ(1u, net.resolves.load());
  // Only busy: the busy slot is reported. Nothing usable: unavailable.
  EXPECT_EQ(receipt_failure_reason_v2::dns_slot_busy,
      client.submit(discovery_only({"http://other.example.org:8197"})).wait().result.reason);
  const auto none = client.submit(discovery_only({"not a url"})).wait().result;
  EXPECT_TRUE(same(none, receipt_attempt_stage_v2::descriptor_lookup,
      receipt_failure_reason_v2::descriptor_unavailable)) << describe(none);
  holder.cancel();
  EXPECT_EQ(receipt_failure_reason_v2::cancelled, holder.wait().result.reason);
}
