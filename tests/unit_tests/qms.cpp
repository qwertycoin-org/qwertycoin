#include <gtest/gtest.h>
#include <sodium.h>

#include "qms/protocol.h"
#include "qms/secure_store.h"
#include "qms/transport_policy.h"
#include "string_tools.h"
#ifdef QWC_ENABLE_QMS2_CRYPTO
#include "qms/crypto_backend.h"
#include "qms/wallet_state.h"
#endif

TEST(qms, strict_native_transport_accepts_only_proxy_and_v3_onion)
{
  const std::string onion(56, 'a');
  std::string reason;
  EXPECT_TRUE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050", "http://" + onion + ".onion:18081", &reason));
  EXPECT_TRUE(reason.empty());
  EXPECT_TRUE(qwertycoin::qms::strict_native_transport_ready(
    "localhost:9050", "HTTPS://" + onion + ".ONION/rpc", &reason));

  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "", "http://" + onion + ".onion:18081", &reason));
  EXPECT_NE(std::string::npos, reason.find("proxy"));
  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050", "https://node.example.org", &reason));
  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050", "http://short.onion:18081", &reason));
  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050", "http://" + std::string(55, 'a') + "1.onion:18081", &reason));
  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050", "http://" + onion + ".onion:0", &reason));
  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050", "ftp://" + onion + ".onion", &reason));
  EXPECT_FALSE(qwertycoin::qms::strict_native_transport_ready(
    "127.0.0.1:9050\n", "http://" + onion + ".onion", &reason));
}
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
  std::string sha256_hex(const qwertycoin::qms::bytes &value)
  {
    qwertycoin::qms::bytes digest(crypto_hash_sha256_BYTES);
    crypto_hash_sha256(digest.data(), value.data(), value.size());
    return qwertycoin::qms::hex(digest);
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

TEST(qms, profile2_browser_transport_vector)
{
  qwertycoin::qms::envelope_context context;
  for (size_t i = 0; i != context.genesis.size(); ++i) context.genesis[i] = 1 + i;
  for (size_t i = 0; i != context.invitation_id.size(); ++i) context.invitation_id[i] = 33 + i;
  for (size_t i = 0; i != context.session_id.size(); ++i) context.session_id[i] = 49 + i;
  for (size_t i = 0; i != context.root_secret.size(); ++i) context.root_secret[i] = 65 + i;
  context.direction = 1;
  const auto id = message_id(17);
  qwertycoin::qms::bytes envelope(1200);
  for (size_t i = 0; i != envelope.size(); ++i) envelope[i] = uint8_t(97 + i);
  const auto fragments = qwertycoin::qms::fragment_envelope(context, id, envelope);
  ASSERT_EQ(2u, fragments.size());
  EXPECT_EQ("d6d84ae3dab889b69d24f3cbf8ad1dab",
    qwertycoin::qms::hex(qwertycoin::qms::bytes(
      fragments[0].discovery_hint.begin(), fragments[0].discovery_hint.end())));
  EXPECT_EQ("caea82ceba3fbdfea07ea3509793de21",
    qwertycoin::qms::hex(qwertycoin::qms::bytes(
      fragments[0].mac.begin(), fragments[0].mac.end())));
  EXPECT_EQ("f630dacd23e94ded3c9159be1e43b2ab50151f447a024044e8f59d289c4fb3d6",
    sha256_hex(qwertycoin::qms::encode_fragment(fragments[0])));
  EXPECT_EQ("4ee38c6ea74047b292516bf8f6760c1dc9ab5e3cd18aedbf7f387c503fcf6b7e",
    sha256_hex(qwertycoin::qms::encode_fragment(fragments[1])));
  qwertycoin::qms::bytes extra;
  ASSERT_TRUE(qwertycoin::qms::append_carrier_nonces(extra, fragments[0]));
  EXPECT_EQ(726u, extra.size());
  EXPECT_EQ("2db1d2f44112122f360cd42c82e340cffa5b6630a872d7bdc994db3a64547a9f",
    sha256_hex(extra));
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

TEST(qms, parser_mutation_smoke_is_fail_closed_and_canonical)
{
  const auto network = genesis(14);
  const auto id = message_id(15);
  const auto alice = qwertycoin::qms::generate_identity();
  const auto bob = qwertycoin::qms::generate_identity();
  const auto bob_invite = qwertycoin::qms::create_invitation(bob, network);
  const auto ciphertext = qwertycoin::qms::seal_text(
    alice, bob_invite, network, id, std::string(700, 'm'));
  const auto fragment = qwertycoin::qms::fragment_ciphertext(
    bob_invite, network, id, ciphertext).front();
  const auto canonical = qwertycoin::qms::encode_fragment(fragment);

  // A deterministic bounded mutation corpus complements the standalone fuzz
  // target and is always exercised by the normal/ASan unit-test jobs.
  uint32_t state = 0x514d5332u;
  for (size_t iteration = 0; iteration != 4096; ++iteration)
  {
    auto mutated = canonical;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    const size_t edits = 1 + (state % 4);
    for (size_t edit = 0; edit != edits; ++edit)
    {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      mutated[state % mutated.size()] ^= uint8_t(1u << (state % 8));
    }
    if ((iteration % 17) == 0 && !mutated.empty())
      mutated.resize(state % mutated.size());
    else if ((iteration % 31) == 0)
      mutated.push_back(uint8_t(state));

    try
    {
      const auto decoded = qwertycoin::qms::decode_fragment(mutated);
      EXPECT_EQ(mutated, qwertycoin::qms::encode_fragment(decoded));
    }
    catch (const std::exception &) {}

    try
    {
      (void)qwertycoin::qms::extract_carrier_fragments(mutated);
    }
    catch (const std::exception &) {}

    try
    {
      (void)qwertycoin::qms::decode_invitation(mutated);
    }
    catch (const std::exception &) {}
  }
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

TEST(qms, native_outer_secret_rotation_repeats_offer_and_keeps_one_grace_secret)
{
  const auto network = genesis(57);
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
  alice = qwertycoin::qms::crypto_backend(alice_import.next_state);
  bob = qwertycoin::qms::crypto_backend(bob_import.next_state);

  const auto initial = alice.transport_context(alice_import.contact_id, false);
  for (uint64_t i = 0; i != 16; ++i)
  {
    const auto sent = alice.prepare_send_text(
      alice_import.contact_id, "advance", 1700000100 + i);
    alice = qwertycoin::qms::crypto_backend(sent.next_state);
    const auto received = bob.prepare_receive_text(bob_import.contact_id, sent.ciphertext);
    bob = qwertycoin::qms::crypto_backend(received.next_state);
  }

  const auto lost = alice.prepare_send_text(
    alice_import.contact_id, "lost-offer", 1700000200);
  alice = qwertycoin::qms::crypto_backend(lost.next_state);
  EXPECT_EQ(2u, alice.transport_contexts(alice_import.contact_id, false).size());
  const auto repeated = alice.prepare_send_text(
    alice_import.contact_id, "repeated-offer", 1700000201);
  alice = qwertycoin::qms::crypto_backend(repeated.next_state);
  const auto received = bob.prepare_receive_text(bob_import.contact_id, repeated.ciphertext);
  bob = qwertycoin::qms::crypto_backend(received.next_state);
  const auto new_context = bob.transport_context(bob_import.contact_id, true);
  EXPECT_NE(initial.root_secret, new_context.root_secret);

  const auto ack = bob.prepare_send_text(bob_import.contact_id, "ack", 1700000202);
  bob = qwertycoin::qms::crypto_backend(ack.next_state);
  const auto opened = alice.prepare_receive_text(alice_import.contact_id, ack.ciphertext);
  alice = qwertycoin::qms::crypto_backend(opened.next_state);
  const auto contexts = alice.transport_contexts(alice_import.contact_id, false);
  ASSERT_EQ(2u, contexts.size());
  EXPECT_TRUE(std::any_of(contexts.begin(), contexts.end(), [&](const auto &context) {
    return context.root_secret == initial.root_secret;
  }));
  EXPECT_TRUE(std::any_of(contexts.begin(), contexts.end(), [&](const auto &context) {
    return context.root_secret == new_context.root_secret;
  }));
}

TEST(qms, shared_wallet_state_is_restart_safe_and_idempotent)
{
  const auto network = genesis(61);
  qwertycoin::qms::wallet_state alice("", network);
  qwertycoin::qms::wallet_state bob("", network);
  std::string alice_package_raw;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(
    alice.own_invitation_hex(), alice_package_raw));
  std::string bob_package_raw;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(
    bob.own_invitation_hex(), bob_package_raw));
  const qwertycoin::qms::bytes bob_package(
    bob_package_raw.begin(), bob_package_raw.end());
  const std::string fingerprint = alice.import_contact("Bob", bob_package, 1700000100);
  const qwertycoin::qms::bytes alice_package(
    alice_package_raw.begin(), alice_package_raw.end());
  bob.import_contact("Alice", alice_package, 1700000100);
  EXPECT_FALSE(bob.history_enabled());
  bob.set_history_enabled(true);
  EXPECT_TRUE(bob.history_enabled());
  EXPECT_EQ(fingerprint, alice.import_contact("Bob", bob_package, 1700000101));
  ASSERT_EQ(1u, alice.contacts().size());

  const auto plan = alice.prepare_send(fingerprint, std::string(4096, 'q'), 1700000102);
  EXPECT_EQ(12u, plan.carrier_extras.size());
  EXPECT_EQ(7200u, plan.envelope_size);
  for (size_t i = 0; i != 6; ++i)
  {
    const auto received = bob.ingest_carrier(plan.carrier_extras[i],
      100 + i, "block", "tx", 1700000103 + i);
    EXPECT_TRUE(received.accepted_fragment);
    EXPECT_FALSE(received.completed);
  }
  qwertycoin::qms::wallet_state resumed_bob(bob.serialize(), network);
  qwertycoin::qms::wallet_receive_result received;
  for (size_t i = 6; i != plan.carrier_extras.size(); ++i)
    received = resumed_bob.ingest_carrier(plan.carrier_extras[i],
      100 + i, "block", "tx", 1700000103 + i);
  EXPECT_TRUE(received.completed);
  EXPECT_EQ(std::string(4096, 'q'), received.text);
  const std::string with_history = resumed_bob.serialize();
  EXPECT_NE(std::string::npos, with_history.find(std::string(4096, 'q')));
  resumed_bob.clear_history();
  EXPECT_EQ(std::string::npos, resumed_bob.serialize().find(std::string(4096, 'q')));
  const auto duplicate = resumed_bob.ingest_carrier(plan.carrier_extras.back(),
    111, "block", "tx", 1700000200);
  EXPECT_TRUE(duplicate.accepted_fragment);
  EXPECT_FALSE(duplicate.completed);
  alice.accept_prepared(plan, "encrypted-pending-journal", 12, 12345, 1700000103);
  ASSERT_TRUE(alice.has_prepared());
  const std::string serialized = alice.serialize();

  qwertycoin::qms::wallet_state restarted(serialized, network);
  EXPECT_TRUE(restarted.has_prepared());
  EXPECT_EQ("encrypted-pending-journal", restarted.prepared_journal());
  EXPECT_EQ(fingerprint, restarted.prepared_contact_fingerprint());
  restarted.update_prepared_journal("one-carrier-remains", 1, 1000, 1700000104);
  EXPECT_EQ("one-carrier-remains", restarted.prepared_journal());
  restarted.clear_prepared();
  EXPECT_FALSE(restarted.has_prepared());
}
#endif
