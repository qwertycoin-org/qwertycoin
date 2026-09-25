#include "secure_store.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <sodium.h>

namespace qwertycoin
{
namespace qms
{
namespace
{
  constexpr std::array<uint8_t, 8> store_magic{{'Q', 'M', 'S', '2', 'S', 'T', 'R', '1'}};
  constexpr uint8_t store_version = 2;
  constexpr size_t store_key_bytes = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
  constexpr size_t wrapped_key_bytes = store_key_bytes
    + crypto_aead_xchacha20poly1305_ietf_ABYTES;
  constexpr size_t maximum_context_bytes = 1024;

  struct parsed_store
  {
    bytes stable_header;
    bytes wrap_header;
    bytes context;
    uint64_t operations = 0;
    uint64_t memory = 0;
    const uint8_t* salt = nullptr;
    const uint8_t* wrap_nonce = nullptr;
    const uint8_t* wrapped_key = nullptr;
    const uint8_t* data_nonce = nullptr;
    const uint8_t* ciphertext = nullptr;
    size_t ciphertext_size = 0;
  };

  void sodium_ready()
  {
    static const int initialized = sodium_init();
    if (initialized < 0) throw std::runtime_error("libsodium initialization failed");
  }

  void append_u64(bytes& output, uint64_t value)
  {
    for (unsigned i = 0; i != 8; ++i)
      output.push_back(uint8_t(value >> (8 * i)));
  }

  void append_bytes(bytes& output, const void* source, size_t size)
  {
    if (size == 0) return;
    const size_t offset = output.size();
    output.resize(offset + size);
    std::memcpy(output.data() + offset, source, size);
  }

  uint64_t read_u64(const bytes& input, size_t& position)
  {
    if (input.size() - position < 8)
      throw std::runtime_error("truncated QMS2 store header");
    uint64_t value = 0;
    for (unsigned i = 0; i != 8; ++i)
      value |= uint64_t(input[position++]) << (8 * i);
    return value;
  }

  bytes associated_data(const char* domain, const bytes& encoded_header)
  {
    bytes result;
    result.reserve(std::strlen(domain) + encoded_header.size());
    append_bytes(result, domain, std::strlen(domain));
    append_bytes(result, encoded_header.data(), encoded_header.size());
    return result;
  }

  std::array<uint8_t, store_key_bytes> derive_key(
    boost::string_ref password, const uint8_t* salt,
    uint64_t operations, uint64_t memory);

  parsed_store parse_store(const bytes& encoded)
  {
    const size_t minimum = store_magic.size() + 1 + 8 + 1 + 8 + 8
      + crypto_pwhash_SALTBYTES
      + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
      + wrapped_key_bytes
      + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
      + crypto_aead_xchacha20poly1305_ietf_ABYTES;
    if (encoded.size() < minimum
        || !std::equal(store_magic.begin(), store_magic.end(), encoded.begin())
        || encoded[store_magic.size()] != store_version)
      throw std::runtime_error("invalid QMS2 store header");

    parsed_store result;
    size_t position = store_magic.size() + 1;
    const uint64_t context_size = read_u64(encoded, position);
    if (context_size == 0 || context_size > maximum_context_bytes
        || context_size > encoded.size() - position)
      throw std::runtime_error("invalid QMS2 store context");
    result.context.assign(encoded.begin() + position,
                          encoded.begin() + position + context_size);
    position += context_size;
    result.stable_header.assign(encoded.begin(), encoded.begin() + position);
    result.operations = read_u64(encoded, position);
    result.memory = read_u64(encoded, position);
    if (encoded.size() - position < crypto_pwhash_SALTBYTES
        + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + wrapped_key_bytes
        + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + crypto_aead_xchacha20poly1305_ietf_ABYTES)
      throw std::runtime_error("truncated QMS2 store header");
    result.salt = encoded.data() + position;
    position += crypto_pwhash_SALTBYTES;
    result.wrap_nonce = encoded.data() + position;
    position += crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    result.wrap_header.assign(encoded.begin(), encoded.begin() + position);
    result.wrapped_key = encoded.data() + position;
    position += wrapped_key_bytes;
    result.data_nonce = encoded.data() + position;
    position += crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    result.ciphertext = encoded.data() + position;
    result.ciphertext_size = encoded.size() - position;
    return result;
  }

