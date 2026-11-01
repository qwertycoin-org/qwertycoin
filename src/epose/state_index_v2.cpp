// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/state_index_v2.h"
#include "epose/hash_transcript.h"

#include <cstring>
#include <limits>

namespace
{
  using qwertycoin::epose::transcript::append_bytes;
  using qwertycoin::epose::transcript::append_u32_le;
  using qwertycoin::epose::transcript::append_u64_le;
  using qwertycoin::epose::transcript::hash_blob;
  using qwertycoin::epose::state_checkpoint_v2;

  struct byte_span
  {
    const char *data;
    size_t size;
  };

  bool read_u32(byte_span in, size_t &offset, uint32_t &value)
  {
    if (offset > in.size || in.size - offset < 4)
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
      value |= static_cast<uint32_t>(static_cast<uint8_t>(in.data[offset++])) << shift;
    return true;
  }

  bool read_u64(byte_span in, size_t &offset, uint64_t &value)
  {
    if (offset > in.size || in.size - offset < 8)
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8)
      value |= static_cast<uint64_t>(static_cast<uint8_t>(in.data[offset++])) << shift;
    return true;
  }

  template <typename T> bool read_bytes(byte_span in, size_t &offset, T &value)
  {
    if (offset > in.size || in.size - offset < sizeof(T))
      return false;
    std::memcpy(&value, in.data + offset, sizeof(T));
    offset += sizeof(T);
    return true;
  }

  bool checkpoint_valid(const state_checkpoint_v2 &checkpoint)
  {
    return checkpoint.block_hash != crypto::null_hash
        && checkpoint.state_hash != crypto::null_hash
        && checkpoint.payload_hash != crypto::null_hash
        && (checkpoint.height == 0 || checkpoint.parent_hash != crypto::null_hash);
  }

  qwertycoin::epose::state_index_status_v2 parse_snapshot_image(
      byte_span image, qwertycoin::epose::state_snapshot_v2 &parsed,
      size_t &payload_offset, uint64_t &payload_size)
  {
    using qwertycoin::epose::state_index_status_v2;
    constexpr size_t header_size = 4 + 4 + 1 + sizeof(crypto::hash)
        + 8 + 2 * sizeof(crypto::hash) + 8;
    if (image.size < header_size + sizeof(crypto::hash)
        || std::memcmp(image.data, "QESN", 4) != 0)
      return state_index_status_v2::corrupt_image;
    crypto::hash checksum{};
    std::memcpy(&checksum, image.data + image.size - sizeof(checksum), sizeof(checksum));
    if (crypto::cn_fast_hash(image.data, image.size - sizeof(checksum)) != checksum)
      return state_index_status_v2::corrupt_image;
    size_t offset = 4;
    uint32_t storage_schema = 0;
    if (!read_u32(image, offset, storage_schema) || offset == image.size)
      return state_index_status_v2::corrupt_image;
    parsed.state_schema = static_cast<uint8_t>(image.data[offset++]);
    if (storage_schema != 1 || (parsed.state_schema != 1 && parsed.state_schema != 2))
      return state_index_status_v2::wrong_schema;
    if (!read_bytes(image, offset, parsed.parameter_set_hash)
        || !read_u64(image, offset, parsed.height)
        || !read_bytes(image, offset, parsed.block_hash)
        || !read_bytes(image, offset, parsed.state_root)
        || !read_u64(image, offset, payload_size))
      return state_index_status_v2::corrupt_image;
    const size_t payload_end = image.size - sizeof(crypto::hash);
    if (parsed.parameter_set_hash == crypto::null_hash
        || parsed.block_hash == crypto::null_hash
        || parsed.state_root == crypto::null_hash
        || payload_size == 0 || offset > payload_end
        || payload_size > payload_end - offset)
      return state_index_status_v2::corrupt_image;
    if (payload_size != payload_end - offset)
      return state_index_status_v2::trailing_bytes;
    payload_offset = offset;
    return state_index_status_v2::accepted;
  }
}

namespace qwertycoin
{
namespace epose
{
  std::string state_snapshot_v2::serialize() const
  {
    if ((state_schema != 1 && state_schema != 2)
        || parameter_set_hash == crypto::null_hash
        || block_hash == crypto::null_hash
        || state_root == crypto::null_hash
        || state_image.empty())
      return {};
    std::string image("QESN");
    append_u32_le(image, 1);
    image.push_back(static_cast<char>(state_schema));
    append_bytes(image, parameter_set_hash);
    append_u64_le(image, height);
    append_bytes(image, block_hash);
    append_bytes(image, state_root);
    append_u64_le(image, state_image.size());
    image.append(state_image);
    append_bytes(image, hash_blob(image));
    return image;
  }

