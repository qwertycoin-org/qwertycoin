// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>

#include "crypto/crypto.h"
#include "epose/diagnostics_v2.h"
#include "epose/coordinator_v2.h"
#include "epose/resource_policy_v2.h"
#include "epose/service_epoch.h"

namespace cryptonote
{
  // Receipt jobs in flight at once: committee_size (9 on mainnet), the
  // subjects a verifier expects per round, so a round runs in one wave.
  constexpr size_t EPOSE_V2_RECEIPT_PARALLEL_JOBS =
      qwertycoin::epose::EPOSE_VERIFIER_COMMITTEE_SIZE;

  // Each stage deadline is min(stage start + cap, round deadline). DNS and
  // every dial attempt share the connect deadline. recv has three bounds,
  // all clipped to the round deadline:
  // - inactivity: recv_idle without a byte is recv_timeout. It also bounds the
  //   time to the first byte.
  // - progress floor: from the first byte on, every span of recv_idle + window
  //   must deliver min_rate x window bytes, else recv_too_slow. The span holds
  //   recv_idle so a stall the inactivity bound tolerates never fails it.
  // - total: recv start + recv_idle + ceil(B / min_rate), B = header + body
  //   maximum, or header + Content-Length once known, else recv_too_slow.
  // Worst-case lease hold per attempt: a trickler below min_rate x window /
  // (window + recv_idle) = 2.67 KiB/s: 2 s connect + 1 s send + 4 s to the
  // first byte + 6 s span = 13 s; a faster peer up to exactly the floor:
  // 2 + 1 + total bound = 521.5 s (service), 9.5 s (descriptor).
  constexpr uint64_t EPOSE_V2_CONNECT_TIMEOUT_MS = 2000;
  constexpr uint64_t EPOSE_V2_SEND_TIMEOUT_MS = 1000;
  constexpr uint64_t EPOSE_V2_RECV_IDLE_TIMEOUT_MS = 4000;
  // The largest exchange is EPOSE_V2_MAX_SERVICE_RECV_BYTES = 4,214,784
  // bytes: 514.5 s at 8 KiB/s, 518.5 s with the idle slack. With a descriptor
  // lookup and both connects and sends that is under 2.5% of the shortest
  // mainnet round (199 x 120 s). Four concurrent fetches from one subject
  // need 262.1 kbit/s of its uplink.
  constexpr uint64_t EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S = 8 * 1024;
  constexpr uint64_t EPOSE_V2_RECV_RATE_WINDOW_MS = 2000;
  // Largest response the client reads for one exchange, headers included.
  constexpr uint64_t EPOSE_V2_MAX_SERVICE_RECV_BYTES =
      2 * (2 * 1024 * 1024) + 4096 + 16 * 1024;
  constexpr uint64_t EPOSE_V2_MAX_DESCRIPTOR_RECV_BYTES = 4096 + 16 * 1024;

  // slack_ms + ceil(max_bytes * 1000 / min_rate), saturating; min_rate 0
  // gives uint64 max.
  uint64_t epose_receipt_recv_total_ms_v2(
      uint64_t max_bytes, uint64_t slack_ms, uint64_t min_rate);

  // Happy Eyeballs (RFC 8305 section 5): the next attempt starts when one
  // fails, else this long after the latest start; none starts at or after the
  // connect deadline. The first connect wins and the others are closed.
  constexpr uint64_t EPOSE_V2_DIAL_ATTEMPT_DELAY_MS = 250;
  // Each job owns one socket (its lease); every further live attempt of a dial
  // borrows a unit from this client-wide FIFO pool, so at most N + N sockets
  // exist. Admission is unchanged.
  constexpr size_t EPOSE_V2_DIAL_EXTRA_ATTEMPTS = EPOSE_V2_RECEIPT_PARALLEL_JOBS;
  // Pool units one dial may hold, so a peer with many black-holed addresses
  // keeps at most 3 attempts live and cannot starve the other dials.
  constexpr size_t EPOSE_V2_DIAL_EXTRA_ATTEMPTS_PER_DIAL = 2;
  // Hosts whose last winning family is remembered (only when both families
  // were offered) and dialed first; least recently recorded evicted.
  // Without an entry IPv4 goes first, deliberately against RFC 8305 section 4
  // and RFC 6724: a node with announced but unrouted IPv6 then loses no
  // attempt delay, and interleaving still reaches IPv6 on the second attempt.
  constexpr size_t EPOSE_V2_DIAL_PREFERENCE_ENTRIES = 4096;

