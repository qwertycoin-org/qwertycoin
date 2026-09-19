#include "protocol.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

#include <sodium.h>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"

namespace qwertycoin
{
namespace qms
{
namespace
{
  constexpr uint8_t magic[4] = {'Q', 'M', 'S', '1'};
  constexpr char signed_domain[] = "QWC-QMS-SIGNED-MESSAGE-V1";
  constexpr char invite_domain[] = "QWC-QMS-INVITATION-V1";
  constexpr char hint_domain[] = "QWC-QMS-DISCOVERY-HINT-V1";
  constexpr char extract_domain[] = "QWC-QMS-HKDF-EXTRACT-V1";
  constexpr char discovery_info[] = "QWC-QMS-DISCOVERY-KEY-V1";
  constexpr char fragment_info[] = "QWC-QMS-FRAGMENT-MAC-KEY-V1";
  constexpr char fragment_domain[] = "QWC-QMS-FRAGMENT-V1";

  void sodium_ready()
  {
    static const int initialized = sodium_init();
    if (initialized < 0) throw std::runtime_error("libsodium initialization failed");
  }

  template<size_t N> void append(bytes& out, const std::array<uint8_t, N>& value)
  { out.insert(out.end(), value.begin(), value.end()); }
  void append(bytes& out, const void* data, size_t size)
  {
    if (size == 0) return;
    const auto* first = static_cast<const uint8_t*>(data);
    const size_t old_size = out.size();
    out.resize(old_size + size);
    std::memcpy(out.data() + old_size, first, size);
  }
  void append_u16(bytes& out, uint16_t value)
  { out.push_back(value & 0xff); out.push_back((value >> 8) & 0xff); }
  void append_u32(bytes& out, uint32_t value)
  {
    for (unsigned i = 0; i != 4; ++i) out.push_back((value >> (8 * i)) & 0xff);
  }
  uint16_t read_u16(const bytes& in, size_t& pos)
  {
    if (in.size() - pos < 2) throw std::runtime_error("truncated uint16");
    const uint16_t value = in[pos] | (uint16_t(in[pos + 1]) << 8); pos += 2; return value;
  }
  uint32_t read_u32(const bytes& in, size_t& pos)
  {
    if (in.size() - pos < 4) throw std::runtime_error("truncated uint32");
    uint32_t value = 0; for (unsigned i = 0; i != 4; ++i) value |= uint32_t(in[pos++]) << (8 * i); return value;
  }
  template<size_t N> std::array<uint8_t, N> read_array(const bytes& in, size_t& pos)
  {
    if (in.size() - pos < N) throw std::runtime_error("truncated array");
    std::array<uint8_t, N> result{}; std::copy_n(in.begin() + pos, N, result.begin()); pos += N; return result;
  }
  hash32 sha256(const bytes& value)
  {
    hash32 result{}; crypto_hash_sha256(result.data(), value.data(), value.size()); return result;
  }
  hash32 hmac(const hash32& key, const bytes& value)
  {
    hash32 result{};
    crypto_auth_hmacsha256_state state;
    crypto_auth_hmacsha256_init(&state, key.data(), key.size());
    crypto_auth_hmacsha256_update(&state, value.data(), value.size());
    crypto_auth_hmacsha256_final(&state, result.data());
    return result;
  }
  hash32 hkdf_key(const hash32& secret, const char* info, const hash32& genesis,
                  const id16& invitation_id, const hash32& recipient_fingerprint)
  {
    bytes salt; append(salt, extract_domain, sizeof(extract_domain) - 1); append(salt, genesis);
    const hash32 salt_hash = sha256(salt);
    const hash32 prk = hmac(salt_hash, bytes(secret.begin(), secret.end()));
    bytes context; append(context, info, std::strlen(info)); context.push_back(WIRE_VERSION);
    context.push_back(CRYPTO_PROFILE_SEALED_BOX_ED25519); append(context, genesis);
    append(context, invitation_id); append(context, recipient_fingerprint); context.push_back(1);
    return hmac(prk, context);
  }
  bytes invitation_unsigned(const invitation& value)
  {
    bytes out; append(out, invite_domain, sizeof(invite_domain) - 1);
    out.push_back(value.version); out.push_back(value.profile); append(out, value.genesis);
    append(out, value.invitation_id); append(out, value.box_public); append(out, value.sign_public);
    append(out, value.discovery_secret); return out;
  }
  bytes fragment_without_mac(const fragment& value)
  {
    bytes out; append(out, magic, sizeof(magic)); out.push_back(value.version); out.push_back(value.profile);
    append_u16(out, value.flags); append(out, value.message_id); append_u16(out, value.index);
    append_u16(out, value.count); append_u32(out, value.ciphertext_size); append(out, value.discovery_hint);
    append(out, value.ciphertext_hash); return out;
  }
  bytes fragment_mac_input(const fragment& value)
  {
    bytes out; append(out, fragment_domain, sizeof(fragment_domain) - 1);
    const bytes header = fragment_without_mac(value); append(out, header.data(), header.size());
    append(out, value.data.data(), value.data.size()); return out;
  }
  void validate_fragment_shape(const fragment& value)
  {
    if (value.version != WIRE_VERSION || value.profile != CRYPTO_PROFILE_SEALED_BOX_ED25519 || value.flags != 0)
      throw std::runtime_error("unsupported QMS fragment version/profile/flags");
    if (value.count == 0 || value.count > MAX_FRAGMENTS || value.index >= value.count)
      throw std::runtime_error("invalid QMS fragment index/count");
    if (value.ciphertext_size == 0 || value.ciphertext_size > MAX_CIPHERTEXT_BYTES || value.data.size() > MAX_FRAGMENT_DATA_BYTES)
      throw std::runtime_error("invalid QMS fragment size");
    const size_t expected_count = (value.ciphertext_size + MAX_FRAGMENT_DATA_BYTES - 1) / MAX_FRAGMENT_DATA_BYTES;
    if (value.count != expected_count) throw std::runtime_error("non-canonical QMS fragment count");
    const size_t expected_size = value.index + 1 == value.count
      ? value.ciphertext_size - size_t(value.index) * MAX_FRAGMENT_DATA_BYTES : MAX_FRAGMENT_DATA_BYTES;
    if (value.data.size() != expected_size) throw std::runtime_error("non-canonical QMS fragment size");
  }
}

identity generate_identity()
{
  sodium_ready(); identity result;
  crypto_box_keypair(result.box_public.data(), result.box_secret.data());
  crypto_sign_keypair(result.sign_public.data(), result.sign_secret.data());
  return result;
}

hash32 fingerprint(const std::array<uint8_t, 32>& box_public, const std::array<uint8_t, 32>& sign_public)
{
  sodium_ready(); bytes value; append(value, "QWC-QMS-FINGERPRINT-V1", 22); append(value, box_public); append(value, sign_public); return sha256(value);
}

invitation create_invitation(const identity& owner, const hash32& genesis)
{
  sodium_ready(); invitation result; result.genesis = genesis; randombytes_buf(result.invitation_id.data(), result.invitation_id.size());
  result.box_public = owner.box_public; result.sign_public = owner.sign_public;
  randombytes_buf(result.discovery_secret.data(), result.discovery_secret.size());
  const bytes unsigned_data = invitation_unsigned(result);
  crypto_sign_detached(result.signature.data(), nullptr, unsigned_data.data(), unsigned_data.size(), owner.sign_secret.data());
  return result;
}

bytes encode_invitation(const invitation& value)
{
  bytes out; append(out, magic, sizeof(magic)); out.push_back(value.version); out.push_back(value.profile);
  append(out, value.genesis); append(out, value.invitation_id); append(out, value.box_public); append(out, value.sign_public);
  append(out, value.discovery_secret); append(out, value.signature); return out;
}

invitation decode_invitation(const bytes& encoded)
{
  if (encoded.size() != 214 || !std::equal(std::begin(magic), std::end(magic), encoded.begin()))
    throw std::runtime_error("invalid QMS invitation encoding");
  size_t pos = 4; invitation result; result.version = encoded[pos++]; result.profile = encoded[pos++];
  result.genesis = read_array<32>(encoded, pos); result.invitation_id = read_array<16>(encoded, pos);
  result.box_public = read_array<32>(encoded, pos); result.sign_public = read_array<32>(encoded, pos);
  result.discovery_secret = read_array<32>(encoded, pos); result.signature = read_array<64>(encoded, pos);
  if (!verify_invitation(result)) throw std::runtime_error("invalid QMS invitation signature or profile");
  return result;
}

bool verify_invitation(const invitation& value)
{
  sodium_ready(); if (value.version != WIRE_VERSION || value.profile != CRYPTO_PROFILE_SEALED_BOX_ED25519) return false;
  const bytes unsigned_data = invitation_unsigned(value);
  return crypto_sign_verify_detached(value.signature.data(), unsigned_data.data(), unsigned_data.size(), value.sign_public.data()) == 0;
}

bytes seal_text(const identity& sender, const invitation& recipient, const hash32& genesis,
                const id16& message_id, const std::string& text)
{
  sodium_ready(); if (!verify_invitation(recipient) || recipient.genesis != genesis) throw std::runtime_error("recipient invitation mismatch");
  if (text.size() > MAX_TEXT_BYTES || !valid_utf8(text)) throw std::runtime_error("message must be valid UTF-8 and at most 4096 bytes");
  const hash32 sender_fp = fingerprint(sender.box_public, sender.sign_public);
  const hash32 recipient_fp = fingerprint(recipient.box_public, recipient.sign_public);
  bytes body; append(body, signed_domain, sizeof(signed_domain) - 1); body.push_back(WIRE_VERSION);
  body.push_back(CRYPTO_PROFILE_SEALED_BOX_ED25519); append(body, genesis); append(body, message_id);
  append(body, recipient.invitation_id); append(body, sender_fp); append(body, recipient_fp);
  body.push_back(1); append_u32(body, static_cast<uint32_t>(text.size())); append(body, text.data(), text.size());
  std::array<uint8_t, crypto_sign_BYTES> signature{};
  crypto_sign_detached(signature.data(), nullptr, body.data(), body.size(), sender.sign_secret.data());
  bytes plaintext = body; append(plaintext, signature);
  bytes ciphertext(plaintext.size() + crypto_box_SEALBYTES);
  if (crypto_box_seal(ciphertext.data(), plaintext.data(), plaintext.size(), recipient.box_public.data()) != 0)
    throw std::runtime_error("QMS sealed-box encryption failed");
  if (ciphertext.size() > MAX_CIPHERTEXT_BYTES) throw std::runtime_error("QMS ciphertext exceeds transport limit");
  return ciphertext;
}

opened_message open_text(const identity& recipient, const invitation& sender, const invitation& own_invitation,
                         const hash32& expected_genesis, const id16& expected_message_id, const bytes& ciphertext)
{
  sodium_ready(); if (!verify_invitation(sender) || !verify_invitation(own_invitation)) throw std::runtime_error("untrusted QMS invitation");
  if (sender.genesis != expected_genesis || own_invitation.genesis != expected_genesis) throw std::runtime_error("QMS genesis mismatch");
  if (ciphertext.size() < crypto_box_SEALBYTES || ciphertext.size() > MAX_CIPHERTEXT_BYTES) throw std::runtime_error("invalid QMS ciphertext size");
  bytes plaintext(ciphertext.size() - crypto_box_SEALBYTES);
  if (crypto_box_seal_open(plaintext.data(), ciphertext.data(), ciphertext.size(), recipient.box_public.data(), recipient.box_secret.data()) != 0)
    throw std::runtime_error("QMS decryption failed");
  if (plaintext.size() < crypto_sign_BYTES) throw std::runtime_error("truncated QMS signed message");
  const size_t body_size = plaintext.size() - crypto_sign_BYTES; const uint8_t* signature = plaintext.data() + body_size;
  if (crypto_sign_verify_detached(signature, plaintext.data(), body_size, sender.sign_public.data()) != 0)
    throw std::runtime_error("QMS sender signature verification failed");
  bytes body(plaintext.begin(), plaintext.begin() + body_size); size_t pos = 0;
  const size_t domain_size = sizeof(signed_domain) - 1;
  if (body.size() < domain_size || !std::equal(body.begin(), body.begin() + domain_size, signed_domain)) throw std::runtime_error("QMS signed domain mismatch");
  pos += domain_size; if (body.size() - pos < 2 || body[pos++] != WIRE_VERSION || body[pos++] != CRYPTO_PROFILE_SEALED_BOX_ED25519) throw std::runtime_error("QMS signed profile mismatch");
  if (read_array<32>(body, pos) != expected_genesis) throw std::runtime_error("QMS signed genesis mismatch");
  opened_message result; result.message_id = read_array<16>(body, pos); if (result.message_id != expected_message_id) throw std::runtime_error("QMS signed message id mismatch");
  result.invitation_id = read_array<16>(body, pos); if (result.invitation_id != own_invitation.invitation_id) throw std::runtime_error("QMS invitation id mismatch");
  result.sender_fingerprint = read_array<32>(body, pos); result.recipient_fingerprint = read_array<32>(body, pos);
  if (result.sender_fingerprint != fingerprint(sender.box_public, sender.sign_public) || result.recipient_fingerprint != fingerprint(recipient.box_public, recipient.sign_public))
    throw std::runtime_error("QMS pinned fingerprint mismatch");
  if (body.size() - pos < 1 || body[pos++] != 1) throw std::runtime_error("unsupported QMS content type");
  const uint32_t text_size = read_u32(body, pos); if (text_size > MAX_TEXT_BYTES || body.size() - pos != text_size) throw std::runtime_error("invalid QMS text size");
  result.text.assign(reinterpret_cast<const char*>(body.data() + pos), text_size); if (!valid_utf8(result.text)) throw std::runtime_error("invalid QMS UTF-8"); return result;
}

std::vector<fragment> fragment_ciphertext(const invitation& recipient, const hash32& genesis,
                                          const id16& message_id, const bytes& ciphertext)
{
  sodium_ready(); if (!verify_invitation(recipient) || recipient.genesis != genesis || ciphertext.empty() || ciphertext.size() > MAX_CIPHERTEXT_BYTES) throw std::runtime_error("invalid QMS fragmentation input");
  const uint16_t count = (ciphertext.size() + MAX_FRAGMENT_DATA_BYTES - 1) / MAX_FRAGMENT_DATA_BYTES;
  if (count > MAX_FRAGMENTS) throw std::runtime_error("too many QMS fragments");
  const hash32 recipient_fp = fingerprint(recipient.box_public, recipient.sign_public);
  const hash32 hint_key = hkdf_key(recipient.discovery_secret, discovery_info, genesis, recipient.invitation_id, recipient_fp);
  const hash32 mac_key = hkdf_key(recipient.discovery_secret, fragment_info, genesis, recipient.invitation_id, recipient_fp);
  bytes hint_input; append(hint_input, hint_domain, sizeof(hint_domain) - 1); append(hint_input, genesis); append(hint_input, message_id);
  const hash32 full_hint = hmac(hint_key, hint_input); const hash32 cipher_hash = sha256(ciphertext);
  std::vector<fragment> result; result.reserve(count);
  for (uint16_t index = 0; index != count; ++index)
  {
    fragment value; value.message_id = message_id; value.index = index; value.count = count;
    value.ciphertext_size = ciphertext.size(); std::copy_n(full_hint.begin(), value.discovery_hint.size(), value.discovery_hint.begin()); value.ciphertext_hash = cipher_hash;
    const size_t offset = size_t(index) * MAX_FRAGMENT_DATA_BYTES; const size_t size = std::min(MAX_FRAGMENT_DATA_BYTES, ciphertext.size() - offset);
    value.data.assign(ciphertext.begin() + offset, ciphertext.begin() + offset + size);
    const hash32 full_mac = hmac(mac_key, fragment_mac_input(value)); std::copy_n(full_mac.begin(), value.mac.size(), value.mac.begin()); result.push_back(std::move(value));
  }
  return result;
}

bytes encode_fragment(const fragment& value)
{
  validate_fragment_shape(value); bytes out = fragment_without_mac(value); append(out, value.mac); append(out, value.data.data(), value.data.size()); return out;
}

fragment decode_fragment(const bytes& encoded)
{
  if (encoded.size() < FRAGMENT_HEADER_BYTES || !std::equal(std::begin(magic), std::end(magic), encoded.begin())) throw std::runtime_error("invalid QMS fragment encoding");
  size_t pos = 4; fragment result; result.version = encoded[pos++]; result.profile = encoded[pos++]; result.flags = read_u16(encoded, pos);
  result.message_id = read_array<16>(encoded, pos); result.index = read_u16(encoded, pos); result.count = read_u16(encoded, pos);
  result.ciphertext_size = read_u32(encoded, pos); result.discovery_hint = read_array<16>(encoded, pos);
  result.ciphertext_hash = read_array<32>(encoded, pos); result.mac = read_array<16>(encoded, pos);
  result.data.assign(encoded.begin() + pos, encoded.end()); validate_fragment_shape(result); return result;
}

bool verify_fragment(const invitation& recipient, const hash32& genesis, const fragment& value)
{
  try { validate_fragment_shape(value); if (!verify_invitation(recipient) || recipient.genesis != genesis) return false;
    const hash32 recipient_fp = fingerprint(recipient.box_public, recipient.sign_public);
    const hash32 hint_key = hkdf_key(recipient.discovery_secret, discovery_info, genesis, recipient.invitation_id, recipient_fp);
    const hash32 mac_key = hkdf_key(recipient.discovery_secret, fragment_info, genesis, recipient.invitation_id, recipient_fp);
    bytes hint_input; append(hint_input, hint_domain, sizeof(hint_domain) - 1); append(hint_input, genesis); append(hint_input, value.message_id);
    const hash32 hint = hmac(hint_key, hint_input); if (sodium_memcmp(hint.data(), value.discovery_hint.data(), value.discovery_hint.size()) != 0) return false;
    const hash32 mac = hmac(mac_key, fragment_mac_input(value)); return sodium_memcmp(mac.data(), value.mac.data(), value.mac.size()) == 0;
  } catch (...) { return false; }
}

bytes reassemble(const std::vector<fragment>& fragments)
{
  if (fragments.empty() || fragments.size() > MAX_FRAGMENTS) throw std::runtime_error("invalid QMS fragment set");
  std::map<uint16_t, fragment> ordered;
  for (const fragment& value : fragments) { validate_fragment_shape(value); auto inserted = ordered.emplace(value.index, value); if (!inserted.second && encode_fragment(inserted.first->second) != encode_fragment(value)) throw std::runtime_error("conflicting QMS duplicate"); }
  const fragment& first = ordered.begin()->second; if (ordered.size() != first.count) throw std::runtime_error("incomplete QMS fragment set");
  bytes result; result.reserve(first.ciphertext_size);
  for (uint16_t i = 0; i != first.count; ++i) { auto it = ordered.find(i); if (it == ordered.end()) throw std::runtime_error("missing QMS fragment");
    const fragment& value = it->second; if (value.message_id != first.message_id || value.count != first.count || value.ciphertext_size != first.ciphertext_size || value.ciphertext_hash != first.ciphertext_hash) throw std::runtime_error("inconsistent QMS fragments"); append(result, value.data.data(), value.data.size()); }
  if (result.size() != first.ciphertext_size || sha256(result) != first.ciphertext_hash) throw std::runtime_error("QMS ciphertext hash mismatch");
  return result;
}

std::vector<bytes> encode_segments(const fragment& value)
{
  const bytes record = encode_fragment(value); const size_t count = (record.size() + MAX_SEGMENT_DATA_BYTES - 1) / MAX_SEGMENT_DATA_BYTES;
  if (count == 0 || count > MAX_SEGMENTS_PER_FRAGMENT) throw std::runtime_error("QMS fragment exceeds nonce segment budget");
  std::vector<bytes> result; for (size_t index = 0; index != count; ++index) { bytes nonce; nonce.push_back(NONCE_SUBTYPE); append(nonce, magic, sizeof(magic)); nonce.push_back(index); nonce.push_back(count);
    const size_t offset = index * MAX_SEGMENT_DATA_BYTES; const size_t size = std::min(MAX_SEGMENT_DATA_BYTES, record.size() - offset); append(nonce, record.data() + offset, size); result.push_back(std::move(nonce)); }
  return result;
}

fragment decode_segments(const std::vector<bytes>& nonces)
{
  if (nonces.empty() || nonces.size() > MAX_SEGMENTS_PER_FRAGMENT) throw std::runtime_error("invalid QMS segment count");
  std::map<uint8_t, bytes> ordered; uint8_t count = 0;
  for (const bytes& nonce : nonces) { if (nonce.size() < SEGMENT_WRAPPER_BYTES || nonce.size() > 255 || nonce[0] != NONCE_SUBTYPE || !std::equal(std::begin(magic), std::end(magic), nonce.begin() + 1)) throw std::runtime_error("invalid QMS nonce segment");
    if (!count) count = nonce[6];
    if (nonce[6] != count || count == 0 || count > MAX_SEGMENTS_PER_FRAGMENT || nonce[5] >= count) throw std::runtime_error("invalid QMS nonce segment index/count");
    bytes data(nonce.begin() + SEGMENT_WRAPPER_BYTES, nonce.end()); if (!ordered.emplace(nonce[5], std::move(data)).second) throw std::runtime_error("duplicate QMS nonce segment"); }
  if (ordered.size() != count) throw std::runtime_error("missing QMS nonce segment");
  bytes record;
  for (uint8_t i = 0; i != count; ++i) { auto it = ordered.find(i); if (it == ordered.end()) throw std::runtime_error("missing QMS nonce segment"); append(record, it->second.data(), it->second.size()); }
  return decode_fragment(record);
}

bool append_carrier_nonces(std::vector<uint8_t>& extra, const fragment& value)
{
  const std::vector<bytes> segments = encode_segments(value); std::vector<uint8_t> candidate = extra;
  for (const bytes& segment : segments) if (!cryptonote::add_extra_nonce_to_tx_extra(candidate, std::string(segment.begin(), segment.end()))) return false;
  if (candidate.size() > MAX_TX_EXTRA_SIZE) return false;
  std::vector<uint8_t> sorted;
  if (!cryptonote::sort_tx_extra(candidate, sorted)) return false;
  if (sorted.size() > MAX_TX_EXTRA_SIZE) return false;
  extra = std::move(sorted);
  return true;
}

