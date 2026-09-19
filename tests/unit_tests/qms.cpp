#include <gtest/gtest.h>

#include "qms/protocol.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"

namespace
{
  qwertycoin::qms::hash32 genesis(uint8_t seed)
  {
    qwertycoin::qms::hash32 result{};
    for (size_t i = 0; i != result.size(); ++i) result[i] = seed + i;
    return result;
  }
  qwertycoin::qms::id16 message_id(uint8_t seed)
  {
    qwertycoin::qms::id16 result{};
    for (size_t i = 0; i != result.size(); ++i) result[i] = seed + i;
    return result;
  }
}

TEST(qms, invitation_roundtrip_and_tamper)
{
  const auto alice = qwertycoin::qms::generate_identity();
  const auto invitation = qwertycoin::qms::create_invitation(alice, genesis(1));
  const auto encoded = qwertycoin::qms::encode_invitation(invitation);
  EXPECT_EQ(invitation.invitation_id, qwertycoin::qms::decode_invitation(encoded).invitation_id);
  auto tampered = encoded; tampered[40] ^= 1;
  EXPECT_THROW(qwertycoin::qms::decode_invitation(tampered), std::runtime_error);
}

TEST(qms, seal_fragment_segment_reassemble_open)
{
  const auto network = genesis(9);
  const auto alice = qwertycoin::qms::generate_identity();
  const auto bob = qwertycoin::qms::generate_identity();
  const auto alice_invite = qwertycoin::qms::create_invitation(alice, network);
  const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  const auto id = message_id(7);
  const std::string text = u8"QMS: Grüße 👋 — encrypted end to end";
  const auto ciphertext = qwertycoin::qms::seal_text(alice, bob_invite, network, id, text);
  auto fragments = qwertycoin::qms::fragment_ciphertext(bob_invite, network, id, ciphertext);
  ASSERT_FALSE(fragments.empty());
  std::reverse(fragments.begin(), fragments.end());
  for (const auto& fragment : fragments)
  {
    EXPECT_TRUE(qwertycoin::qms::verify_fragment(bob_invite, network, fragment));
    auto segments = qwertycoin::qms::encode_segments(fragment);
    std::reverse(segments.begin(), segments.end());
    EXPECT_EQ(qwertycoin::qms::encode_fragment(fragment), qwertycoin::qms::encode_fragment(qwertycoin::qms::decode_segments(segments)));
  }
  const auto opened = qwertycoin::qms::open_text(bob, alice_invite, bob_invite, network, id, qwertycoin::qms::reassemble(fragments));
  EXPECT_EQ(text, opened.text);
}

TEST(qms, rejects_wrong_identity_genesis_sender_and_tamper)
{
  const auto network = genesis(3); const auto id = message_id(2);
  const auto alice = qwertycoin::qms::generate_identity(); const auto mallory = qwertycoin::qms::generate_identity(); const auto bob = qwertycoin::qms::generate_identity();
  const auto alice_invite = qwertycoin::qms::create_invitation(alice, network); const auto mallory_invite = qwertycoin::qms::create_invitation(mallory, network); const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  auto ciphertext = qwertycoin::qms::seal_text(alice, bob_invite, network, id, "hello");
  EXPECT_THROW(qwertycoin::qms::open_text(mallory, alice_invite, bob_invite, network, id, ciphertext), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::open_text(bob, mallory_invite, bob_invite, network, id, ciphertext), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::open_text(bob, alice_invite, bob_invite, genesis(4), id, ciphertext), std::runtime_error);
  ciphertext.back() ^= 1;
  EXPECT_THROW(qwertycoin::qms::open_text(bob, alice_invite, bob_invite, network, id, ciphertext), std::runtime_error);
}

TEST(qms, utf8_and_4096_byte_limit)
{
  const auto network = genesis(5); const auto id = message_id(4);
  const auto alice = qwertycoin::qms::generate_identity(); const auto bob = qwertycoin::qms::generate_identity();
  const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  EXPECT_NO_THROW(qwertycoin::qms::seal_text(alice, bob_invite, network, id, std::string(4096, 'x')));
  EXPECT_THROW(qwertycoin::qms::seal_text(alice, bob_invite, network, id, std::string(4097, 'x')), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::seal_text(alice, bob_invite, network, id, std::string("\xc0\xaf", 2)), std::runtime_error);
}