  state_index_status_v2 state_snapshot_v2::restore(
      const std::string &image,
      const crypto::hash &expected_parameter_set_hash)
  {
    state_snapshot_v2 parsed{};
    size_t payload_offset = 0;
    uint64_t payload_size = 0;
    const state_index_status_v2 status = parse_snapshot_image(
        {image.data(), image.size()}, parsed, payload_offset, payload_size);
    if (status != state_index_status_v2::accepted)
      return status;
    if (parsed.parameter_set_hash != expected_parameter_set_hash)
      return state_index_status_v2::wrong_parameter_set;
    parsed.state_image.assign(image.data() + payload_offset, static_cast<size_t>(payload_size));
    *this = std::move(parsed);
    return state_index_status_v2::accepted;
  }

  bool snapshot_matches_image(const char *data, size_t size,
      uint8_t schema, const crypto::hash &parameters, uint64_t height,
      const crypto::hash &block_hash, const crypto::hash &state_root)
  {
    state_snapshot_v2 parsed{};
    size_t payload_offset = 0;
    uint64_t payload_size = 0;
    return parse_snapshot_image({data, size}, parsed, payload_offset, payload_size)
            == state_index_status_v2::accepted
        && parsed.parameter_set_hash == parameters
        && parsed.matches(schema, height, block_hash, state_root);
  }

  bool state_snapshot_v2::matches(uint8_t expected_schema,
      uint64_t expected_height, const crypto::hash &expected_block_hash,
      const crypto::hash &expected_state_root) const
  {
    return state_schema == expected_schema && height == expected_height
        && block_hash == expected_block_hash && state_root == expected_state_root;
  }

  state_index_v2::state_index_v2(const crypto::hash &parameter_set_hash, size_t undo_horizon)
    : parameter_set_hash_(parameter_set_hash), undo_horizon_(undo_horizon)
  {
  }

  bool state_index_v2::valid() const
  {
    return parameter_set_hash_ != crypto::null_hash
        && undo_horizon_ > 0
        && undo_horizon_ < std::numeric_limits<size_t>::max();
  }

  state_index_status_v2 state_index_v2::connect(const state_checkpoint_v2 &checkpoint)
  {
    if (!valid())
      return state_index_status_v2::invalid_configuration;
    if (!checkpoint_valid(checkpoint))
      return state_index_status_v2::invalid_checkpoint;
    if (!checkpoints_.empty())
    {
      const state_checkpoint_v2 &current = checkpoints_.back();
      if (checkpoint.height == current.height)
      {
        if (checkpoint.block_hash == current.block_hash
            && checkpoint.parent_hash == current.parent_hash
            && checkpoint.state_hash == current.state_hash
            && checkpoint.payload_hash == current.payload_hash)
          return state_index_status_v2::accepted;
        return state_index_status_v2::duplicate_conflict;
      }
      if (current.height == std::numeric_limits<uint64_t>::max() || checkpoint.height != current.height + 1)
        return state_index_status_v2::height_gap;
      if (checkpoint.parent_hash != current.block_hash)
        return state_index_status_v2::parent_mismatch;
    }
    else if (checkpoint.height != 0)
      return state_index_status_v2::height_gap;

    checkpoints_.push_back(checkpoint);
    if (checkpoints_.size() > undo_horizon_ + 1)
      checkpoints_.erase(checkpoints_.begin());
    return state_index_status_v2::accepted;
  }

  state_index_status_v2 state_index_v2::disconnect(const crypto::hash &expected_tip_hash)
  {
    if (checkpoints_.empty() || checkpoints_.back().block_hash != expected_tip_hash)
      return state_index_status_v2::tip_mismatch;
    if (checkpoints_.size() <= 1)
      return state_index_status_v2::rebuild_required;
    checkpoints_.pop_back();
    return state_index_status_v2::accepted;
  }

  state_index_status_v2 state_index_v2::rebuild(const std::vector<state_checkpoint_v2> &canonical_history)
  {
    if (!valid())
      return state_index_status_v2::invalid_configuration;
    state_index_v2 rebuilt(parameter_set_hash_, undo_horizon_);
    for (const state_checkpoint_v2 &checkpoint : canonical_history)
    {
      const state_index_status_v2 status = rebuilt.connect(checkpoint);
      if (status != state_index_status_v2::accepted)
        return status;
    }
    checkpoints_.swap(rebuilt.checkpoints_);
    return state_index_status_v2::accepted;
  }

  const state_checkpoint_v2 *state_index_v2::tip() const
  {
    return checkpoints_.empty() ? nullptr : &checkpoints_.back();
  }

  size_t state_index_v2::retained_checkpoints() const
  {
    return checkpoints_.size();
  }