  std::array<uint8_t, store_key_bytes> unwrap_store_key(
    const parsed_store& parsed, boost::string_ref password)
  {
    auto wrapping_key = derive_key(password, parsed.salt, parsed.operations, parsed.memory);
    const bytes ad = associated_data("QWC-QMS2-STORE-KEY", parsed.wrap_header);
    std::array<uint8_t, store_key_bytes> store_key{};
    unsigned long long clear_size = 0;
    const int status = crypto_aead_xchacha20poly1305_ietf_decrypt(
      store_key.data(), &clear_size, nullptr,
      parsed.wrapped_key, wrapped_key_bytes, ad.data(), ad.size(),
      parsed.wrap_nonce, wrapping_key.data());
    sodium_memzero(wrapping_key.data(), wrapping_key.size());
    if (status != 0 || clear_size != store_key.size())
    {
      sodium_memzero(store_key.data(), store_key.size());
      throw std::runtime_error("QMS2 store key authentication failed");
    }
    return store_key;
  }

  std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> derive_key(
    boost::string_ref password,
    const uint8_t* salt,
    uint64_t operations,
    uint64_t memory)
  {
    if (password.empty())
      throw std::runtime_error("QMS2 store requires a non-empty wallet password");
    if (operations < crypto_pwhash_OPSLIMIT_INTERACTIVE
        || operations > crypto_pwhash_OPSLIMIT_MODERATE
        || memory < crypto_pwhash_MEMLIMIT_INTERACTIVE
        || memory > crypto_pwhash_MEMLIMIT_MODERATE
        || memory > std::numeric_limits<size_t>::max())
      throw std::runtime_error("QMS2 store Argon2id parameters are outside safe bounds");
    std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> key{};
    if (crypto_pwhash(key.data(), key.size(), password.data(), password.size(), salt,
                      operations, static_cast<size_t>(memory), crypto_pwhash_ALG_ARGON2ID13) != 0)
      throw std::runtime_error("QMS2 store Argon2id derivation failed");
    return key;
  }
}

bytes encrypt_store(const bytes& plaintext, boost::string_ref password,
                    const bytes& context)
{
  sodium_ready();
  if (context.empty() || context.size() > maximum_context_bytes)
    throw std::runtime_error("QMS2 store context is outside supported bounds");
  const uint64_t operations = crypto_pwhash_OPSLIMIT_INTERACTIVE;
  const uint64_t memory = crypto_pwhash_MEMLIMIT_INTERACTIVE;
  bytes result;
  result.reserve(store_magic.size() + 1 + 8 + context.size() + 8 + 8
    + crypto_pwhash_SALTBYTES
    + 2 * crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
    + wrapped_key_bytes + plaintext.size()
    + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  append_bytes(result, store_magic.data(), store_magic.size());
  result.push_back(store_version);
  append_u64(result, context.size());
  append_bytes(result, context.data(), context.size());
  const bytes stable_header(result);
  append_u64(result, operations);
  append_u64(result, memory);
  const size_t salt_offset = result.size();
  result.resize(result.size() + crypto_pwhash_SALTBYTES);
  randombytes_buf(result.data() + salt_offset, crypto_pwhash_SALTBYTES);
  const size_t wrap_nonce_offset = result.size();
  result.resize(result.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  randombytes_buf(result.data() + wrap_nonce_offset,
                  crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  const bytes wrap_header(result);
  auto wrapping_key = derive_key(password, result.data() + salt_offset, operations, memory);
  std::array<uint8_t, store_key_bytes> store_key{};
  randombytes_buf(store_key.data(), store_key.size());
  const bytes wrap_ad = associated_data("QWC-QMS2-STORE-KEY", wrap_header);
  const size_t wrapped_key_offset = result.size();
  result.resize(result.size() + wrapped_key_bytes);
  unsigned long long wrapped_size = 0;
  const int wrap_status = crypto_aead_xchacha20poly1305_ietf_encrypt(
    result.data() + wrapped_key_offset, &wrapped_size,
    store_key.data(), store_key.size(), wrap_ad.data(), wrap_ad.size(), nullptr,
    result.data() + wrap_nonce_offset, wrapping_key.data());
  sodium_memzero(wrapping_key.data(), wrapping_key.size());
  if (wrap_status != 0 || wrapped_size != wrapped_key_bytes)
  {
    sodium_memzero(store_key.data(), store_key.size());
    throw std::runtime_error("QMS2 store key wrapping failed");
  }

  const size_t data_nonce_offset = result.size();
  result.resize(result.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  randombytes_buf(result.data() + data_nonce_offset,
                  crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  const bytes data_ad = associated_data("QWC-QMS2-STORE-DATA", stable_header);
  const size_t ciphertext_offset = result.size();
  result.resize(result.size() + plaintext.size()
    + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long ciphertext_size = 0;
  const int status = crypto_aead_xchacha20poly1305_ietf_encrypt(
    result.data() + ciphertext_offset, &ciphertext_size,
    plaintext.data(), plaintext.size(), data_ad.data(), data_ad.size(), nullptr,
    result.data() + data_nonce_offset, store_key.data());
  sodium_memzero(store_key.data(), store_key.size());
  if (status != 0
      || ciphertext_size != plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES)
    throw std::runtime_error("QMS2 store encryption failed");
  return result;
}

bytes decrypt_store(const bytes& encoded, boost::string_ref password,
                    const bytes& expected_context)
{
  sodium_ready();
  const parsed_store parsed = parse_store(encoded);
  if (parsed.context != expected_context)
    throw std::runtime_error("QMS2 store context mismatch");
  auto store_key = unwrap_store_key(parsed, password);
  const bytes ad = associated_data("QWC-QMS2-STORE-DATA", parsed.stable_header);
  bytes plaintext(parsed.ciphertext_size
    - crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long plaintext_size = 0;
  const int status = crypto_aead_xchacha20poly1305_ietf_decrypt(
    plaintext.data(), &plaintext_size, nullptr,
    parsed.ciphertext, parsed.ciphertext_size,
    ad.data(), ad.size(), parsed.data_nonce, store_key.data());
  sodium_memzero(store_key.data(), store_key.size());
  if (status != 0 || plaintext_size != plaintext.size())
  {
    sodium_memzero(plaintext.data(), plaintext.size());
    throw std::runtime_error("QMS2 store authentication failed");
  }
  return plaintext;
}

bytes rewrap_store(const bytes& encoded, boost::string_ref old_password,
                   boost::string_ref new_password)
{
  sodium_ready();
  const parsed_store parsed = parse_store(encoded);
  auto store_key = unwrap_store_key(parsed, old_password);
  const uint64_t operations = crypto_pwhash_OPSLIMIT_INTERACTIVE;
  const uint64_t memory = crypto_pwhash_MEMLIMIT_INTERACTIVE;

  bytes result(parsed.stable_header);
  append_u64(result, operations);
  append_u64(result, memory);
  const size_t salt_offset = result.size();
  result.resize(result.size() + crypto_pwhash_SALTBYTES);
  randombytes_buf(result.data() + salt_offset, crypto_pwhash_SALTBYTES);
  const size_t wrap_nonce_offset = result.size();
  result.resize(result.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  randombytes_buf(result.data() + wrap_nonce_offset,
                  crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  const bytes wrap_header(result);
  auto wrapping_key = derive_key(new_password, result.data() + salt_offset,
                                 operations, memory);
  const bytes wrap_ad = associated_data("QWC-QMS2-STORE-KEY", wrap_header);
  const size_t wrapped_key_offset = result.size();
  result.resize(result.size() + wrapped_key_bytes);
  unsigned long long wrapped_size = 0;
  const int status = crypto_aead_xchacha20poly1305_ietf_encrypt(
    result.data() + wrapped_key_offset, &wrapped_size,
    store_key.data(), store_key.size(), wrap_ad.data(), wrap_ad.size(), nullptr,
    result.data() + wrap_nonce_offset, wrapping_key.data());
  sodium_memzero(wrapping_key.data(), wrapping_key.size());
  sodium_memzero(store_key.data(), store_key.size());
  if (status != 0 || wrapped_size != wrapped_key_bytes)
    throw std::runtime_error("QMS2 store key rewrapping failed");

  append_bytes(result, parsed.data_nonce,
               crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  append_bytes(result, parsed.ciphertext, parsed.ciphertext_size);
  return result;
}
}
}