  // Remembered hostname resolutions, used while the resolver slot is busy.
  // An entry serves only requests of the service epoch that resolved it. The
  // first resolution of a newer epoch evicts all earlier ones; an answer for
  // an older epoch is dropped. Memory only.
  constexpr size_t EPOSE_V2_RESOLVED_HOST_ENTRIES = 4096;

  // (d - h + 1) * T ms at height h for a window ending at d, T =
  // DIFFICULTY_TARGET_V2 * 1000; 0 once h > d.
  uint64_t epose_receipt_round_budget_ms_v2(
      uint64_t height, uint64_t deadline_height);

  // Start jitter for a round over blocks [first, last], B = last - first + 1:
  //   window_ms = min(T, B * T / 100)
  //   offset_ms = LE64(cn_fast_hash("QWC_EPOSE_RECEIPT_JITTER_V2" || local
  //               || peer || LE64(epoch) || LE64(round))[0..8]) mod window_ms
  // or 0 when window_ms is 0, counted from when this node first saw the round.
  uint64_t epose_receipt_jitter_window_ms_v2(
      uint64_t first_height, uint64_t last_height);
  uint64_t epose_receipt_start_offset_ms_v2(
      const crypto::public_key &local, const crypto::public_key &peer,
      uint64_t epoch, uint64_t round, uint64_t window_ms);

  struct epose_stage_deadlines_v2
  {
    std::chrono::steady_clock::time_point round_deadline =
        std::chrono::steady_clock::time_point::max();
    std::chrono::milliseconds connect{EPOSE_V2_CONNECT_TIMEOUT_MS};
    // Happy Eyeballs connection attempt delay; see
    // EPOSE_V2_DIAL_ATTEMPT_DELAY_MS.
    std::chrono::milliseconds connect_attempt_delay{EPOSE_V2_DIAL_ATTEMPT_DELAY_MS};
    std::chrono::milliseconds send{EPOSE_V2_SEND_TIMEOUT_MS};
    std::chrono::milliseconds recv_idle{EPOSE_V2_RECV_IDLE_TIMEOUT_MS};
    // Floor in bytes per second. A rate of 0 disables the floor and the
    // total bound; a window of 0 only the floor (tests only).
    uint64_t recv_min_rate = EPOSE_V2_RECV_MIN_RATE_BYTES_PER_S;
    std::chrono::milliseconds recv_rate_window{EPOSE_V2_RECV_RATE_WINDOW_MS};

    // min(now + cap, round_deadline).
    std::chrono::steady_clock::time_point stage(std::chrono::milliseconds cap) const;
  };

  // Pool of EPOSE_V2_DIAL_EXTRA_ATTEMPTS units. Worker thread only.
  class epose_dial_budget_v2
  {
  public:
    explicit epose_dial_budget_v2(size_t limit);
    size_t in_use() const;
    bool try_acquire();
    // With a waiter queued the unit passes to the oldest one and its wake
    // runs inside release(), so wake must post its work.
    void release();
    // wake receives the returned id.
    uint64_t wait(std::function<void(uint64_t)> wake);
    void leave(uint64_t id);

  private:
    const size_t limit_;
    size_t in_use_ = 0;
    uint64_t next_waiter_ = 0;
    std::vector<std::pair<uint64_t, std::function<void(uint64_t)>>> waiters_;
  };

  // What one dial observed. Worker thread only; never written after the dial
  // completed or was cancelled.
  struct epose_dial_report_v2
  {
    // Most informative failure so far: refused, unreachable, other, timed_out.
    boost::system::error_code failure;
    bool prohibited = false;
    // The deadline passed with an attempt pending or a candidate undialed.
    bool expired = false;
    bool connected = false;
    boost::asio::ip::address winner;
  };

