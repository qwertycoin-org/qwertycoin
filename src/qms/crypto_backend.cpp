#include "crypto_backend.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include <sodium.h>

#include "crypto/qwc_qms_crypto.h"

namespace qwertycoin
{
namespace qms
{
namespace
{
  class owned_buffer
  {
  public:
    owned_buffer() = default;
    ~owned_buffer() { qwc_qms_crypto_buffer_free(value_); }
    owned_buffer(const owned_buffer&) = delete;
    owned_buffer& operator=(const owned_buffer&) = delete;
    qwc_qms_crypto_buffer* out() noexcept { return &value_; }
    bytes copy() const
    {
      return value_.len == 0 ? bytes{} : bytes(value_.data, value_.data + value_.len);
    }
    std::string string() const
    {
      return value_.len == 0 ? std::string{} :
        std::string(reinterpret_cast<const char*>(value_.data), value_.len);
    }
  private:
    qwc_qms_crypto_buffer value_{nullptr, 0};
  };

  void checked_call(int32_t result, const owned_buffer& error)
  {
    if (result == 0) return;
    const std::string detail = error.string();
    throw std::runtime_error(detail.empty() ? "QMS crypto backend failure" : detail);
  }

  uint32_t read_u32(const bytes& value, size_t& position)
  {
    if (value.size() - position < 4)
      throw std::runtime_error("truncated QMS crypto backend response");
    uint32_t result = 0;
    for (unsigned i = 0; i != 4; ++i)
      result |= uint32_t(value[position++]) << (8 * i);
    return result;
  }

  bytes take(const bytes& value, size_t& position, size_t length)
  {
    if (length > value.size() - position)
      throw std::runtime_error("truncated QMS crypto backend response");
    bytes result(value.begin() + position, value.begin() + position + length);
    position += length;
    return result;
  }

  template<size_t N>
  std::array<uint8_t, N> take_array(const bytes& value, size_t& position)
  {
    const bytes encoded = take(value, position, N);
    std::array<uint8_t, N> result{};
    std::copy(encoded.begin(), encoded.end(), result.begin());
    return result;
  }

