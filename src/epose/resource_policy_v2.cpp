// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/resource_policy_v2.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include <boost/asio/ip/address.hpp>

namespace
{
  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
  }

  void append_u8(std::string &out, uint8_t value) { out.push_back(static_cast<char>(value)); }
  void append_u16(std::string &out, uint16_t value)
  {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
  }
  void append_u64(std::string &out, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }
  void append_network(std::string &out, cryptonote::network_type nettype)
  {
    for (const auto byte : cryptonote::get_config(nettype).NETWORK_ID)
      append_u8(out, byte);
  }
  crypto::hash fast_hash(const std::string &blob) { return crypto::cn_fast_hash(blob.data(), blob.size()); }

  bool canonical_dns(const std::string &host)
  {
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.'
        || host.find('.') == std::string::npos)
      return false;
    size_t label = 0;
    unsigned char previous = 0;
    for (const unsigned char byte : host)
    {
      if (byte == '.')
      {
        if (label == 0 || label > 63 || previous == '-')
          return false;
        label = 0;
      }
      else
      {
        const bool ascii_lower = byte >= 'a' && byte <= 'z';
        const bool ascii_digit = byte >= '0' && byte <= '9';
        if (!(ascii_lower || ascii_digit || byte == '-'))
          return false;
        if ((label == 0 && byte == '-') || label >= 63)
          return false;
        ++label;
      }
      previous = byte;
    }
    return label > 0 && label <= 63 && previous != '-';
  }

  bool prohibited_v4(const boost::asio::ip::address_v4 &address)
  {
    const auto b = address.to_bytes();
    return address.is_unspecified() || address.is_loopback() || address.is_multicast()
        || b[0] == 0 || b[0] == 10 || b[0] == 127
        || (b[0] == 100 && b[1] >= 64 && b[1] <= 127)
        || (b[0] == 169 && b[1] == 254)
        || (b[0] == 172 && b[1] >= 16 && b[1] <= 31)
        || (b[0] == 192 && b[1] == 0 && b[2] == 0)
        || (b[0] == 192 && b[1] == 88 && b[2] == 99)
        || (b[0] == 192 && b[1] == 168)
        || (b[0] == 198 && (b[1] == 18 || b[1] == 19))
        || (b[0] == 198 && b[1] == 51 && b[2] == 100)
        || (b[0] == 203 && b[1] == 0 && b[2] == 113)
        || b[0] >= 224;
  }

  bool prohibited_v6(const boost::asio::ip::address_v6 &address)
  {
    if (address.is_v4_mapped())
    {
      const auto b = address.to_bytes();
      boost::asio::ip::address_v4::bytes_type v4{{b[12], b[13], b[14], b[15]}};
      return prohibited_v4(boost::asio::ip::address_v4(v4));
    }
    const auto b = address.to_bytes();
    const bool discard_only = b[0] == 0x01 && b[1] == 0x00
        && std::all_of(b.begin() + 2, b.begin() + 8, [](uint8_t byte) { return byte == 0; });
    const bool benchmarking = b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x00
        && b[3] == 0x02 && b[4] == 0x00 && b[5] == 0x00;
    return address.is_unspecified() || address.is_loopback() || address.is_multicast()
        || address.is_link_local() || address.is_site_local()
        || (b[0] & 0xfe) == 0xfc
        || (b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x0d && b[3] == 0xb8)
        || discard_only || benchmarking;
  }
}

namespace qwertycoin
{
namespace epose
{
  crypto::hash hash_endpoint_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const endpoint_descriptor_v2 &descriptor)
  {
    std::string blob("QWC_EPOSE_ENDPOINT_V2");
    append_network(blob, nettype);
    append_bytes(blob, genesis_hash);
    append_bytes(blob, parameter_set_hash);
    append_u8(blob, descriptor.version);
    append_bytes(blob, descriptor.service_public_key);
    append_u8(blob, static_cast<uint8_t>(descriptor.transport));
    append_u64(blob, descriptor.host.size());
    blob.append(descriptor.host);
    append_u16(blob, descriptor.port);
    append_u8(blob, descriptor.service_kind);
    append_u8(blob, descriptor.service_version);
    append_u64(blob, descriptor.sequence);
    append_u64(blob, descriptor.expiry_epoch);
    return fast_hash(blob);
  }