  // Network hooks, replaced in tests. Every hook runs on the client's
  // worker thread and completes on its io_context.
  struct epose_network_v2
  {
    using resolve_handler = std::function<void(boost::system::error_code,
        std::vector<boost::asio::ip::address>)>;
    using connect_handler = std::function<void(boost::system::error_code,
        boost::asio::ip::tcp::socket)>;
    using cancel_operation = std::function<void()>;
    using cancel_resolve = cancel_operation;
    using connect_function = std::function<cancel_operation(
        const boost::asio::ip::tcp::endpoint &, boost::asio::io_context &,
        std::chrono::steady_clock::time_point, connect_handler)>;
    struct dial_request
    {
      // Checked and in dial order (epose_happy_eyeballs_order_v2).
      std::vector<boost::asio::ip::address> addresses;
      uint16_t port = 0;
      std::chrono::steady_clock::time_point deadline{};
      std::chrono::milliseconds attempt_delay{EPOSE_V2_DIAL_ATTEMPT_DELAY_MS};
      connect_function connect; // empty: connect_epose_address
      // Empty: one attempt at a time.
      std::shared_ptr<epose_dial_budget_v2> budget;
      size_t extra_limit = EPOSE_V2_DIAL_EXTRA_ATTEMPTS_PER_DIAL;
      std::shared_ptr<epose_dial_report_v2> report;               // required
      std::shared_ptr<qwertycoin::epose::receipt_dial_counters_v2> counters; // required
    };
    // Completes at most once on io, when one attempt connects or all failed;
    // cancellation suppresses the completion and closes every attempt.
    using dialer = std::function<cancel_operation(
        dial_request, boost::asio::io_context &, connect_handler)>;

    // Returns promptly, as does its cancel; completes at most once on io, even
    // after cancel, possibly synchronously. A pending lookup holds the resolver
    // slot until it completes and a late success still refreshes the
    // remembered resolution. Throwing leaves nothing outstanding.
    std::function<cancel_resolve(const std::string &, boost::asio::io_context &,
        resolve_handler)> resolve;
    // One TCP connect, completed at most once on io (synchronously allowed);
    // throwing boost::system::system_error is a failed connect with that code.
    connect_function connect;
    dialer dial;
  };

  bool public_epose_dial_address(const boost::asio::ip::address &address);
  // Default connect hook: closes the socket at deadline, reports timed_out.
  epose_network_v2::cancel_operation connect_epose_address(
      const boost::asio::ip::tcp::endpoint &endpoint, boost::asio::io_context &io,
      std::chrono::steady_clock::time_point deadline,
      epose_network_v2::connect_handler complete);
  // RFC 8305 section 4 with a First Address Family Count of 1: families
  // alternate, the preferred one first; resolver order within a family.
  std::vector<boost::asio::ip::address> epose_happy_eyeballs_order_v2(
      const std::vector<boost::asio::ip::address> &addresses, bool prefer_v6);
  // Default dialer. A candidate public_epose_dial_address rejects is skipped
  // and counted; with nothing dialable it fails access_denied and sets
  // report.prohibited.
  epose_network_v2::cancel_operation happy_eyeballs_epose_dial_v2(
      epose_network_v2::dial_request request, boost::asio::io_context &io,
      epose_network_v2::connect_handler complete);
  bool parse_epose_discovery_url(
      const std::string &url, std::string &host, uint16_t &port);

  struct epose_receipt_request_v2
  {
    qwertycoin::epose::consensus_parameters_v2 parameters{};
    qwertycoin::epose::service_challenge_v2 challenge{};
    // A known endpoint skips discovery. Otherwise each discovery URL is asked
    // for challenge.endpoint_descriptor_hash in order until one validates.
    bool endpoint_known = false;
    qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
    std::vector<std::string> discovery;
    // Production: the endpoint must belong to the challenge's subject and a
    // known endpoint must match its descriptor hash.
    bool bind_to_challenge = true;
    // Stop after discovery and return the validated descriptor.
    bool descriptor_only = false;
    epose_stage_deadlines_v2 deadlines{};
  };

