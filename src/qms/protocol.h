#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace qwertycoin
{
namespace qms
{
  constexpr uint8_t WIRE_VERSION = 1;
  constexpr uint8_t CRYPTO_PROFILE_SEALED_BOX_ED25519 = 1;
  // 0x70 and 0x71 are already assigned to EPoSE registration/attestation.
  constexpr uint8_t NONCE_SUBTYPE = 0x72;
  constexpr size_t MAX_TEXT_BYTES = 4096;
  constexpr size_t MAX_CIPHERTEXT_BYTES = 9600;
  constexpr size_t MAX_FRAGMENT_DATA_BYTES = 600;
  constexpr size_t MAX_FRAGMENTS = 16;
  constexpr size_t FRAGMENT_HEADER_BYTES = 96;
  constexpr size_t SEGMENT_WRAPPER_BYTES = 7;
  constexpr size_t MAX_SEGMENT_DATA_BYTES = 248;
  constexpr size_t MAX_SEGMENTS_PER_FRAGMENT = 3;

  using bytes = std::vector<uint8_t>;
  using id16 = std::array<uint8_t, 16>;
  using hash32 = std::array<uint8_t, 32>;

  struct identity
  {
    std::array<uint8_t, 32> box_public{};
    std::array<uint8_t, 32> box_secret{};
    std::array<uint8_t, 32> sign_public{};
    std::array<uint8_t, 64> sign_secret{};
  };

  struct invitation
  {
    uint8_t version = WIRE_VERSION;
    uint8_t profile = CRYPTO_PROFILE_SEALED_BOX_ED25519;
    hash32 genesis{};
    id16 invitation_id{};
    std::array<uint8_t, 32> box_public{};
    std::array<uint8_t, 32> sign_public{};
    hash32 discovery_secret{};
    std::array<uint8_t, 64> signature{};
  };

  struct fragment
  {
    uint8_t version = WIRE_VERSION;
    uint8_t profile = CRYPTO_PROFILE_SEALED_BOX_ED25519;
    uint16_t flags = 0;
    id16 message_id{};
    uint16_t index = 0;
    uint16_t count = 0;
    uint32_t ciphertext_size = 0;
    id16 discovery_hint{};
    hash32 ciphertext_hash{};
    id16 mac{};
    bytes data;
  };

  struct opened_message
  {
    id16 message_id{};
    id16 invitation_id{};
    hash32 sender_fingerprint{};
    hash32 recipient_fingerprint{};
    std::string text;
  };

  identity generate_identity();
  hash32 fingerprint(const std::array<uint8_t, 32>& box_public,
                     const std::array<uint8_t, 32>& sign_public);
  invitation create_invitation(const identity& owner, const hash32& genesis);
  bytes encode_invitation(const invitation& value);
  invitation decode_invitation(const bytes& encoded);
  bool verify_invitation(const invitation& value);

  bytes seal_text(const identity& sender, const invitation& recipient,
                  const hash32& genesis, const id16& message_id,
                  const std::string& text);
  opened_message open_text(const identity& recipient, const invitation& sender,
                           const invitation& own_invitation,
                           const hash32& expected_genesis,
                           const id16& expected_message_id,
                           const bytes& ciphertext);

  std::vector<fragment> fragment_ciphertext(const invitation& recipient,
                                            const hash32& genesis,
                                            const id16& message_id,
                                            const bytes& ciphertext);
  bytes encode_fragment(const fragment& value);
  fragment decode_fragment(const bytes& encoded);
  bool verify_fragment(const invitation& recipient, const hash32& genesis,
                       const fragment& value);
  bytes reassemble(const std::vector<fragment>& fragments);

  std::vector<bytes> encode_segments(const fragment& value);
  fragment decode_segments(const std::vector<bytes>& nonces);
  bool append_carrier_nonces(std::vector<uint8_t>& extra, const fragment& value);
  std::vector<fragment> extract_carrier_fragments(const std::vector<uint8_t>& extra);

  bool valid_utf8(const std::string& text);
  std::string hex(const bytes& value);
}
}
