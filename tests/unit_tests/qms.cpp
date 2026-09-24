#include <gtest/gtest.h>

#include "qms/protocol.h"
#include "qms/secure_store.h"
#ifdef QWC_ENABLE_QMS2_CRYPTO
#include "qms/crypto_backend.h"
#endif
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
  qwertycoin::qms::envelope_context envelope_context(uint8_t seed, uint8_t direction = 0)
  {
    qwertycoin::qms::envelope_context result;
    result.genesis = genesis(seed);
    result.invitation_id = message_id(seed + 1);
    result.session_id = message_id(seed + 2);
    for (size_t i = 0; i != result.root_secret.size(); ++i)
      result.root_secret[i] = seed + 3 + i;
    result.direction = direction;
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

TEST(qms, profile2_outer_envelope_uses_canonical_padding_and_aad)
{
  const auto context = envelope_context(21);
  const auto id = message_id(22);
  for (const auto& item : std::vector<std::pair<size_t, size_t>>{
         {1, 1200}, {1156, 1200}, {1157, 2400}, {4096 + 104 + 1700, 7200}})
  {
    const qwertycoin::qms::bytes inner(item.first, uint8_t(item.first));
    const auto envelope = qwertycoin::qms::seal_outer_envelope(context, id, inner);
    ASSERT_EQ(item.second, envelope.size());
    EXPECT_EQ(inner, qwertycoin::qms::open_outer_envelope(context, id, envelope));
  }

  const qwertycoin::qms::bytes maximum_inner(9556, 0x5a);
  const auto maximum = qwertycoin::qms::seal_outer_envelope(context, id, maximum_inner);
  ASSERT_EQ(9600u, maximum.size());
  EXPECT_EQ(maximum_inner, qwertycoin::qms::open_outer_envelope(context, id, maximum));
  EXPECT_THROW(qwertycoin::qms::seal_outer_envelope(context, id,
    qwertycoin::qms::bytes(9557, 0)), std::runtime_error);

  auto tampered = maximum;
  tampered.back() ^= 1;
  EXPECT_THROW(qwertycoin::qms::open_outer_envelope(context, id, tampered), std::runtime_error);
  auto wrong_direction = context;
  wrong_direction.direction = 1;
  EXPECT_THROW(qwertycoin::qms::open_outer_envelope(wrong_direction, id, maximum), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::open_outer_envelope(context, message_id(23), maximum), std::runtime_error);
  auto wrong_genesis = context;
  wrong_genesis.genesis[0] ^= 1;
  EXPECT_THROW(qwertycoin::qms::open_outer_envelope(wrong_genesis, id, maximum), std::runtime_error);
}

TEST(qms, profile2_fragment_transport_preserves_9600_byte_envelope)
{
  const auto context = envelope_context(31, 1);
  const auto id = message_id(32);
  const qwertycoin::qms::bytes inner(8000, 0xa5);
  const auto envelope = qwertycoin::qms::seal_outer_envelope(context, id, inner);
  ASSERT_EQ(9600u, envelope.size());
  auto fragments = qwertycoin::qms::fragment_envelope(context, id, envelope);
  ASSERT_EQ(16u, fragments.size());
  for (const auto& fragment : fragments)
  {
    EXPECT_EQ(qwertycoin::qms::WIRE_VERSION_TRIPLE_RATCHET, fragment.version);
    EXPECT_EQ(qwertycoin::qms::CRYPTO_PROFILE_TRIPLE_RATCHET, fragment.profile);
    EXPECT_TRUE(qwertycoin::qms::verify_envelope_fragment(context, fragment));
    EXPECT_EQ(696u, qwertycoin::qms::encode_fragment(fragment).size());
    const auto segments = qwertycoin::qms::encode_segments(fragment);
    ASSERT_EQ(3u, segments.size());
    EXPECT_EQ(255u, segments[0].size());
    EXPECT_EQ(255u, segments[1].size());
    EXPECT_EQ(207u, segments[2].size());
    std::vector<uint8_t> extra;
    ASSERT_TRUE(qwertycoin::qms::append_carrier_nonces(extra, fragment));
    EXPECT_EQ(726u, extra.size());
    const auto parsed = qwertycoin::qms::extract_carrier_fragments(extra);
    ASSERT_EQ(1u, parsed.size());
    EXPECT_EQ(qwertycoin::qms::encode_fragment(fragment),
              qwertycoin::qms::encode_fragment(parsed.front()));
  }
  std::reverse(fragments.begin(), fragments.end());
  EXPECT_EQ(envelope, qwertycoin::qms::reassemble(fragments));

  auto wrong_context = context;
  wrong_context.root_secret[0] ^= 1;
  EXPECT_FALSE(qwertycoin::qms::verify_envelope_fragment(wrong_context, fragments.front()));
  auto bad = fragments.front();
  bad.data[0] ^= 1;
  EXPECT_FALSE(qwertycoin::qms::verify_envelope_fragment(context, bad));
}

TEST(qms, profile2_store_uses_argon2id_and_authenticated_context)
{
  const qwertycoin::qms::bytes plaintext{'s', 'e', 's', 's', 'i', 'o', 'n'};
  const qwertycoin::qms::bytes context{'m', 'a', 'i', 'n', 'n', 'e', 't'};
  const auto encrypted = qwertycoin::qms::encrypt_store(plaintext, "wallet-password", context);
  EXPECT_NE(plaintext, encrypted);
  EXPECT_EQ(plaintext, qwertycoin::qms::decrypt_store(
    encrypted, "wallet-password", context));
  EXPECT_THROW(qwertycoin::qms::decrypt_store(
    encrypted, "wrong-password", context), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::decrypt_store(
    encrypted, "wallet-password", qwertycoin::qms::bytes{'t', 'e', 's', 't'}),
    std::runtime_error);
  auto tampered = encrypted;
  tampered.back() ^= 1;
  EXPECT_THROW(qwertycoin::qms::decrypt_store(
    tampered, "wallet-password", context), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::encrypt_store(plaintext, "", context), std::runtime_error);

  const auto rewrapped = qwertycoin::qms::rewrap_store(
    encrypted, "wallet-password", "replacement-password");
  EXPECT_NE(encrypted, rewrapped);
  EXPECT_EQ(plaintext, qwertycoin::qms::decrypt_store(
    rewrapped, "replacement-password", context));
  EXPECT_THROW(qwertycoin::qms::decrypt_store(
    rewrapped, "wallet-password", context), std::runtime_error);
  EXPECT_THROW(qwertycoin::qms::rewrap_store(
    encrypted, "wrong-password", "replacement-password"), std::runtime_error);
}

#ifdef QWC_ENABLE_QMS2_CRYPTO
TEST(qms, native_ffi_pqxdh_triple_ratchet_and_envelope_roundtrip)
{
  const auto network = genesis(41);
  auto alice = qwertycoin::qms::crypto_backend::create();
  auto bob = qwertycoin::qms::crypto_backend::create();
  const auto alice_package = alice.prepare_contact_package(network);
  const auto bob_package = bob.prepare_contact_package(network);
  alice = qwertycoin::qms::crypto_backend(alice_package.next_state);
  bob = qwertycoin::qms::crypto_backend(bob_package.next_state);

  const auto alice_import = alice.prepare_import_contact(
    alice_package.invitation_id, bob_package.package, 1700000000);
  const auto bob_import = bob.prepare_import_contact(
    bob_package.invitation_id, alice_package.package, 1700000000);
  EXPECT_EQ(bob_package.fingerprint, alice_import.fingerprint);
  EXPECT_EQ(alice_package.fingerprint, bob_import.fingerprint);
  alice = qwertycoin::qms::crypto_backend(alice_import.next_state);
  bob = qwertycoin::qms::crypto_backend(bob_import.next_state);

  const auto send = alice.prepare_send_text(
    alice_import.contact_id, std::string(4096, 'x'), 1700000001);
  EXPECT_EQ(1, send.ciphertext.message_type); // PQXDH pre-key message.
  qwertycoin::qms::bytes inner;
  inner.reserve(1 + send.ciphertext.data.size());
  inner.push_back(send.ciphertext.message_type);
  inner.insert(inner.end(), send.ciphertext.data.begin(), send.ciphertext.data.end());
  const auto context = alice.transport_context(alice_import.contact_id, true);
  const auto bob_context = bob.transport_context(bob_import.contact_id, false);
  EXPECT_EQ(context.genesis, bob_context.genesis);
  EXPECT_EQ(context.invitation_id, bob_context.invitation_id);
  EXPECT_EQ(context.session_id, bob_context.session_id);
  EXPECT_EQ(context.root_secret, bob_context.root_secret);
  EXPECT_EQ(context.direction, bob_context.direction);
  const auto envelope = qwertycoin::qms::seal_outer_envelope(context, send.message_id, inner);
  EXPECT_EQ(7200u, envelope.size());
  const auto opened = qwertycoin::qms::open_outer_envelope(context, send.message_id, envelope);
  ASSERT_GT(opened.size(), 1u);
  qwertycoin::qms::ratchet_ciphertext received_ciphertext;
  received_ciphertext.message_type = opened.front();
  received_ciphertext.data.assign(opened.begin() + 1, opened.end());
  const auto receive = bob.prepare_receive_text(bob_import.contact_id, received_ciphertext);
  EXPECT_EQ(std::string(4096, 'x'), receive.text);
  EXPECT_EQ(send.message_id, receive.message_id);

  alice = qwertycoin::qms::crypto_backend(send.next_state);
  bob = qwertycoin::qms::crypto_backend(receive.next_state);
  const auto reply = bob.prepare_send_text(bob_import.contact_id, "reply", 1700000002);
  EXPECT_EQ(2, reply.ciphertext.message_type); // Ongoing Triple Ratchet message.
  const auto reply_receive = alice.prepare_receive_text(
    alice_import.contact_id, reply.ciphertext);
  EXPECT_EQ("reply", reply_receive.text);
}
#endif