  bool sign_endpoint_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      endpoint_descriptor_v2 &descriptor,
      const crypto::secret_key &service_secret_key)
  {
    crypto::public_key public_key{};
    if (!crypto::secret_key_to_public_key(service_secret_key, public_key)
        || public_key != descriptor.service_public_key)
      return false;
    crypto::generate_signature(hash_endpoint_descriptor_v2(nettype, genesis_hash, parameter_set_hash, descriptor),
        descriptor.service_public_key, service_secret_key, descriptor.signature);
    return true;
  }

  resource_status_v2 validate_endpoint_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const endpoint_descriptor_v2 &descriptor)
  {
    if (nettype == cryptonote::UNDEFINED || genesis_hash == crypto::null_hash
        || parameter_set_hash == crypto::null_hash || descriptor.version != 1
        || descriptor.service_public_key == crypto::null_pkey
        || !crypto::check_key(descriptor.service_public_key) || descriptor.port == 0
        || descriptor.service_kind == 0 || descriptor.service_version == 0
        || (descriptor.transport != endpoint_transport_v2::tcp_ipv4
            && descriptor.transport != endpoint_transport_v2::tcp_ipv6
            && descriptor.transport != endpoint_transport_v2::dns))
      return resource_status_v2::invalid_descriptor;

    boost::system::error_code error;
    const auto parsed = boost::asio::ip::make_address(descriptor.host, error);
    if (descriptor.transport == endpoint_transport_v2::dns)
    {
      if (!error || !canonical_dns(descriptor.host))
        return resource_status_v2::noncanonical_host;
    }
    else
    {
      if (error || parsed.to_string() != descriptor.host
          || (descriptor.transport == endpoint_transport_v2::tcp_ipv4 && !parsed.is_v4())
          || (descriptor.transport == endpoint_transport_v2::tcp_ipv6 && !parsed.is_v6()))
        return resource_status_v2::noncanonical_host;
      if (!public_probe_address_v2(descriptor.host))
        return resource_status_v2::prohibited_address;
    }
    if (!crypto::check_signature(hash_endpoint_descriptor_v2(nettype, genesis_hash, parameter_set_hash, descriptor),
            descriptor.service_public_key, descriptor.signature))
      return resource_status_v2::invalid_signature;
    return resource_status_v2::accepted;
  }

  bool public_probe_address_v2(const std::string &text)
  {
    boost::system::error_code error;
    const auto address = boost::asio::ip::make_address(text, error);
    if (error)
      return false;
    return address.is_v4() ? !prohibited_v4(address.to_v4()) : !prohibited_v6(address.to_v6());
  }

  resource_status_v2 validate_resolved_targets_v2(const std::vector<std::string> &addresses, size_t max_addresses)
  {
    if (max_addresses == 0)
      return resource_status_v2::invalid_configuration;
    if (addresses.empty())
      return resource_status_v2::resolution_empty;
    if (addresses.size() > max_addresses)
      return resource_status_v2::resolution_limit_exceeded;
    for (const auto &address : addresses)
      if (!public_probe_address_v2(address))
        return resource_status_v2::prohibited_address;
    return resource_status_v2::accepted;
  }

  bool probe_limits_v2::valid() const
  {
    return max_concurrent > 0 && max_pending_peers > 0 && max_per_peer > 0
        && max_per_peer <= max_concurrent && max_request_bytes > 0
        && max_response_bytes > 0 && max_timeout_ms > 0;
  }

  probe_budget_v2::probe_budget_v2(const probe_limits_v2 &limits) : limits_(limits) {}

  resource_status_v2 probe_budget_v2::acquire(
      const crypto::hash &peer, size_t request_bytes, size_t response_limit, uint64_t timeout_ms)
  {
    if (!limits_.valid() || peer == crypto::null_hash)
      return resource_status_v2::invalid_configuration;
    if (request_bytes > limits_.max_request_bytes)
      return resource_status_v2::request_too_large;
    if (response_limit > limits_.max_response_bytes)
      return resource_status_v2::response_too_large;
    if (timeout_ms > limits_.max_timeout_ms || timeout_ms == 0)
      return resource_status_v2::timeout_too_large;
    if (active_ >= limits_.max_concurrent)
      return resource_status_v2::concurrency_exceeded;
    auto found = std::find_if(peers_.begin(), peers_.end(), [&](const peer_count &entry) { return entry.peer == peer; });
    if (found == peers_.end())
    {
      if (peers_.size() >= limits_.max_pending_peers)
        return resource_status_v2::peer_limit_exceeded;
      peers_.push_back({peer, 1});
    }
    else
    {
      if (found->count >= limits_.max_per_peer)
        return resource_status_v2::peer_limit_exceeded;
      ++found->count;
    }
    ++active_;
    return resource_status_v2::accepted;
  }

  void probe_budget_v2::release(const crypto::hash &peer)
  {
    auto found = std::find_if(peers_.begin(), peers_.end(), [&](const peer_count &entry) { return entry.peer == peer; });
    if (found == peers_.end() || found->count == 0)
      return;
    --found->count;
    --active_;
    if (found->count == 0)
      peers_.erase(found);
  }

  size_t probe_budget_v2::active() const { return active_; }

  admission_context_cache_v2::admission_context_cache_v2(size_t max_contexts) : max_contexts_(max_contexts) {}

  resource_status_v2 admission_context_cache_v2::admit(
      const crypto::hash &context, const std::vector<crypto::hash> &currently_allowed)
  {
    if (max_contexts_ == 0 || context == crypto::null_hash)
      return resource_status_v2::invalid_configuration;
    if (std::find(currently_allowed.begin(), currently_allowed.end(), context) == currently_allowed.end())
      return resource_status_v2::unknown_admission_context;
    contexts_.erase(std::remove_if(contexts_.begin(), contexts_.end(), [&](const crypto::hash &cached) {
      return std::find(currently_allowed.begin(), currently_allowed.end(), cached) == currently_allowed.end();
    }), contexts_.end());
    if (std::find(contexts_.begin(), contexts_.end(), context) != contexts_.end())
      return resource_status_v2::accepted;
    if (contexts_.size() >= max_contexts_)
      return resource_status_v2::context_cache_full;
    contexts_.push_back(context);
    return resource_status_v2::accepted;
  }

  size_t admission_context_cache_v2::size() const { return contexts_.size(); }

  bool relay_queue_limits_v2::valid() const
  {
    return max_items > 0 && max_bytes > 0
        && reserved_enrollment_items > 0 && reserved_evidence_items > 0
        && reserved_enrollment_items <= max_items
        && reserved_evidence_items <= max_items - reserved_enrollment_items
        && reserved_enrollment_bytes > 0 && reserved_evidence_bytes > 0
        && reserved_enrollment_bytes <= max_bytes
        && reserved_evidence_bytes <= max_bytes - reserved_enrollment_bytes;
  }

  bool relay_template_limits_v2::valid() const
  {
    return max_items > 0 && max_bytes > 0
        && reserved_enrollment_items > 0 && reserved_evidence_items > 0
        && reserved_enrollment_items <= max_items
        && reserved_evidence_items <= max_items - reserved_enrollment_items
        && reserved_enrollment_bytes > 0 && reserved_evidence_bytes > 0
        && reserved_enrollment_bytes <= max_bytes
        && reserved_evidence_bytes <= max_bytes - reserved_enrollment_bytes;
  }

  deadline_relay_queue_v2::deadline_relay_queue_v2(const relay_queue_limits_v2 &limits)
    : limits_(limits)
  {
  }

  resource_status_v2 deadline_relay_queue_v2::enqueue(
      const relay_item_v2 &item, uint64_t current_height)
  {
    if (!limits_.valid() || item.id == crypto::null_hash || item.bytes == 0
        || (item.record_class != relay_class_v2::enrollment
            && item.record_class != relay_class_v2::evidence))
      return resource_status_v2::invalid_configuration;
    prune_expired(current_height);
    if (item.deadline_height < current_height)
      return resource_status_v2::relay_item_expired;
    const auto duplicate = std::find_if(items_.begin(), items_.end(), [&](const relay_item_v2 &stored) {
      return stored.id == item.id;
    });
    if (duplicate != items_.end())
      return duplicate->record_class == item.record_class && duplicate->bytes == item.bytes
              && duplicate->deadline_height == item.deadline_height
          ? resource_status_v2::idempotent_duplicate : resource_status_v2::relay_item_conflict;

    size_t class_items = 0;
    size_t class_bytes = 0;
    for (const auto &stored : items_)
      if (stored.record_class == item.record_class)
      {
        ++class_items;
        if (class_bytes > std::numeric_limits<size_t>::max() - stored.bytes)
          return resource_status_v2::invalid_configuration;
        class_bytes += stored.bytes;
      }
    const bool enrollment = item.record_class == relay_class_v2::enrollment;
    const size_t other_reserved_items = enrollment
        ? limits_.reserved_evidence_items : limits_.reserved_enrollment_items;
    const size_t other_reserved_bytes = enrollment
        ? limits_.reserved_evidence_bytes : limits_.reserved_enrollment_bytes;
    if (items_.size() >= limits_.max_items
        || class_items >= limits_.max_items - other_reserved_items
        || item.bytes > limits_.max_bytes - bytes_
        || item.bytes > limits_.max_bytes - other_reserved_bytes - class_bytes)
      return resource_status_v2::relay_queue_full;
    items_.push_back(item);
    bytes_ += item.bytes;
    return resource_status_v2::accepted;
  }

  void deadline_relay_queue_v2::prune_expired(uint64_t current_height)
  {
    items_.erase(std::remove_if(items_.begin(), items_.end(), [&](const relay_item_v2 &item) {
      if (item.deadline_height >= current_height)
        return false;
      bytes_ -= item.bytes;
      return true;
    }), items_.end());
  }

  resource_status_v2 deadline_relay_queue_v2::select_for_template(
      uint64_t current_height,
      const relay_template_limits_v2 &limits,
      std::vector<relay_item_v2> &selected) const
  {
    selected.clear();
    if (!limits_.valid() || !limits.valid())
      return resource_status_v2::invalid_configuration;
    std::vector<const relay_item_v2 *> candidates;
    candidates.reserve(items_.size());
    for (const auto &item : items_)
      if (item.deadline_height >= current_height)
        candidates.push_back(&item);
    std::sort(candidates.begin(), candidates.end(), [](const relay_item_v2 *left, const relay_item_v2 *right) {
      if (left->deadline_height != right->deadline_height)
        return left->deadline_height < right->deadline_height;
      const int id_order = std::memcmp(&left->id, &right->id, sizeof(left->id));
      if (id_order != 0)
        return id_order < 0;
      return static_cast<uint8_t>(left->record_class) < static_cast<uint8_t>(right->record_class);
    });
    size_t selected_bytes = 0;
    const auto already_selected = [&](const relay_item_v2 *candidate) {
      return std::any_of(selected.begin(), selected.end(), [&](const relay_item_v2 &item) {
        return item.id == candidate->id;
      });
    };
    const auto add = [&](const relay_item_v2 *candidate) {
      if (selected.size() >= limits.max_items || candidate->bytes > limits.max_bytes - selected_bytes)
        return false;
      selected.push_back(*candidate);
      selected_bytes += candidate->bytes;
      return true;
    };
    const auto reserve_class = [&](relay_class_v2 record_class, size_t item_limit, size_t byte_limit) {
      size_t class_items = 0;
      size_t class_bytes = 0;
      for (const relay_item_v2 *candidate : candidates)
      {
        if (candidate->record_class != record_class || class_items >= item_limit
            || candidate->bytes > byte_limit - class_bytes)
          continue;
        if (add(candidate))
        {
          ++class_items;
          class_bytes += candidate->bytes;
        }
      }
    };
    reserve_class(relay_class_v2::enrollment, limits.reserved_enrollment_items,
        limits.reserved_enrollment_bytes);
    reserve_class(relay_class_v2::evidence, limits.reserved_evidence_items,
        limits.reserved_evidence_bytes);
    for (const relay_item_v2 *candidate : candidates)
      if (!already_selected(candidate))
        add(candidate);
    return resource_status_v2::accepted;
  }

  size_t deadline_relay_queue_v2::size() const { return items_.size(); }
  size_t deadline_relay_queue_v2::bytes() const { return bytes_; }
  size_t deadline_relay_queue_v2::size(relay_class_v2 record_class) const
  {
    return static_cast<size_t>(std::count_if(items_.begin(), items_.end(), [&](const relay_item_v2 &item) {
      return item.record_class == record_class;
    }));
  }

  resource_status_v2 validate_rpc_page_v2(
      uint64_t offset, uint64_t limit, uint64_t max_limit, uint64_t available_records,
      uint64_t max_scan, uint64_t &end_offset)
  {
    end_offset = 0;
    if (limit == 0 || max_limit == 0 || limit > max_limit || offset > available_records)
      return resource_status_v2::invalid_page;
    if (offset > max_scan || limit > max_scan - offset)
      return resource_status_v2::scan_limit_exceeded;
    end_offset = std::min(available_records, offset + limit);
    return resource_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