  std::vector<fragment> extract_carrier_fragments(const std::vector<uint8_t>& extra)
  {
  std::vector<cryptonote::tx_extra_field> fields; if (!cryptonote::parse_tx_extra(extra, fields)) return {};
  std::vector<bytes> qms_nonces; for (const auto& field : fields) if (field.type() == typeid(cryptonote::tx_extra_nonce)) { const auto& nonce = boost::get<cryptonote::tx_extra_nonce>(field).nonce;
    if (nonce.size() >= SEGMENT_WRAPPER_BYTES && uint8_t(nonce[0]) == NONCE_SUBTYPE) qms_nonces.emplace_back(nonce.begin(), nonce.end()); }
  if (qms_nonces.empty()) return {};
  // The MVP deliberately permits exactly one carrier fragment per transaction.
  // This also prevents ambiguous grouping if an attacker interleaves segment sets.
  return {decode_segments(qms_nonces)};
  }

bool valid_utf8(const std::string& text)
{
  const auto* s = reinterpret_cast<const uint8_t*>(text.data()); size_t i = 0;
  while (i < text.size()) { uint32_t cp; size_t n; if (s[i] <= 0x7f) { cp = s[i]; n = 1; }
    else if ((s[i] & 0xe0) == 0xc0) { cp = s[i] & 0x1f; n = 2; }
    else if ((s[i] & 0xf0) == 0xe0) { cp = s[i] & 0x0f; n = 3; }
    else if ((s[i] & 0xf8) == 0xf0) { cp = s[i] & 0x07; n = 4; } else return false;
    if (i + n > text.size()) return false;
    for (size_t j = 1; j != n; ++j) { if ((s[i + j] & 0xc0) != 0x80) return false; cp = (cp << 6) | (s[i + j] & 0x3f); }
    if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    i += n;
  }
  return true;
}

std::string hex(const bytes& value)
{
  static const char digits[] = "0123456789abcdef"; std::string result; result.reserve(value.size() * 2);
  for (uint8_t byte : value) { result.push_back(digits[byte >> 4]); result.push_back(digits[byte & 15]); } return result;
}
}
}