  std::string hex_string(const uint8_t* data, size_t size)
  {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(size * 2);
    for (size_t i = 0; i != size; ++i)
    {
      result.push_back(digits[data[i] >> 4]);
      result.push_back(digits[data[i] & 15]);
    }
    return result;
  }
}

crypto_backend crypto_backend::create()
{
  if (qwc_qms_crypto_abi_version() != 3)
    throw std::runtime_error("unsupported QMS crypto backend ABI");
  owned_buffer output, error;
  checked_call(qwc_qms_crypto_engine_new(output.out(), error.out()), error);
  return crypto_backend(output.copy());
}

crypto_backend::crypto_backend(bytes encoded_state)
  : state_(std::move(encoded_state))
{
  if (state_.empty())
    throw std::runtime_error("empty QMS crypto backend state");
}

crypto_backend::~crypto_backend()
{
  if (!state_.empty()) sodium_memzero(state_.data(), state_.size());
}

crypto_backend::crypto_backend(crypto_backend&& other) noexcept
  : state_(std::move(other.state_))
{
  other.state_.clear();
}

crypto_backend& crypto_backend::operator=(crypto_backend&& other) noexcept
{
  if (this == &other) return *this;
  if (!state_.empty()) sodium_memzero(state_.data(), state_.size());
  state_ = std::move(other.state_);
  other.state_.clear();
  return *this;
}

prepared_contact_package crypto_backend::prepare_contact_package(const hash32& genesis) const
{
  owned_buffer output, error;
  checked_call(qwc_qms_crypto_prepare_contact_package(
    state_.data(), state_.size(), genesis.data(), genesis.size(), output.out(), error.out()), error);
  const bytes encoded = output.copy();
  size_t position = 0;
  prepared_contact_package result;
  result.invitation_id = take_array<16>(encoded, position);
  result.fingerprint = take_array<32>(encoded, position);
  const uint32_t package_size = read_u32(encoded, position);
  result.package = take(encoded, position, package_size);
  result.next_state = take(encoded, position, encoded.size() - position);
  if (result.next_state.empty())
    throw std::runtime_error("missing QMS post-package state");
  return result;
}

prepared_contact_import crypto_backend::prepare_import_contact(
  const id16& local_invitation_id, const bytes& remote_package,
  uint64_t now_unix_seconds) const
{
  owned_buffer output, error;
  checked_call(qwc_qms_crypto_prepare_import_contact(
    state_.data(), state_.size(), local_invitation_id.data(), local_invitation_id.size(),
    remote_package.data(), remote_package.size(), now_unix_seconds,
    output.out(), error.out()), error);
  const bytes encoded = output.copy();
  if (encoded.size() <= 32)
    throw std::runtime_error("truncated QMS contact import response");
  prepared_contact_import result;
  std::copy_n(encoded.begin(), result.fingerprint.size(), result.fingerprint.begin());
  result.contact_id = hex_string(result.fingerprint.data(), result.fingerprint.size());
  result.next_state.assign(encoded.begin() + result.fingerprint.size(), encoded.end());
  return result;
}

prepared_ratchet_send crypto_backend::prepare_send_text(
  const std::string& contact_id, const std::string& text,
  uint64_t now_unix_seconds) const
{
  owned_buffer output, error;
  checked_call(qwc_qms_crypto_prepare_send_text(
    state_.data(), state_.size(),
    reinterpret_cast<const uint8_t*>(contact_id.data()), contact_id.size(),
    reinterpret_cast<const uint8_t*>(text.data()), text.size(), now_unix_seconds,
    output.out(), error.out()), error);
  const bytes encoded = output.copy();
  if (encoded.size() < 1 + 16 + 4)
    throw std::runtime_error("truncated QMS send response");
  size_t position = 0;
  prepared_ratchet_send result;
  result.ciphertext.message_type = encoded[position++];
  result.message_id = take_array<16>(encoded, position);
  const uint32_t ciphertext_size = read_u32(encoded, position);
  result.ciphertext.data = take(encoded, position, ciphertext_size);
  result.next_state = take(encoded, position, encoded.size() - position);
  if (result.next_state.empty())
    throw std::runtime_error("missing QMS post-send state");
  return result;
}

prepared_ratchet_receive crypto_backend::prepare_receive_text(
  const std::string& contact_id, const ratchet_ciphertext& ciphertext) const
{
  owned_buffer output, error;
  checked_call(qwc_qms_crypto_prepare_receive_text(
    state_.data(), state_.size(),
    reinterpret_cast<const uint8_t*>(contact_id.data()), contact_id.size(),
    ciphertext.message_type, ciphertext.data.data(), ciphertext.data.size(),
    output.out(), error.out()), error);
  const bytes encoded = output.copy();
  if (encoded.size() < 16 + 4)
    throw std::runtime_error("truncated QMS receive response");
  size_t position = 0;
  prepared_ratchet_receive result;
  result.message_id = take_array<16>(encoded, position);
  const uint32_t text_size = read_u32(encoded, position);
  const bytes text = take(encoded, position, text_size);
  result.text.assign(reinterpret_cast<const char*>(text.data()), text.size());
  result.next_state = take(encoded, position, encoded.size() - position);
  if (result.next_state.empty())
    throw std::runtime_error("missing QMS post-receive state");
  return result;
}

std::vector<envelope_context> crypto_backend::transport_contexts(
  const std::string& contact_id, bool outgoing) const
{
  owned_buffer output, error;
  checked_call(qwc_qms_crypto_transport_context(
    state_.data(), state_.size(),
    reinterpret_cast<const uint8_t*>(contact_id.data()), contact_id.size(),
    outgoing, output.out(), error.out()), error);
  const bytes encoded = output.copy();
  if (encoded.size() < 4)
    throw std::runtime_error("invalid QMS transport context response");
  size_t position = 0;
  const uint32_t count = read_u32(encoded, position);
  if (count == 0 || encoded.size() - position != size_t(count) * 97)
    throw std::runtime_error("invalid QMS transport context count");
  std::vector<envelope_context> result;
  result.reserve(count);
  for (uint32_t i = 0; i != count; ++i)
  {
    envelope_context context;
    context.genesis = take_array<32>(encoded, position);
    context.invitation_id = take_array<16>(encoded, position);
    context.session_id = take_array<16>(encoded, position);
    context.root_secret = take_array<32>(encoded, position);
    context.direction = encoded[position++];
    result.push_back(context);
  }
  return result;
}

envelope_context crypto_backend::transport_context(
  const std::string& contact_id, bool outgoing) const
{
  const auto contexts = transport_contexts(contact_id, outgoing);
  if (contexts.empty())
    throw std::runtime_error("missing QMS transport context");
  return contexts.front();
}
}
}