TEST(qms, carrier_uses_existing_nonce_fields_and_limits)
{
  const auto network = genesis(6); const auto id = message_id(5);
  const auto alice = qwertycoin::qms::generate_identity(); const auto bob = qwertycoin::qms::generate_identity();
  const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  const auto ciphertext = qwertycoin::qms::seal_text(alice, bob_invite, network, id, std::string(4096, 'z'));
  const auto fragments = qwertycoin::qms::fragment_ciphertext(bob_invite, network, id, ciphertext);
  ASSERT_GT(fragments.size(), 1u); ASSERT_LE(fragments.size(), qwertycoin::qms::MAX_FRAGMENTS);
  for (const auto& fragment : fragments)
  {
    std::vector<uint8_t> extra;
    ASSERT_TRUE(qwertycoin::qms::append_carrier_nonces(extra, fragment));
    EXPECT_LE(extra.size(), size_t(MAX_TX_EXTRA_SIZE));
    const auto parsed = qwertycoin::qms::extract_carrier_fragments(extra);
    ASSERT_EQ(1u, parsed.size()); EXPECT_EQ(qwertycoin::qms::encode_fragment(fragment), qwertycoin::qms::encode_fragment(parsed[0]));
  }
}

TEST(qms, incomplete_conflicting_and_bad_mac_rejected)
{
  const auto network = genesis(8); const auto id = message_id(8);
  const auto alice = qwertycoin::qms::generate_identity(); const auto bob = qwertycoin::qms::generate_identity(); const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  const auto ciphertext = qwertycoin::qms::seal_text(alice, bob_invite, network, id, std::string(1000, 'a'));
  auto fragments = qwertycoin::qms::fragment_ciphertext(bob_invite, network, id, ciphertext); ASSERT_GT(fragments.size(), 1u);
  auto incomplete = fragments; incomplete.pop_back(); EXPECT_THROW(qwertycoin::qms::reassemble(incomplete), std::runtime_error);
  auto bad = fragments.front(); bad.data[0] ^= 1; EXPECT_FALSE(qwertycoin::qms::verify_fragment(bob_invite, network, bad));
  auto conflicting = fragments; conflicting.push_back(fragments.front()); conflicting.back().data[0] ^= 1; EXPECT_THROW(qwertycoin::qms::reassemble(conflicting), std::runtime_error);
}

TEST(qms, existing_nonce_varint_boundaries_and_extra_limit_are_unchanged)
{
  for (const size_t size : {size_t(127), size_t(128), size_t(255)})
  {
    std::vector<uint8_t> extra;
    const std::string nonce(size, '\x42');
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(extra, nonce));
    std::vector<cryptonote::tx_extra_field> fields;
    ASSERT_TRUE(cryptonote::parse_tx_extra(extra, fields));
    ASSERT_EQ(1u, fields.size());
    EXPECT_EQ(nonce, boost::get<cryptonote::tx_extra_nonce>(fields[0]).nonce);
  }
  std::vector<uint8_t> extra;
  EXPECT_FALSE(cryptonote::add_extra_nonce_to_tx_extra(extra, std::string(256, '\x42')));
  EXPECT_EQ(1060, MAX_TX_EXTRA_SIZE);
}

TEST(qms, malformed_segments_and_trailing_bytes_are_rejected)
{
  const auto network = genesis(12); const auto id = message_id(3);
  const auto alice = qwertycoin::qms::generate_identity(); const auto bob = qwertycoin::qms::generate_identity();
  const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  const auto ciphertext = qwertycoin::qms::seal_text(alice, bob_invite, network, id, std::string(900, 'q'));
  auto fragment = qwertycoin::qms::fragment_ciphertext(bob_invite, network, id, ciphertext).front();
  auto segments = qwertycoin::qms::encode_segments(fragment); ASSERT_GT(segments.size(), 1u);
  auto missing = segments; missing.pop_back(); EXPECT_THROW(qwertycoin::qms::decode_segments(missing), std::runtime_error);
  auto duplicate = segments; duplicate[1][5] = duplicate[0][5]; EXPECT_THROW(qwertycoin::qms::decode_segments(duplicate), std::runtime_error);
  auto bad_count = segments; bad_count[0][6] = 4; EXPECT_THROW(qwertycoin::qms::decode_segments(bad_count), std::runtime_error);
  auto trailing = qwertycoin::qms::encode_fragment(fragment); trailing.push_back(0);
  EXPECT_THROW(qwertycoin::qms::decode_fragment(trailing), std::runtime_error);
}
