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
  constexpr uint8_t store_version = 1;
  constexpr size_t fixed_header = store_magic.size() + 1 + 8 + 8
    + crypto_pwhash_SALTBYTES + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

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

  bytes associated_data(const bytes& encoded_header, const bytes& context)
  {
    bytes result;
    static constexpr char domain[] = "QWC-QMS2-LOCAL-STORE";
    result.reserve(sizeof(domain) - 1 + encoded_header.size() + context.size());
    append_bytes(result, domain, sizeof(domain) - 1);
    append_bytes(result, encoded_header.data(), encoded_header.size());
    append_bytes(result, context.data(), context.size());
    return result;
  }

  std::array<uint8_t, crypto_aead_xchacha20poly1305_ietf_KEYBYTES> derive_key(
    const std::string& password,
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

bytes encrypt_store(const bytes& plaintext, const std::string& password,
                    const bytes& context)
{
  sodium_ready();
  const uint64_t operations = crypto_pwhash_OPSLIMIT_INTERACTIVE;
  const uint64_t memory = crypto_pwhash_MEMLIMIT_INTERACTIVE;
  bytes result;
  result.reserve(fixed_header + plaintext.size()
    + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  append_bytes(result, store_magic.data(), store_magic.size());
  result.push_back(store_version);
  append_u64(result, operations);
  append_u64(result, memory);
  const size_t salt_offset = result.size();
  result.resize(result.size() + crypto_pwhash_SALTBYTES);
  randombytes_buf(result.data() + salt_offset, crypto_pwhash_SALTBYTES);
  const size_t nonce_offset = result.size();
  result.resize(result.size() + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  randombytes_buf(result.data() + nonce_offset,
                  crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
  const bytes ad = associated_data(
    bytes(result.begin(), result.begin() + nonce_offset), context);
  auto key = derive_key(password, result.data() + salt_offset, operations, memory);
  const size_t ciphertext_offset = result.size();
  result.resize(result.size() + plaintext.size()
    + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long ciphertext_size = 0;
  const int status = crypto_aead_xchacha20poly1305_ietf_encrypt(
    result.data() + ciphertext_offset, &ciphertext_size,
    plaintext.data(), plaintext.size(), ad.data(), ad.size(), nullptr,
    result.data() + nonce_offset, key.data());
  sodium_memzero(key.data(), key.size());
  if (status != 0
      || ciphertext_size != plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES)
    throw std::runtime_error("QMS2 store encryption failed");
  return result;
}

bytes decrypt_store(const bytes& encoded, const std::string& password,
                    const bytes& expected_context)
{
  sodium_ready();
  if (encoded.size() < fixed_header + crypto_aead_xchacha20poly1305_ietf_ABYTES
      || !std::equal(store_magic.begin(), store_magic.end(), encoded.begin())
      || encoded[store_magic.size()] != store_version)
    throw std::runtime_error("invalid QMS2 store header");
  size_t position = store_magic.size() + 1;
  const uint64_t operations = read_u64(encoded, position);
  const uint64_t memory = read_u64(encoded, position);
  const size_t salt_offset = position;
  position += crypto_pwhash_SALTBYTES;
  const size_t nonce_offset = position;
  position += crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  const bytes ad = associated_data(bytes(encoded.begin(), encoded.begin() + nonce_offset),
                                   expected_context);
  auto key = derive_key(password, encoded.data() + salt_offset, operations, memory);
  const size_t ciphertext_size = encoded.size() - position;
  bytes plaintext(ciphertext_size - crypto_aead_xchacha20poly1305_ietf_ABYTES);
  unsigned long long plaintext_size = 0;
  const int status = crypto_aead_xchacha20poly1305_ietf_decrypt(
    plaintext.data(), &plaintext_size, nullptr,
    encoded.data() + position, ciphertext_size,
    ad.data(), ad.size(), encoded.data() + nonce_offset, key.data());
  sodium_memzero(key.data(), key.size());
  if (status != 0 || plaintext_size != plaintext.size())
  {
    sodium_memzero(plaintext.data(), plaintext.size());
    throw std::runtime_error("QMS2 store authentication failed");
  }
  return plaintext;
}
}
}