  struct epose_receipt_exchange_v2
  {
    qwertycoin::epose::receipt_step_result_v2 result{};
    qwertycoin::epose::endpoint_descriptor_v2 endpoint{};
    qwertycoin::epose::canonical_service_response_v2 response{};
    // Connect attempts of every exchange of this job, per address family.
    qwertycoin::epose::receipt_dial_counters_v2 dial{};
  };

  namespace detail
  {
    struct epose_receipt_job_state;
    struct epose_receipt_client_state;
  }

  // Thread-safe handle on one submitted receipt job.
  class epose_receipt_job_v2
  {
  public:
    explicit epose_receipt_job_v2(std::shared_ptr<detail::epose_receipt_job_state> state);
    bool ready() const;
    // Blocks until the job finishes. Never call it from a network hook.
    epose_receipt_exchange_v2 wait() const;
    // Asynchronous; the job ends as cancelled unless it already finished.
    void cancel() const;

  private:
    std::shared_ptr<detail::epose_receipt_job_state> state_;
  };

  // One worker thread runs up to capacity jobs, each a chain of HTTP/1.1
  // JSON POSTs (discovery, then the challenge) with per-stage deadlines.
  // A connect races the endpoint's addresses (EPOSE_V2_DIAL_ATTEMPT_DELAY_MS)
  // and, if all fail, reports connection_refused, else connect_unreachable,
  // else transport_failure, else connect_timeout; deadline_expired instead if
  // the round deadline cut the dial short. Addresses the dial policy removes
  // are counted per family (remembered resolutions are stored filtered).
  // Hostnames share one resolver slot, held until its lookup completes even
  // if cancelled; nothing queues for it. A busy slot means: dial the
  // remembered resolution of the same epoch, else attach to a live lookup of
  // the same host, else fail at once with dns_slot_busy. A lookup is
  // cancelled once no job waits on it, so a stuck lookup fails only its own
  // host's jobs. A stage clipped to the round deadline is deadline_expired.
  // Responses need Content-Length or EOF; Transfer-Encoding is malformed.
  class epose_receipt_client_v2
  {
  public:
    explicit epose_receipt_client_v2(epose_network_v2 network = {},
        size_t capacity = EPOSE_V2_RECEIPT_PARALLEL_JOBS,
        size_t extra_attempts = EPOSE_V2_DIAL_EXTRA_ATTEMPTS,
        size_t preference_entries = EPOSE_V2_DIAL_PREFERENCE_ENTRIES);
    // Cancels every job and joins the worker. The daemon uses shared(), whose
    // in-flight jobs only the receipt scheduler's stop() cancels and joins.
    ~epose_receipt_client_v2();
    epose_receipt_client_v2(const epose_receipt_client_v2 &) = delete;
    epose_receipt_client_v2 &operator=(const epose_receipt_client_v2 &) = delete;

    // Deliberately process-lifetime: Asio's resolver joins its worker during
    // io_context destruction and libc getaddrinfo is not interruptible.
    static epose_receipt_client_v2 &shared();

    size_t capacity() const;
    size_t in_flight() const;
    // Never blocks; beyond capacity the job is already finished as
    // local_lease / lease_exhausted.
    epose_receipt_job_v2 submit(epose_receipt_request_v2 request);
    epose_receipt_job_v2 fetch_descriptor(const std::string &url,
        const qwertycoin::epose::consensus_parameters_v2 &parameters,
        const crypto::hash &required_hash,
        const epose_stage_deadlines_v2 &deadlines = {});
    epose_receipt_job_v2 request_service(
        const qwertycoin::epose::service_challenge_v2 &challenge,
        const qwertycoin::epose::endpoint_descriptor_v2 &endpoint,
        const qwertycoin::epose::consensus_parameters_v2 &parameters,
        const epose_stage_deadlines_v2 &deadlines = {});

  private:
    std::shared_ptr<detail::epose_receipt_client_state> state_;
    std::thread worker_;
  };
}