  crypto::hash state_index_v2::index_root() const
  {
    std::string blob("QWC_EPOSE_INDEX_ROOT_V2");
    append_bytes(blob, parameter_set_hash_);
    append_u64_le(blob, undo_horizon_);
    append_u64_le(blob, checkpoints_.size());
    for (const auto &checkpoint : checkpoints_)
    {
      append_u64_le(blob, checkpoint.height);
      append_bytes(blob, checkpoint.block_hash);
      append_bytes(blob, checkpoint.parent_hash);
      append_bytes(blob, checkpoint.state_hash);
      append_bytes(blob, checkpoint.payload_hash);
    }
    return hash_blob(blob);
  }

  std::string state_index_v2::serialize() const
  {
    std::string image("QEI2");
    append_u32_le(image, EPOSE_STATE_INDEX_SCHEMA_V2);
    append_bytes(image, parameter_set_hash_);
    append_u64_le(image, undo_horizon_);
    append_u64_le(image, checkpoints_.size());
    for (const auto &checkpoint : checkpoints_)
    {
      append_u64_le(image, checkpoint.height);
      append_bytes(image, checkpoint.block_hash);
      append_bytes(image, checkpoint.parent_hash);
      append_bytes(image, checkpoint.state_hash);
      append_bytes(image, checkpoint.payload_hash);
    }
    append_bytes(image, hash_blob(image));
    return image;
  }

  state_index_status_v2 state_index_v2::restore(const std::string &image)
  {
    if (!valid())
      return state_index_status_v2::invalid_configuration;
    constexpr size_t fixed_header = 4 + 4 + sizeof(crypto::hash) + 8 + 8;
    if (image.size() < fixed_header + sizeof(crypto::hash) || image.compare(0, 4, "QEI2") != 0)
      return state_index_status_v2::corrupt_image;
    crypto::hash expected_checksum{};
    std::memcpy(&expected_checksum, image.data() + image.size() - sizeof(expected_checksum), sizeof(expected_checksum));
    if (hash_blob(image.substr(0, image.size() - sizeof(expected_checksum))) != expected_checksum)
      return state_index_status_v2::corrupt_image;

    size_t offset = 4;
    const byte_span bytes{image.data(), image.size()};
    uint32_t schema = 0;
    crypto::hash parameters{};
    uint64_t serialized_horizon = 0;
    uint64_t count = 0;
    if (!read_u32(bytes, offset, schema) || !read_bytes(bytes, offset, parameters)
        || !read_u64(bytes, offset, serialized_horizon) || !read_u64(bytes, offset, count))
      return state_index_status_v2::corrupt_image;
    if (schema != EPOSE_STATE_INDEX_SCHEMA_V2)
      return state_index_status_v2::wrong_schema;
    if (parameters != parameter_set_hash_ || serialized_horizon != undo_horizon_)
      return state_index_status_v2::wrong_parameter_set;
    constexpr size_t checkpoint_size = 8 + 4 * sizeof(crypto::hash);
    const size_t payload_end = image.size() - sizeof(crypto::hash);
    if (count > undo_horizon_ + 1 || count > (payload_end - offset) / checkpoint_size)
      return state_index_status_v2::corrupt_image;

    std::vector<state_checkpoint_v2> parsed;
    parsed.reserve(static_cast<size_t>(count));
    for (uint64_t i = 0; i < count; ++i)
    {
      state_checkpoint_v2 checkpoint{};
      if (!read_u64(bytes, offset, checkpoint.height)
          || !read_bytes(bytes, offset, checkpoint.block_hash)
          || !read_bytes(bytes, offset, checkpoint.parent_hash)
          || !read_bytes(bytes, offset, checkpoint.state_hash)
          || !read_bytes(bytes, offset, checkpoint.payload_hash))
        return state_index_status_v2::corrupt_image;
      parsed.push_back(checkpoint);
    }
    if (offset != payload_end)
      return state_index_status_v2::trailing_bytes;

    state_index_v2 validated(parameter_set_hash_, undo_horizon_);
    if (!parsed.empty())
    {
      validated.checkpoints_.push_back(parsed.front());
      if (!checkpoint_valid(parsed.front()))
        return state_index_status_v2::invalid_checkpoint;
      for (size_t i = 1; i < parsed.size(); ++i)
      {
        if (!checkpoint_valid(parsed[i]))
          return state_index_status_v2::invalid_checkpoint;
        const state_checkpoint_v2 &previous = parsed[i - 1];
        if (parsed[i].height == previous.height)
          return state_index_status_v2::duplicate_conflict;
        if (previous.height == std::numeric_limits<uint64_t>::max()
            || parsed[i].height != previous.height + 1)
          return state_index_status_v2::height_gap;
        if (parsed[i].parent_hash != previous.block_hash)
          return state_index_status_v2::parent_mismatch;
        validated.checkpoints_.push_back(parsed[i]);
      }
    }
    if (validated.serialize() != image)
      return state_index_status_v2::corrupt_image;
    checkpoints_.swap(validated.checkpoints_);
    return state_index_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
