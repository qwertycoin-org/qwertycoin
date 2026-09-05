// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "epose/chain_state.h"
#include "epose/attestation_pool.h"
#include "epose/service_epoch.h"
#include "epose/service_node.h"
#include "epose/service_node_config.h"
#include "epose/service_registry.h"
#include "cryptonote_config.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "string_tools.h"

#include <boost/filesystem.hpp>

namespace
{
  constexpr size_t MONERO_TX_EXTRA_NONCE_MAX_COUNT = 255;

  struct test_service_identity
  {
    qwertycoin::epose::service_node_identity identity;
    crypto::secret_key service_secret;
  };

  struct test_reward_account
  {
    crypto::secret_key spend_secret;
    crypto::secret_key view_secret;
    cryptonote::account_public_address address{};
  };

  test_reward_account make_reward_account()
  {
    test_reward_account account{};
    crypto::generate_keys(account.address.m_spend_public_key, account.spend_secret);
    crypto::generate_keys(account.address.m_view_public_key, account.view_secret);
    return account;
  }

  cryptonote::account_public_address make_reward_address()
  {
    return make_reward_account().address;
  }

  std::vector<uint64_t> decomposed_amounts(uint64_t amount)
  {
    std::vector<uint64_t> amounts;
    cryptonote::decompose_amount_into_digits(amount, 0,
      [&amounts](uint64_t chunk) { amounts.push_back(chunk); },
      [&amounts](uint64_t dust) { amounts.push_back(dust); });
    return amounts;
  }

  qwertycoin::epose::local_service_node_config make_local_service_node_config()
  {
    qwertycoin::epose::local_service_node_config config{};
    config.enabled = true;
    config.key_loaded = true;
    crypto::generate_keys(config.service_public_key, config.service_secret_key);
    const test_reward_account reward_account = make_reward_account();
    config.reward_address = reward_account.address;
    config.reward_view_secret_key = reward_account.view_secret;
    config.reward_address_string = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, config.reward_address);
    config.advertised_endpoint = "node.example:8196";
    config.endpoint_commitment = qwertycoin::epose::make_endpoint_commitment(config.advertised_endpoint);
    return config;
  }

  test_service_identity make_test_identity_for_endpoint(
      const std::string &endpoint,
      uint64_t registration_epoch = 1,
      uint8_t admission_leading_zero_bits = 4)
  {
    test_service_identity service{};
    crypto::generate_keys(service.identity.service_public_key, service.service_secret);
    const test_reward_account reward_account = make_reward_account();
    service.identity.reward_address = reward_account.address;
    service.identity.reward_view_secret_key = reward_account.view_secret;
    service.identity.endpoint_commitment = qwertycoin::epose::make_endpoint_commitment(endpoint);
    service.identity.registration_epoch = registration_epoch;
    service.identity.expiry_epoch = registration_epoch + 2;
    crypto::hash previous_epoch_hash{};

    for (uint64_t nonce = 0;; ++nonce)
    {
      service.identity.admission_nonce = nonce;
      service.identity.admission_hash = qwertycoin::epose::calculate_admission_hash(service.identity, cryptonote::TESTNET, previous_epoch_hash);
      if (qwertycoin::epose::admission_hash_meets_target(
          service.identity.admission_hash,
          admission_leading_zero_bits))
        break;
    }

    qwertycoin::epose::sign_registration(service.identity, service.service_secret, cryptonote::TESTNET);
    return service;
  }

  test_service_identity make_test_identity(uint64_t registration_epoch = 1)
  {
    static uint64_t next_endpoint_id = 0;
    return make_test_identity_for_endpoint("node-" + std::to_string(++next_endpoint_id) + ".example:8196", registration_epoch);
  }

  test_service_identity make_relay_identity(uint64_t registration_epoch = 1)
  {
    const auto config = make_local_service_node_config();
    test_service_identity service{};
    service.service_secret = config.service_secret_key;

    std::string error;
    if (!qwertycoin::epose::build_service_node_registration_identity(
        config,
        cryptonote::TESTNET,
        registration_epoch,
        crypto::null_hash,
        service.identity,
        error,
        4))
    {
      throw std::runtime_error("failed to build relay test identity: " + error);
    }
    return service;
  }

  qwertycoin::epose::service_node_identity make_identity(uint64_t registration_epoch = 1)
  {
    return make_test_identity(registration_epoch).identity;
  }

  qwertycoin::epose::service_attestation make_attestation(
      const crypto::public_key &subject,
      uint64_t epoch,
      bool service_ok = true)
  {
    crypto::secret_key verifier_secret;
    qwertycoin::epose::service_attestation attestation{};
    crypto::generate_keys(attestation.verifier_public_key, verifier_secret);
    attestation.subject_public_key = subject;
    attestation.epoch = epoch;
    attestation.challenge_hash = crypto::cn_fast_hash("challenge", 9);
    attestation.response_hash = crypto::cn_fast_hash("response", 8);
    attestation.observed_tip_hash = crypto::cn_fast_hash("tip", 3);
    attestation.service_ok = service_ok;
    qwertycoin::epose::sign_attestation(attestation, verifier_secret, cryptonote::TESTNET);
    return attestation;
  }

  qwertycoin::epose::service_attestation make_attestation_from_verifier(
      const crypto::public_key &subject,
      const test_service_identity &verifier,
      uint64_t epoch,
      bool service_ok = true,
      const crypto::hash &previous_epoch_hash = crypto::null_hash)
  {
    qwertycoin::epose::service_attestation attestation{};
    attestation.verifier_public_key = verifier.identity.service_public_key;
    attestation.subject_public_key = subject;
    attestation.epoch = epoch;
    const crypto::hash epoch_seed = qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, epoch, previous_epoch_hash);
    attestation.challenge_hash = qwertycoin::epose::calculate_challenge_hash(
        epoch_seed,
        subject,
        verifier.identity.service_public_key,
        epoch,
        0);
    attestation.observed_tip_hash = crypto::cn_fast_hash("tip", 3);
    attestation.response_hash = qwertycoin::epose::calculate_response_hash(
        attestation.challenge_hash,
        attestation.observed_tip_hash,
        subject,
        verifier.identity.service_public_key,
        epoch);
    attestation.service_ok = service_ok;
    qwertycoin::epose::sign_attestation(attestation, verifier.service_secret, cryptonote::TESTNET);
    return attestation;
  }

  bool contains_public_key(const std::vector<crypto::public_key> &keys, const crypto::public_key &key)
  {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
  }

  std::vector<qwertycoin::epose::service_node_identity> make_identities(size_t count, uint64_t registration_epoch = 1)
  {
    std::vector<qwertycoin::epose::service_node_identity> identities;
    identities.reserve(count);
    for (size_t i = 0; i < count; ++i)
      identities.push_back(make_identity(registration_epoch));
    return identities;
  }

  cryptonote::transaction make_transaction_with_extra_nonce(const cryptonote::blobdata &nonce)
  {
    cryptonote::transaction tx{};
    if (!cryptonote::add_extra_nonce_to_tx_extra(tx.extra, nonce))
      throw std::runtime_error("failed to add tx extra nonce");
    return tx;
  }

  cryptonote::transaction make_transaction_with_extra_nonces(const std::vector<cryptonote::blobdata> &nonces)
  {
    cryptonote::transaction tx{};
    for (const cryptonote::blobdata &nonce : nonces)
    {
      if (!cryptonote::add_extra_nonce_to_tx_extra(tx.extra, nonce))
        throw std::runtime_error("failed to add tx extra nonce");
    }
    return tx;
  }
}

TEST(epose, epoch_boundaries_are_deterministic)
{
  EXPECT_EQ(0u, qwertycoin::epose::epoch_for_height(0));
  EXPECT_EQ(0u, qwertycoin::epose::epoch_for_height(719));
  EXPECT_EQ(1u, qwertycoin::epose::epoch_for_height(720));
  EXPECT_EQ(720u, qwertycoin::epose::epoch_start_height(1));
  EXPECT_EQ(1439u, qwertycoin::epose::epoch_end_height(1));
  EXPECT_EQ(660u, qwertycoin::epose::epoch_seed_height(1));
  EXPECT_EQ(0u, qwertycoin::epose::reward_source_epoch_for_height(719));
  EXPECT_EQ(0u, qwertycoin::epose::reward_source_epoch_for_height(720));
  EXPECT_EQ(1u, qwertycoin::epose::reward_source_epoch_for_height(1440));
}

TEST(qwc_epose, default_ports_keep_historical_qwertycoin_values)
{
  EXPECT_EQ(8196, config::P2P_DEFAULT_PORT);
  EXPECT_EQ(8197, config::RPC_DEFAULT_PORT);
  EXPECT_EQ(8199, config::ZMQ_RPC_DEFAULT_PORT);
  EXPECT_EQ(8196, config::testnet::P2P_DEFAULT_PORT);
  EXPECT_EQ(8197, config::testnet::RPC_DEFAULT_PORT);
  EXPECT_EQ(8199, config::testnet::ZMQ_RPC_DEFAULT_PORT);
  EXPECT_EQ(8196, config::stagenet::P2P_DEFAULT_PORT);
  EXPECT_EQ(8197, config::stagenet::RPC_DEFAULT_PORT);
  EXPECT_EQ(8199, config::stagenet::ZMQ_RPC_DEFAULT_PORT);
}

TEST(qwc_epose, relaunch_supply_uses_approved_tokenomics)
{
  EXPECT_EQ(UINT64_C(18446744073709551), MONEY_SUPPLY);
  EXPECT_EQ(8, CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  EXPECT_EQ(UINT64_C(100000000), COIN);
  EXPECT_EQ(UINT64_C(30000000), FINAL_SUBSIDY_PER_MINUTE);
  EXPECT_EQ(UINT64_C(200000), FEE_PER_KB);
}

TEST(qwc_epose, wallet_units_support_qwertycoin_decimal_point)
{
  cryptonote::set_default_decimal_point(CRYPTONOTE_DISPLAY_DECIMAL_POINT);
  EXPECT_EQ("qwertycoin", cryptonote::get_unit());
  EXPECT_EQ("milliqwertycoin", cryptonote::get_unit(CRYPTONOTE_DISPLAY_DECIMAL_POINT - 3));
  EXPECT_EQ("microqwertycoin", cryptonote::get_unit(CRYPTONOTE_DISPLAY_DECIMAL_POINT - 6));
  EXPECT_EQ("qwc-12dp", cryptonote::get_unit(12));
  EXPECT_EQ("qwc-9dp", cryptonote::get_unit(9));
  EXPECT_EQ("qwc-6dp", cryptonote::get_unit(6));
  EXPECT_EQ("qwc-3dp", cryptonote::get_unit(3));
  EXPECT_EQ("atomic-qwc", cryptonote::get_unit(0));

  cryptonote::set_default_decimal_point(CRYPTONOTE_DISPLAY_DECIMAL_POINT - 3);
  EXPECT_EQ("milliqwertycoin", cryptonote::get_unit());
  cryptonote::set_default_decimal_point(CRYPTONOTE_DISPLAY_DECIMAL_POINT);
}

TEST(epose, registration_signature_binds_identity_fields)
{
  auto identity = make_identity();
  EXPECT_TRUE(qwertycoin::epose::verify_registration_signature(identity, cryptonote::TESTNET));

  identity.expiry_epoch += 1;
  EXPECT_FALSE(qwertycoin::epose::verify_registration_signature(identity, cryptonote::TESTNET));
}

TEST(epose, identity_serialization_is_bounded_and_roundtrips)
{
  const auto identity = make_identity();
  const std::string blob = qwertycoin::epose::serialize_identity(identity);
  EXPECT_EQ(qwertycoin::epose::EPOSE_IDENTITY_BLOB_SIZE, blob.size());

  qwertycoin::epose::service_node_identity parsed{};
  ASSERT_TRUE(qwertycoin::epose::parse_identity(blob, parsed));
  EXPECT_EQ(identity.service_public_key, parsed.service_public_key);
  EXPECT_EQ(identity.registration_epoch, parsed.registration_epoch);
  EXPECT_TRUE(qwertycoin::epose::verify_registration_signature(parsed, cryptonote::TESTNET));

  EXPECT_FALSE(qwertycoin::epose::parse_identity(blob.substr(0, blob.size() - 1), parsed));
}

TEST(epose, service_node_key_file_is_created_and_reused)
{
  const boost::filesystem::path dir =
      boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("qwc-epose-key-%%%%-%%%%");
  ASSERT_TRUE(boost::filesystem::create_directories(dir));
  const boost::filesystem::path key_path = dir / "service-node.key";

  crypto::public_key first_public_key{};
  crypto::secret_key first_secret_key{};
  bool created = false;
  std::string error;
  ASSERT_TRUE(qwertycoin::epose::load_or_create_service_node_key(key_path.string(), first_public_key, first_secret_key, created, error)) << error;
  EXPECT_TRUE(created);
  EXPECT_TRUE(boost::filesystem::exists(key_path));

  crypto::public_key second_public_key{};
  crypto::secret_key second_secret_key{};
  created = true;
  ASSERT_TRUE(qwertycoin::epose::load_or_create_service_node_key(key_path.string(), second_public_key, second_secret_key, created, error)) << error;
  EXPECT_FALSE(created);
  EXPECT_EQ(first_public_key, second_public_key);

  boost::filesystem::remove_all(dir);
}

TEST(epose, service_node_reward_address_must_be_primary_address_for_network)
{
  const auto address = make_reward_address();
  const std::string encoded = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, address);
  cryptonote::account_public_address parsed{};
  std::string error;
  EXPECT_TRUE(qwertycoin::epose::parse_reward_address(encoded, cryptonote::TESTNET, parsed, error)) << error;
  EXPECT_EQ(address.m_spend_public_key, parsed.m_spend_public_key);
  EXPECT_EQ(address.m_view_public_key, parsed.m_view_public_key);

  EXPECT_FALSE(qwertycoin::epose::parse_reward_address(encoded, cryptonote::MAINNET, parsed, error));
}

TEST(epose, service_node_reward_view_secret_must_match_reward_address)
{
  const test_reward_account reward_account = make_reward_account();
  const test_reward_account other_account = make_reward_account();
  const std::string view_key_hex = epee::string_tools::pod_to_hex(unwrap(unwrap(other_account.view_secret)));
  crypto::secret_key parsed{};
  std::string error;

  EXPECT_FALSE(qwertycoin::epose::parse_reward_view_secret_key(view_key_hex, reward_account.address, parsed, error));
  EXPECT_FALSE(error.empty());
}

TEST(epose, service_node_reward_view_secret_accepts_matching_reward_address)
{
  const test_reward_account reward_account = make_reward_account();
  const std::string view_key_hex = epee::string_tools::pod_to_hex(unwrap(unwrap(reward_account.view_secret)));
  crypto::secret_key parsed{};
  std::string error;

  EXPECT_TRUE(qwertycoin::epose::parse_reward_view_secret_key(view_key_hex, reward_account.address, parsed, error)) << error;
  EXPECT_EQ(reward_account.view_secret, parsed);
}

TEST(epose, registration_signature_requires_matching_reward_view_secret)
{
  auto service = make_test_identity();
  service.identity.reward_view_secret_key = make_reward_account().view_secret;

  EXPECT_FALSE(qwertycoin::epose::verify_registration_signature(service.identity, cryptonote::TESTNET));
}

TEST(qwc_address_branding, mainnet_standard_address_uses_historical_qwc_prefix)
{
  const auto address = make_reward_address();
  const std::string encoded = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, address);

  EXPECT_EQ(0u, encoded.rfind("QWC", 0));
  EXPECT_EQ(0x14820cu, config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);

  cryptonote::address_parse_info parsed{};
  ASSERT_TRUE(cryptonote::get_account_address_from_str(parsed, cryptonote::MAINNET, encoded));
  EXPECT_EQ(address.m_spend_public_key, parsed.address.m_spend_public_key);
  EXPECT_EQ(address.m_view_public_key, parsed.address.m_view_public_key);
}

TEST(qwc_address_branding, network_specific_address_validation_rejects_wrong_network)
{
  const auto address = make_reward_address();
  const std::string mainnet_address = cryptonote::get_account_address_as_str(cryptonote::MAINNET, false, address);
  const std::string testnet_address = cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, address);

  cryptonote::address_parse_info parsed{};
  EXPECT_FALSE(cryptonote::get_account_address_from_str(parsed, cryptonote::TESTNET, mainnet_address));
  EXPECT_FALSE(cryptonote::get_account_address_from_str(parsed, cryptonote::MAINNET, testnet_address));
}

TEST(qwc_address_branding, monero_standard_address_is_not_valid_qwertycoin_address)
{
  const std::string monero_address =
      "42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm";

  cryptonote::address_parse_info parsed{};
  EXPECT_FALSE(cryptonote::get_account_address_from_str(parsed, cryptonote::MAINNET, monero_address));
  EXPECT_FALSE(cryptonote::get_account_address_from_str(parsed, cryptonote::TESTNET, monero_address));
  EXPECT_FALSE(cryptonote::get_account_address_from_str(parsed, cryptonote::STAGENET, monero_address));
}

TEST(epose, service_node_registration_builder_creates_valid_tx_extra_payload)
{
  const auto config = make_local_service_node_config();
  crypto::hash previous_epoch_hash = crypto::cn_fast_hash("previous epoch", 14);
  qwertycoin::epose::service_node_identity identity{};
  std::string error;

  ASSERT_TRUE(qwertycoin::epose::build_service_node_registration_identity(
      config, cryptonote::TESTNET, 7, previous_epoch_hash, identity, error, 4, 10000)) << error;

  EXPECT_EQ(config.service_public_key, identity.service_public_key);
  EXPECT_EQ(config.reward_address.m_spend_public_key, identity.reward_address.m_spend_public_key);
  EXPECT_EQ(config.endpoint_commitment, identity.endpoint_commitment);
  EXPECT_EQ(7u, identity.registration_epoch);
  EXPECT_EQ(7u + qwertycoin::epose::EPOSE_REGISTRATION_TTL_EPOCHS, identity.expiry_epoch);
  EXPECT_TRUE(qwertycoin::epose::verify_registration_signature(identity, cryptonote::TESTNET));
  EXPECT_TRUE(qwertycoin::epose::verify_admission_proof(identity, cryptonote::TESTNET, previous_epoch_hash, 4));

  const std::string extra_nonce = qwertycoin::epose::make_registration_tx_extra_nonce(identity);
  EXPECT_LE(extra_nonce.size(), MONERO_TX_EXTRA_NONCE_MAX_COUNT);
  qwertycoin::epose::service_node_identity parsed{};
  ASSERT_TRUE(qwertycoin::epose::parse_registration_tx_extra_nonce(extra_nonce, parsed));
  EXPECT_EQ(identity.service_public_key, parsed.service_public_key);
}

TEST(epose, service_node_registration_builder_requires_ready_local_config)
{
  qwertycoin::epose::local_service_node_config config{};
  qwertycoin::epose::service_node_identity identity{};
  std::string error;

  EXPECT_FALSE(qwertycoin::epose::build_service_node_registration_identity(
      config, cryptonote::TESTNET, 1, crypto::null_hash, identity, error, 4, 10000));

  config = make_local_service_node_config();
  config.advertised_endpoint.clear();
  EXPECT_FALSE(qwertycoin::epose::build_service_node_registration_identity(
      config, cryptonote::TESTNET, 1, crypto::null_hash, identity, error, 4, 10000));
}

TEST(epose, registration_tx_extra_nonce_fits_monero_limit)
{
  const auto identity = make_identity();
  const std::string extra_nonce = qwertycoin::epose::make_registration_tx_extra_nonce(identity);
  EXPECT_EQ(qwertycoin::epose::EPOSE_IDENTITY_BLOB_SIZE + 1, extra_nonce.size());
  EXPECT_LE(extra_nonce.size(), MONERO_TX_EXTRA_NONCE_MAX_COUNT);

  qwertycoin::epose::service_node_identity parsed{};
  ASSERT_TRUE(qwertycoin::epose::parse_registration_tx_extra_nonce(extra_nonce, parsed));
  EXPECT_TRUE(qwertycoin::epose::verify_registration_signature(parsed, cryptonote::TESTNET));

  std::string wrong_type = extra_nonce;
  wrong_type[0] = qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_ATTESTATION;
  EXPECT_FALSE(qwertycoin::epose::parse_registration_tx_extra_nonce(wrong_type, parsed));
}

TEST(epose, extracts_registration_from_monero_tx_extra)
{
  const auto identity = make_identity();
  cryptonote::blobdata nonce = qwertycoin::epose::make_registration_tx_extra_nonce(identity);
  std::vector<uint8_t> tx_extra;

  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, nonce));

  std::vector<qwertycoin::epose::service_node_identity> registrations;
  ASSERT_TRUE(qwertycoin::epose::extract_registrations_from_tx_extra(tx_extra, registrations));
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(identity.service_public_key, registrations.front().service_public_key);
  EXPECT_TRUE(qwertycoin::epose::verify_registration_signature(registrations.front(), cryptonote::TESTNET));
}

TEST(epose, extracts_registration_after_miner_reserved_extra_nonce)
{
  const auto identity = make_identity();
  const cryptonote::blobdata miner_reserved_nonce(8, 0);
  const cryptonote::transaction tx = make_transaction_with_extra_nonces({
      miner_reserved_nonce,
      qwertycoin::epose::make_registration_tx_extra_nonce(identity)});

  std::vector<qwertycoin::epose::service_node_identity> registrations;
  ASSERT_TRUE(qwertycoin::epose::extract_registrations_from_tx_extra(tx.extra, registrations));
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(identity.service_public_key, registrations.front().service_public_key);
}

TEST(epose, ignores_non_epose_tx_extra_nonce)
{
  cryptonote::blobdata payment_id_nonce(9, 0);
  payment_id_nonce[0] = TX_EXTRA_NONCE_ENCRYPTED_PAYMENT_ID;
  std::vector<uint8_t> tx_extra;

  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, payment_id_nonce));

  std::vector<qwertycoin::epose::service_node_identity> registrations;
  std::vector<qwertycoin::epose::service_attestation> attestations;
  EXPECT_TRUE(qwertycoin::epose::extract_registrations_from_tx_extra(tx_extra, registrations));
  EXPECT_TRUE(qwertycoin::epose::extract_attestations_from_tx_extra(tx_extra, attestations));
  EXPECT_TRUE(registrations.empty());
  EXPECT_TRUE(attestations.empty());
}

TEST(epose, rejects_malformed_epose_tx_extra_nonce)
{
  cryptonote::blobdata malformed(2, 0);
  malformed[0] = qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_REGISTRATION;
  std::vector<uint8_t> tx_extra;

  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, malformed));

  std::vector<qwertycoin::epose::service_node_identity> registrations;
  EXPECT_FALSE(qwertycoin::epose::extract_registrations_from_tx_extra(tx_extra, registrations));
  EXPECT_TRUE(registrations.empty());
}

TEST(epose, rejects_truncated_attestation_tx_extra_nonce)
{
  cryptonote::blobdata malformed(2, 0);
  malformed[0] = qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_ATTESTATION;
  std::vector<uint8_t> tx_extra;

  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, malformed));

  std::vector<qwertycoin::epose::service_attestation> attestations;
  EXPECT_FALSE(qwertycoin::epose::extract_attestations_from_tx_extra(tx_extra, attestations));
  EXPECT_TRUE(attestations.empty());
}

TEST(epose, malformed_epose_tx_extra_batch_rolls_back_after_valid_registration)
{
  const auto identity = make_identity();
  const cryptonote::transaction valid_registration =
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(identity));

  cryptonote::blobdata malformed(2, 0);
  malformed[0] = qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_REGISTRATION;
  const cryptonote::transaction bad_registration = make_transaction_with_extra_nonce(malformed);

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions({valid_registration, bad_registration}, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, admission_proof_is_epoch_and_network_bound)
{
  auto identity = make_identity();
  crypto::hash previous_epoch_hash{};
  EXPECT_TRUE(qwertycoin::epose::verify_admission_proof(identity, cryptonote::TESTNET, previous_epoch_hash, 4));
  EXPECT_FALSE(qwertycoin::epose::verify_admission_proof(identity, cryptonote::MAINNET, previous_epoch_hash, 4));

  const crypto::hash alternate_previous_epoch_hash = crypto::cn_fast_hash("alternate previous epoch", 24);
  EXPECT_FALSE(qwertycoin::epose::verify_admission_proof(identity, cryptonote::TESTNET, alternate_previous_epoch_hash, 4));
}

TEST(epose, registry_rejects_registration_replayed_with_wrong_epoch_seed)
{
  const auto identity = make_identity();
  const crypto::hash wrong_previous_epoch_hash = crypto::cn_fast_hash("wrong previous epoch", 20);
  qwertycoin::epose::service_registry_state registry;
  qwertycoin::epose::registration_status status{};

  EXPECT_FALSE(registry.apply_registration(identity, cryptonote::TESTNET, wrong_previous_epoch_hash, 4, &status));
  EXPECT_EQ(qwertycoin::epose::registration_status::invalid_admission_proof, status);
  EXPECT_EQ(0u, registry.size());
}

TEST(epose, registry_rejects_registration_with_tampered_signature)
{
  auto identity = make_identity();
  std::memset(&identity.signature, 0, sizeof(identity.signature));

  qwertycoin::epose::service_registry_state registry;
  qwertycoin::epose::registration_status status{};
  crypto::hash previous_epoch_hash{};

  EXPECT_FALSE(registry.apply_registration(identity, cryptonote::TESTNET, previous_epoch_hash, 4, &status));
  EXPECT_EQ(qwertycoin::epose::registration_status::invalid_signature, status);
  EXPECT_EQ(0u, registry.size());
}

TEST(epose, required_attestations_use_two_thirds_quorum)
{
  const uint64_t expected[] = {
      0, 1, 2, 2, 3, 4, 4, 5, 6, 6,
      7, 8, 8, 9, 10, 10, 11, 12, 12, 13, 14};

  for (size_t committee_size = 0; committee_size < sizeof(expected) / sizeof(expected[0]); ++committee_size)
  {
    EXPECT_EQ(expected[committee_size],
        qwertycoin::epose::required_attestations_for_committee_size(committee_size))
        << "committee_size=" << committee_size;
  }
}

TEST(epose, qualification_counts_unique_valid_attestations)
{
  const auto subject = make_test_identity();
  const auto verifier_a = make_test_identity();
  const auto verifier_b = make_test_identity();
  std::vector<qwertycoin::epose::service_node_identity> nodes{
      subject.identity,
      verifier_a.identity,
      verifier_b.identity};
  std::vector<qwertycoin::epose::service_attestation> attestations{
      make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1),
      make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1)};

  auto qualified = qwertycoin::epose::qualified_nodes(nodes, attestations, 1, cryptonote::TESTNET);
  ASSERT_EQ(1u, qualified.size());
  EXPECT_EQ(subject.identity.service_public_key, qualified.front());
}

TEST(epose, attestation_serialization_is_bounded_and_roundtrips)
{
  const auto identity = make_identity();
  const auto attestation = make_attestation(identity.service_public_key, 1);
  const std::string blob = qwertycoin::epose::serialize_attestation(attestation);
  EXPECT_EQ(qwertycoin::epose::EPOSE_ATTESTATION_BLOB_SIZE, blob.size());

  qwertycoin::epose::service_attestation parsed{};
  ASSERT_TRUE(qwertycoin::epose::parse_attestation(blob, parsed));
  EXPECT_EQ(attestation.verifier_public_key, parsed.verifier_public_key);
  EXPECT_EQ(attestation.subject_public_key, parsed.subject_public_key);
  EXPECT_EQ(attestation.epoch, parsed.epoch);
  EXPECT_TRUE(qwertycoin::epose::verify_attestation_signature(parsed, cryptonote::TESTNET));

  std::string invalid_bool_blob = blob;
  invalid_bool_blob[1 + 32 + 32 + 8 + 32 + 32 + 32] = 2;
  EXPECT_FALSE(qwertycoin::epose::parse_attestation(invalid_bool_blob, parsed));
}

TEST(epose, attestation_tx_extra_nonce_fits_monero_limit)
{
  const auto identity = make_identity();
  const auto attestation = make_attestation(identity.service_public_key, 1);
  const std::string extra_nonce = qwertycoin::epose::make_attestation_tx_extra_nonce(attestation);
  EXPECT_EQ(qwertycoin::epose::EPOSE_ATTESTATION_BLOB_SIZE + 1, extra_nonce.size());
  EXPECT_LE(extra_nonce.size(), MONERO_TX_EXTRA_NONCE_MAX_COUNT);

  qwertycoin::epose::service_attestation parsed{};
  ASSERT_TRUE(qwertycoin::epose::parse_attestation_tx_extra_nonce(extra_nonce, parsed));
  EXPECT_TRUE(qwertycoin::epose::verify_attestation_signature(parsed, cryptonote::TESTNET));

  std::string wrong_type = extra_nonce;
  wrong_type[0] = qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_REGISTRATION;
  EXPECT_FALSE(qwertycoin::epose::parse_attestation_tx_extra_nonce(wrong_type, parsed));
}

TEST(epose, extracts_attestation_from_monero_tx_extra)
{
  const auto identity = make_identity();
  const auto attestation = make_attestation(identity.service_public_key, 1);
  cryptonote::blobdata nonce = qwertycoin::epose::make_attestation_tx_extra_nonce(attestation);
  std::vector<uint8_t> tx_extra;

  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, nonce));

  std::vector<qwertycoin::epose::service_attestation> attestations;
  ASSERT_TRUE(qwertycoin::epose::extract_attestations_from_tx_extra(tx_extra, attestations));
  ASSERT_EQ(1u, attestations.size());
  EXPECT_EQ(attestation.verifier_public_key, attestations.front().verifier_public_key);
  EXPECT_EQ(attestation.subject_public_key, attestations.front().subject_public_key);
  EXPECT_TRUE(qwertycoin::epose::verify_attestation_signature(attestations.front(), cryptonote::TESTNET));
}

TEST(epose, extracts_attestation_after_miner_reserved_extra_nonce)
{
  const auto identity = make_identity();
  const auto attestation = make_attestation(identity.service_public_key, 1);
  const cryptonote::blobdata miner_reserved_nonce(8, 0);
  const cryptonote::transaction tx = make_transaction_with_extra_nonces({
      miner_reserved_nonce,
      qwertycoin::epose::make_attestation_tx_extra_nonce(attestation)});

  std::vector<qwertycoin::epose::service_attestation> attestations;
  ASSERT_TRUE(qwertycoin::epose::extract_attestations_from_tx_extra(tx.extra, attestations));
  ASSERT_EQ(1u, attestations.size());
  EXPECT_EQ(attestation.verifier_public_key, attestations.front().verifier_public_key);
  EXPECT_EQ(attestation.subject_public_key, attestations.front().subject_public_key);
}

TEST(epose, qualification_rejects_duplicate_voter_and_offline_vote)
{
  const auto subject = make_test_identity();
  const auto verifier_a = make_test_identity();
  const auto verifier_b = make_test_identity();
  auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1);
  auto offline = make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1, false);
  std::vector<qwertycoin::epose::service_node_identity> nodes{
      subject.identity,
      verifier_a.identity,
      verifier_b.identity};
  std::vector<qwertycoin::epose::service_attestation> attestations{attestation, attestation, offline};

  auto qualified = qwertycoin::epose::qualified_nodes(nodes, attestations, 1, cryptonote::TESTNET);
  EXPECT_TRUE(qualified.empty());
}

TEST(epose, reward_split_is_bounded_and_deterministic)
{
  const auto split = qwertycoin::epose::split_block_reward(1000, 250, 1000);
  EXPECT_EQ(1125u, split.miner_reward);
  EXPECT_EQ(125u, split.service_reward);
  EXPECT_THROW(qwertycoin::epose::split_block_reward(UINT64_MAX, 1, 1000), std::overflow_error);
  EXPECT_THROW(qwertycoin::epose::split_block_reward(1000, 0, 10001), std::invalid_argument);
}

TEST(epose, miner_tx_can_include_consensus_visible_service_reward_output)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  const auto &service_address = service_account.address;
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1,
      &service_address,
      service_reward));

  const std::vector<uint64_t> service_amounts = decomposed_amounts(service_reward);
  ASSERT_EQ(1u + service_amounts.size(), miner_tx.vout.size());
  for (size_t i = 0; i < service_amounts.size(); ++i)
    EXPECT_EQ(service_amounts[i], miner_tx.vout[1 + i].amount);
  EXPECT_EQ(typeid(cryptonote::txout_to_tagged_key), miner_tx.vout.back().target.type());

  crypto::public_key service_output_key{};
  ASSERT_TRUE(cryptonote::get_output_public_key(miner_tx.vout.back(), service_output_key));
  EXPECT_NE(service_address.m_spend_public_key, service_output_key);
  EXPECT_TRUE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      service_address,
      service_account.view_secret,
      service_reward));
}

TEST(epose, service_rewards_to_same_wallet_use_unique_one_time_output_keys)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  constexpr uint64_t service_reward = 123456789;
  std::vector<crypto::public_key> output_keys;

  for (uint64_t height = 720; height < 730; ++height)
  {
    cryptonote::transaction miner_tx;
    ASSERT_TRUE(cryptonote::construct_miner_tx(
        height,
        CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
        0,
        1,
        0,
        miner_address,
        miner_tx,
        cryptonote::blobdata(),
        1,
        HF_VERSION_QWC_EPOSE_V1,
        &service_account.address,
        service_reward));

    ASSERT_TRUE(qwertycoin::epose::validate_service_reward_output(
        miner_tx,
        service_account.address,
        service_account.view_secret,
        service_reward));

    const std::vector<uint64_t> service_amounts = decomposed_amounts(service_reward);
    ASSERT_EQ(1u + service_amounts.size(), miner_tx.vout.size());
    for (size_t output_index = 1; output_index < miner_tx.vout.size(); ++output_index)
    {
      crypto::public_key service_output_key{};
      ASSERT_TRUE(cryptonote::get_output_public_key(miner_tx.vout[output_index], service_output_key));
      EXPECT_EQ(output_keys.end(), std::find(output_keys.begin(), output_keys.end(), service_output_key));
      output_keys.push_back(service_output_key);
    }
  }

  EXPECT_EQ(10u * decomposed_amounts(service_reward).size(), output_keys.size());
}

TEST(epose, service_reward_validation_rejects_single_non_decomposed_output)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1,
      &service_account.address,
      service_reward));

  ASSERT_GT(miner_tx.vout.size(), 2u);
  miner_tx.vout[1].amount = service_reward;
  miner_tx.vout.erase(miner_tx.vout.begin() + 2, miner_tx.vout.end());
  miner_tx.invalidate_hashes();

  EXPECT_FALSE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      service_account.address,
      service_account.view_secret,
      service_reward));
}

TEST(epose, transparent_service_reward_validation_rejects_missing_output)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  const auto &service_address = service_account.address;
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1));

  EXPECT_FALSE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      service_address,
      service_account.view_secret,
      service_reward));
}

TEST(epose, transparent_service_reward_validation_rejects_wrong_amount)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  const auto &service_address = service_account.address;
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1,
      &service_address,
      service_reward + 1));

  EXPECT_FALSE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      service_address,
      service_account.view_secret,
      service_reward));
}

TEST(epose, transparent_service_reward_validation_rejects_wrong_recipient)
{
  const auto miner_address = make_reward_address();
  const auto expected_service_account = make_reward_account();
  const auto wrong_service_account = make_reward_account();
  const auto &expected_service_address = expected_service_account.address;
  const auto &wrong_service_address = wrong_service_account.address;
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1,
      &wrong_service_address,
      service_reward));

  EXPECT_FALSE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      expected_service_address,
      expected_service_account.view_secret,
      service_reward));
}

TEST(epose, service_reward_validation_rejects_wrong_view_key)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  const auto wrong_service_account = make_reward_account();
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1,
      &service_account.address,
      service_reward));

  EXPECT_FALSE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      service_account.address,
      wrong_service_account.view_secret,
      service_reward));
}

TEST(epose, transparent_service_reward_validation_rejects_duplicate_payee_output)
{
  const auto miner_address = make_reward_address();
  const auto service_account = make_reward_account();
  const auto &service_address = service_account.address;
  constexpr uint64_t service_reward = 123456789;

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      720,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      0,
      miner_address,
      miner_tx,
      cryptonote::blobdata(),
      1,
      HF_VERSION_QWC_EPOSE_V1,
      &service_address,
      service_reward));

  miner_tx.vout.push_back(miner_tx.vout.back());
  miner_tx.invalidate_hashes();

  EXPECT_FALSE(qwertycoin::epose::validate_service_reward_output(
      miner_tx,
      service_address,
      service_account.view_secret,
      service_reward));
}

TEST(epose, payout_selection_rotates_over_qualified_set)
{
  const auto a = make_identity();
  const auto b = make_identity(2);
  std::vector<crypto::public_key> qualified{a.service_public_key, b.service_public_key};
  crypto::hash seed = crypto::cn_fast_hash("seed", 4);
  crypto::public_key selected_0{};
  crypto::public_key selected_1{};

  ASSERT_TRUE(qwertycoin::epose::select_service_payee(qualified, seed, 100, selected_0));
  ASSERT_TRUE(qwertycoin::epose::select_service_payee(qualified, seed, 101, selected_1));
  EXPECT_NE(selected_0, selected_1);
}

TEST(epose, current_epoch_attestations_do_not_change_current_reward_source)
{
  const auto subject = make_test_identity(1);
  const auto verifier_a = make_test_identity(1);
  const auto verifier_b = make_test_identity(1);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);

  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};
  ASSERT_TRUE(state.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));

  const uint64_t current_epoch = 1;
  const uint64_t reward_epoch = qwertycoin::epose::reward_source_epoch_for_height(qwertycoin::epose::epoch_start_height(current_epoch));
  EXPECT_TRUE(state.qualified_service_nodes(reward_epoch).empty());

  std::vector<cryptonote::transaction> attestations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, current_epoch))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, current_epoch)))};
  ASSERT_TRUE(state.apply_transactions(attestations, previous_epoch_hash, nullptr, current_epoch));

  EXPECT_EQ(1u, state.qualified_service_nodes(current_epoch).size());
  EXPECT_TRUE(state.qualified_service_nodes(reward_epoch).empty());
}

TEST(epose, attestation_ordering_does_not_change_next_epoch_reward_source)
{
  const auto subject = make_test_identity(1);
  const auto verifier_a = make_test_identity(1);
  const auto verifier_b = make_test_identity(1);
  crypto::hash previous_epoch_hash{};

  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};
  std::vector<cryptonote::transaction> attestations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1)))};

  qwertycoin::epose::chain_state first(cryptonote::TESTNET, 4);
  ASSERT_TRUE(first.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));
  ASSERT_TRUE(first.apply_transactions(attestations, previous_epoch_hash, nullptr, 1));

  std::reverse(attestations.begin(), attestations.end());
  qwertycoin::epose::chain_state second(cryptonote::TESTNET, 4);
  ASSERT_TRUE(second.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));
  ASSERT_TRUE(second.apply_transactions(attestations, previous_epoch_hash, nullptr, 1));

  const uint64_t reward_epoch = qwertycoin::epose::reward_source_epoch_for_height(qwertycoin::epose::epoch_start_height(2));
  EXPECT_EQ(first.qualified_service_nodes(reward_epoch), second.qualified_service_nodes(reward_epoch));
  EXPECT_EQ(first.state_hash(), second.state_hash());
}

TEST(epose, omitted_current_epoch_attestation_does_not_change_current_reward_source)
{
  const auto subject = make_test_identity(1);
  const auto verifier_a = make_test_identity(1);
  const auto verifier_b = make_test_identity(1);
  crypto::hash previous_epoch_hash{};

  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};
  std::vector<cryptonote::transaction> all_attestations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1)))};
  std::vector<cryptonote::transaction> omitted_attestations{all_attestations.front()};

  qwertycoin::epose::chain_state included(cryptonote::TESTNET, 4);
  ASSERT_TRUE(included.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));
  ASSERT_TRUE(included.apply_transactions(all_attestations, previous_epoch_hash, nullptr, 1));

  qwertycoin::epose::chain_state omitted(cryptonote::TESTNET, 4);
  ASSERT_TRUE(omitted.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));
  ASSERT_TRUE(omitted.apply_transactions(omitted_attestations, previous_epoch_hash, nullptr, 1));

  const uint64_t reward_epoch = qwertycoin::epose::reward_source_epoch_for_height(qwertycoin::epose::epoch_start_height(1));
  EXPECT_TRUE(included.qualified_service_nodes(reward_epoch).empty());
  EXPECT_EQ(included.qualified_service_nodes(reward_epoch), omitted.qualified_service_nodes(reward_epoch));

  EXPECT_EQ(1u, included.qualified_service_nodes(1).size());
  EXPECT_TRUE(omitted.qualified_service_nodes(1).empty());
}

TEST(epose, later_epoch_attestations_do_not_mutate_finalized_qualified_set)
{
  const auto subject = make_test_identity(1);
  const auto verifier_a = make_test_identity(1);
  const auto verifier_b = make_test_identity(1);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);

  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};
  ASSERT_TRUE(state.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));

  std::vector<cryptonote::transaction> epoch_1_attestations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1)))};
  ASSERT_TRUE(state.apply_transactions(epoch_1_attestations, previous_epoch_hash, nullptr, 1));
  const auto finalized_epoch_1_qualified = state.qualified_service_nodes(1);
  ASSERT_EQ(1u, finalized_epoch_1_qualified.size());

  std::vector<cryptonote::transaction> epoch_2_attestations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 2))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 2)))};
  ASSERT_TRUE(state.apply_transactions(epoch_2_attestations, previous_epoch_hash, nullptr, 2));

  EXPECT_EQ(finalized_epoch_1_qualified, state.qualified_service_nodes(1));
  EXPECT_EQ(1u, state.qualified_service_nodes(2).size());
}

TEST(epose, registry_accepts_valid_registration_and_lists_active_nodes)
{
  qwertycoin::epose::service_registry_state registry;
  const auto identity = make_identity();
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::registration_status status{};

  EXPECT_TRUE(registry.apply_registration(identity, cryptonote::TESTNET, previous_epoch_hash, 4, &status));
  EXPECT_EQ(qwertycoin::epose::registration_status::accepted, status);
  EXPECT_EQ(1u, registry.size());

  EXPECT_EQ(1u, registry.active_nodes(identity.registration_epoch).size());
  EXPECT_TRUE(registry.active_nodes(identity.expiry_epoch).empty());
}

TEST(epose, registry_rejects_overlapping_duplicate_identity)
{
  qwertycoin::epose::service_registry_state registry;
  const auto identity = make_identity();
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::registration_status status{};

  ASSERT_TRUE(registry.apply_registration(identity, cryptonote::TESTNET, previous_epoch_hash, 4, &status));
  EXPECT_FALSE(registry.apply_registration(identity, cryptonote::TESTNET, previous_epoch_hash, 4, &status));
  EXPECT_EQ(qwertycoin::epose::registration_status::duplicate_identity, status);
  EXPECT_EQ(1u, registry.size());
}

TEST(epose, registry_rejects_overlapping_duplicate_endpoint)
{
  qwertycoin::epose::service_registry_state registry;
  const auto first = make_test_identity_for_endpoint("same-endpoint.example:8196");
  const auto second = make_test_identity_for_endpoint("same-endpoint.example:8196");
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::registration_status status{};

  ASSERT_TRUE(registry.apply_registration(first.identity, cryptonote::TESTNET, previous_epoch_hash, 4, &status));
  EXPECT_FALSE(registry.apply_registration(second.identity, cryptonote::TESTNET, previous_epoch_hash, 4, &status));
  EXPECT_EQ(qwertycoin::epose::registration_status::duplicate_endpoint, status);
  EXPECT_EQ(1u, registry.size());
}

TEST(epose, registry_rejects_wrong_network_admission)
{
  qwertycoin::epose::service_registry_state registry;
  const auto identity = make_identity();
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::registration_status status{};

  EXPECT_FALSE(registry.apply_registration(identity, cryptonote::MAINNET, previous_epoch_hash, 4, &status));
  EXPECT_EQ(qwertycoin::epose::registration_status::invalid_signature, status);
  EXPECT_EQ(0u, registry.size());
}

TEST(epose, chain_state_applies_registration_from_tx_extra)
{
  const auto identity = make_identity();
  cryptonote::blobdata nonce = qwertycoin::epose::make_registration_tx_extra_nonce(identity);
  std::vector<uint8_t> tx_extra;
  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(tx_extra, nonce));

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::tx_extra_apply_summary summary{};

  ASSERT_TRUE(state.apply_tx_extra(tx_extra, previous_epoch_hash, &summary));
  EXPECT_EQ(1u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(1u, state.registry().size());
  EXPECT_EQ(1u, state.registry().active_nodes(identity.registration_epoch).size());
}

TEST(epose, chain_state_rolls_back_invalid_transaction_extra)
{
  const auto identity = make_identity();
  cryptonote::blobdata registration_nonce = qwertycoin::epose::make_registration_tx_extra_nonce(identity);
  std::vector<uint8_t> registration_extra;
  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(registration_extra, registration_nonce));

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  ASSERT_TRUE(state.apply_tx_extra(registration_extra, previous_epoch_hash));
  const auto before = state.make_snapshot();

  auto bad_attestation = make_attestation(identity.service_public_key, identity.registration_epoch);
  bad_attestation.epoch += 1;
  cryptonote::blobdata bad_attestation_nonce = qwertycoin::epose::make_attestation_tx_extra_nonce(bad_attestation);
  std::vector<uint8_t> bad_extra;
  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(bad_extra, bad_attestation_nonce));

  EXPECT_FALSE(state.apply_tx_extra(bad_extra, previous_epoch_hash));
  EXPECT_EQ(before.registry.size(), state.registry().size());
  EXPECT_EQ(before.attestations.size(), state.attestations().size());
}

TEST(epose, chain_state_rejects_registration_from_wrong_epoch)
{
  const auto identity = make_identity(1);
  cryptonote::blobdata registration_nonce = qwertycoin::epose::make_registration_tx_extra_nonce(identity);
  std::vector<uint8_t> registration_extra;
  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(registration_extra, registration_nonce));

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};

  EXPECT_FALSE(state.apply_tx_extra(registration_extra, previous_epoch_hash, nullptr, 2));
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_TRUE(state.apply_tx_extra(registration_extra, previous_epoch_hash, nullptr, 1));
  EXPECT_EQ(1u, state.registry().size());
}

TEST(epose, chain_state_rejects_attestation_from_wrong_epoch)
{
  const auto subject = make_test_identity(1);
  const auto verifier = make_test_identity(1);
  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  ASSERT_TRUE(state.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));
  const auto before = state.make_snapshot();

  const auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, subject.identity.registration_epoch);
  cryptonote::blobdata attestation_nonce = qwertycoin::epose::make_attestation_tx_extra_nonce(attestation);
  std::vector<uint8_t> attestation_extra;
  ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(attestation_extra, attestation_nonce));

  EXPECT_FALSE(state.apply_tx_extra(attestation_extra, previous_epoch_hash, nullptr, 2));
  EXPECT_EQ(before.registry.size(), state.registry().size());
  EXPECT_EQ(before.attestations.size(), state.attestations().size());
  EXPECT_TRUE(state.apply_tx_extra(attestation_extra, previous_epoch_hash, nullptr, 1));
  EXPECT_EQ(1u, state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_from_unregistered_verifier)
{
  const auto subject = make_test_identity();
  const auto unregistered_verifier = make_test_identity();
  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, unregistered_verifier, subject.identity.registration_epoch)))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_from_unselected_verifier)
{
  const auto subject = make_test_identity();
  std::vector<test_service_identity> verifiers;
  std::vector<qwertycoin::epose::service_node_identity> active_nodes{subject.identity};
  for (size_t i = 0; i < qwertycoin::epose::EPOSE_VERIFIER_COMMITTEE_SIZE + 2; ++i)
  {
    verifiers.push_back(make_test_identity());
    active_nodes.push_back(verifiers.back().identity);
  }

  crypto::hash previous_epoch_hash{};
  const crypto::hash epoch_seed = qwertycoin::epose::calculate_epoch_seed(
      cryptonote::TESTNET,
      subject.identity.registration_epoch,
      previous_epoch_hash);
  const auto assignments = qwertycoin::epose::select_verifiers(
      active_nodes,
      subject.identity.service_public_key,
      cryptonote::TESTNET,
      subject.identity.registration_epoch,
      epoch_seed,
      qwertycoin::epose::EPOSE_VERIFIER_COMMITTEE_SIZE);

  std::vector<crypto::public_key> selected;
  for (const auto &assignment : assignments)
    selected.push_back(assignment.verifier_public_key);

  const test_service_identity *unselected = nullptr;
  for (const auto &verifier : verifiers)
  {
    if (!contains_public_key(selected, verifier.identity.service_public_key))
    {
      unselected = &verifier;
      break;
    }
  }
  ASSERT_NE(nullptr, unselected);

  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity))};
  for (const auto &verifier : verifiers)
    txs.push_back(make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)));
  txs.push_back(make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
      make_attestation_from_verifier(subject.identity.service_public_key, *unselected, subject.identity.registration_epoch))));

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_with_wrong_challenge)
{
  const auto subject = make_test_identity();
  const auto verifier = make_test_identity();
  auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, subject.identity.registration_epoch);
  attestation.challenge_hash = crypto::cn_fast_hash("wrong challenge", 15);
  qwertycoin::epose::sign_attestation(attestation, verifier.service_secret, cryptonote::TESTNET);

  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_with_wrong_response_binding)
{
  const auto subject = make_test_identity();
  const auto verifier = make_test_identity();
  auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, subject.identity.registration_epoch);
  attestation.response_hash = crypto::cn_fast_hash("wrong response", 14);
  qwertycoin::epose::sign_attestation(attestation, verifier.service_secret, cryptonote::TESTNET);

  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_with_tampered_signature)
{
  const auto subject = make_test_identity();
  const auto verifier = make_test_identity();
  auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, subject.identity.registration_epoch);
  std::memset(&attestation.signature, 0, sizeof(attestation.signature));

  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rejects_offline_attestation_and_rolls_back)
{
  const auto subject = make_test_identity();
  const auto verifier = make_test_identity();
  const auto offline_attestation = make_attestation_from_verifier(
      subject.identity.service_public_key,
      verifier,
      subject.identity.registration_epoch,
      false);

  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(offline_attestation))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_replayed_with_wrong_epoch_seed)
{
  const auto subject = make_test_identity();
  const auto verifier = make_test_identity();
  const crypto::hash expected_previous_epoch_hash{};
  const crypto::hash wrong_previous_epoch_hash = crypto::cn_fast_hash("wrong attestation seed", 22);
  const auto replayed_attestation = make_attestation_from_verifier(
      subject.identity.service_public_key,
      verifier,
      subject.identity.registration_epoch,
      true,
      wrong_previous_epoch_hash);

  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  ASSERT_TRUE(state.apply_transactions(registrations, expected_previous_epoch_hash, nullptr, subject.identity.registration_epoch));
  const auto before = state.make_snapshot();

  EXPECT_FALSE(state.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(replayed_attestation)),
      expected_previous_epoch_hash,
      nullptr,
      subject.identity.registration_epoch));
  EXPECT_EQ(before.registry.size(), state.registry().size());
  EXPECT_EQ(before.attestations.size(), state.attestations().size());
}

TEST(epose, chain_state_rejects_attestation_for_inactive_subject)
{
  auto subject = make_test_identity(1);
  subject.identity.expiry_epoch = 2;
  qwertycoin::epose::sign_registration(subject.identity, subject.service_secret, cryptonote::TESTNET);
  const auto verifier = make_test_identity(2);
  const auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, 2);
  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};

  ASSERT_TRUE(state.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      previous_epoch_hash,
      nullptr,
      1));
  ASSERT_TRUE(state.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)),
      previous_epoch_hash,
      nullptr,
      2));
  const auto before = state.make_snapshot();

  EXPECT_FALSE(state.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation)),
      previous_epoch_hash,
      nullptr,
      2));
  EXPECT_EQ(before.registry.size(), state.registry().size());
  EXPECT_EQ(before.attestations.size(), state.attestations().size());
}

TEST(epose, chain_state_qualification_is_reorg_restorable)
{
  const auto subject = make_test_identity();
  const auto verifier_a = make_test_identity();
  const auto verifier_b = make_test_identity();
  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  ASSERT_TRUE(state.apply_transactions(registrations, previous_epoch_hash));
  const auto before_attestations = state.make_snapshot();

  for (const auto &verifier : {verifier_a, verifier_b})
  {
    const auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, subject.identity.registration_epoch);
    cryptonote::blobdata attestation_nonce = qwertycoin::epose::make_attestation_tx_extra_nonce(attestation);
    std::vector<uint8_t> attestation_extra;
    ASSERT_TRUE(cryptonote::add_extra_nonce_to_tx_extra(attestation_extra, attestation_nonce));
    ASSERT_TRUE(state.apply_tx_extra(attestation_extra, previous_epoch_hash));
  }

  EXPECT_EQ(1u, state.qualified_service_nodes(subject.identity.registration_epoch).size());

  state.restore_snapshot(before_attestations);
  EXPECT_TRUE(state.qualified_service_nodes(subject.identity.registration_epoch).empty());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_reorg_keeps_only_winning_branch_registrations)
{
  const auto common = make_test_identity();
  const auto losing_branch = make_test_identity();
  const auto winning_branch = make_test_identity();
  crypto::hash previous_epoch_hash{};

  qwertycoin::epose::chain_state incremental(cryptonote::TESTNET, 4);
  ASSERT_TRUE(incremental.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(common.identity)),
      previous_epoch_hash,
      nullptr,
      common.identity.registration_epoch));
  const auto split_snapshot = incremental.make_snapshot();

  ASSERT_TRUE(incremental.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(losing_branch.identity)),
      previous_epoch_hash,
      nullptr,
      losing_branch.identity.registration_epoch));
  ASSERT_EQ(2u, incremental.registry().registrations().size());

  incremental.restore_snapshot(split_snapshot);
  ASSERT_TRUE(incremental.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(winning_branch.identity)),
      previous_epoch_hash,
      nullptr,
      winning_branch.identity.registration_epoch));

  const std::vector<qwertycoin::epose::service_node_identity> registrations = incremental.registry().registrations();
  ASSERT_EQ(2u, registrations.size());
  EXPECT_TRUE(std::find_if(registrations.begin(), registrations.end(), [&](const qwertycoin::epose::service_node_identity &identity) {
    return identity.service_public_key == common.identity.service_public_key;
  }) != registrations.end());
  EXPECT_TRUE(std::find_if(registrations.begin(), registrations.end(), [&](const qwertycoin::epose::service_node_identity &identity) {
    return identity.service_public_key == winning_branch.identity.service_public_key;
  }) != registrations.end());
  EXPECT_TRUE(std::find_if(registrations.begin(), registrations.end(), [&](const qwertycoin::epose::service_node_identity &identity) {
    return identity.service_public_key == losing_branch.identity.service_public_key;
  }) == registrations.end());

  qwertycoin::epose::chain_state rebuilt(cryptonote::TESTNET, 4);
  std::vector<cryptonote::transaction> winning_chain{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(common.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(winning_branch.identity))};
  ASSERT_TRUE(rebuilt.apply_transactions(winning_chain, previous_epoch_hash, nullptr, common.identity.registration_epoch));
  EXPECT_EQ(rebuilt.state_hash(), incremental.state_hash());
}

TEST(epose, chain_state_reorg_a_b_a_restores_original_qualified_state)
{
  const auto subject = make_test_identity(1);
  const auto verifier_a = make_test_identity(1);
  const auto verifier_b = make_test_identity(1);
  const auto alternate_subject = make_test_identity(1);
  crypto::hash previous_epoch_hash{};

  const std::vector<cryptonote::transaction> base_chain{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};

  const std::vector<cryptonote::transaction> branch_a{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1)))};

  const std::vector<cryptonote::transaction> branch_b{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(alternate_subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1)))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  ASSERT_TRUE(state.apply_transactions(base_chain, previous_epoch_hash, nullptr, 1));
  const auto base_snapshot = state.make_snapshot();

  ASSERT_TRUE(state.apply_transactions(branch_a, previous_epoch_hash, nullptr, 1));
  const crypto::hash branch_a_hash = state.state_hash();
  const auto branch_a_qualified = state.qualified_service_nodes(1);
  ASSERT_EQ(1u, branch_a_qualified.size());
  EXPECT_EQ(subject.identity.service_public_key, branch_a_qualified.front());

  state.restore_snapshot(base_snapshot);
  ASSERT_TRUE(state.apply_transactions(branch_b, previous_epoch_hash, nullptr, 1));
  const crypto::hash branch_b_hash = state.state_hash();
  EXPECT_NE(branch_a_hash, branch_b_hash);
  EXPECT_TRUE(state.qualified_service_nodes(1).empty());

  state.restore_snapshot(base_snapshot);
  ASSERT_TRUE(state.apply_transactions(branch_a, previous_epoch_hash, nullptr, 1));
  EXPECT_EQ(branch_a_hash, state.state_hash());
  EXPECT_EQ(branch_a_qualified, state.qualified_service_nodes(1));
}

TEST(epose, late_attestation_for_finalized_epoch_is_rejected_without_changing_state)
{
  const auto subject = make_test_identity(1);
  const auto verifier_a = make_test_identity(1);
  const auto verifier_b = make_test_identity(1);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);

  std::vector<cryptonote::transaction> registrations{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity))};
  ASSERT_TRUE(state.apply_transactions(registrations, previous_epoch_hash, nullptr, 1));

  ASSERT_TRUE(state.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, 1))),
      previous_epoch_hash,
      nullptr,
      1));

  const auto before_late_attestation = state.make_snapshot();
  const crypto::hash before_hash = state.state_hash();
  ASSERT_TRUE(state.qualified_service_nodes(1).empty());

  EXPECT_FALSE(state.apply_transaction(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, 1))),
      previous_epoch_hash,
      nullptr,
      2));

  EXPECT_EQ(before_late_attestation.registry.size(), state.registry().size());
  EXPECT_EQ(before_late_attestation.attestations.size(), state.attestations().size());
  EXPECT_EQ(before_hash, state.state_hash());
  EXPECT_TRUE(state.qualified_service_nodes(1).empty());
}

TEST(epose, chain_state_prunes_expired_consensus_state_deterministically)
{
  auto expired_subject = make_test_identity(1);
  expired_subject.identity.expiry_epoch = 2;
  qwertycoin::epose::sign_registration(expired_subject.identity, expired_subject.service_secret, cryptonote::TESTNET);
  auto expired_verifier_a = make_test_identity(1);
  expired_verifier_a.identity.expiry_epoch = 2;
  qwertycoin::epose::sign_registration(expired_verifier_a.identity, expired_verifier_a.service_secret, cryptonote::TESTNET);
  auto expired_verifier_b = make_test_identity(1);
  expired_verifier_b.identity.expiry_epoch = 2;
  qwertycoin::epose::sign_registration(expired_verifier_b.identity, expired_verifier_b.service_secret, cryptonote::TESTNET);

  const auto active_subject = make_test_identity(4);
  const auto active_verifier_a = make_test_identity(4);
  const auto active_verifier_b = make_test_identity(4);
  const crypto::hash previous_epoch_hash{};

  std::vector<cryptonote::transaction> expired_txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(expired_subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(expired_verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(expired_verifier_b.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(expired_subject.identity.service_public_key, expired_verifier_a, 1))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(expired_subject.identity.service_public_key, expired_verifier_b, 1)))};
  std::vector<cryptonote::transaction> active_txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(active_subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(active_verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(active_verifier_b.identity))};

  qwertycoin::epose::chain_state incremental(cryptonote::TESTNET, 4);
  ASSERT_TRUE(incremental.apply_transactions(expired_txs, previous_epoch_hash, nullptr, 1));
  ASSERT_TRUE(incremental.apply_transactions(active_txs, previous_epoch_hash, nullptr, 4));

  EXPECT_EQ(3u, incremental.registry().size());
  EXPECT_EQ(0u, incremental.attestations().size());
  EXPECT_EQ(0u, incremental.qualified_service_nodes(1).size());

  qwertycoin::epose::chain_state rebuilt(cryptonote::TESTNET, 4);
  ASSERT_TRUE(rebuilt.apply_transactions(expired_txs, previous_epoch_hash, nullptr, 1));
  ASSERT_TRUE(rebuilt.apply_transactions(active_txs, previous_epoch_hash, nullptr, 4));
  EXPECT_EQ(incremental.state_hash(), rebuilt.state_hash());
}

TEST(epose, chain_state_hash_is_stable_for_same_consensus_state)
{
  const auto subject = make_test_identity();
  const auto verifier_a = make_test_identity();
  const auto verifier_b = make_test_identity();
  const auto attestation_a = make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, subject.identity.registration_epoch);
  const auto attestation_b = make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, subject.identity.registration_epoch);
  crypto::hash previous_epoch_hash{};

  qwertycoin::epose::chain_state first(cryptonote::TESTNET, 4);
  ASSERT_TRUE(first.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(first.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(first.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(first.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation_a)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(first.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation_b)).extra,
      previous_epoch_hash));

  qwertycoin::epose::chain_state second(cryptonote::TESTNET, 4);
  ASSERT_TRUE(second.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(second.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(second.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(second.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation_b)).extra,
      previous_epoch_hash));
  ASSERT_TRUE(second.apply_tx_extra(
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation_a)).extra,
      previous_epoch_hash));

  EXPECT_EQ(first.state_hash(), second.state_hash());
}

TEST(epose, chain_state_applies_transaction_batch_atomically)
{
  const auto subject = make_test_identity();
  const auto verifier_a = make_test_identity();
  const auto verifier_b = make_test_identity();
  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_a.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier_b.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_a, subject.identity.registration_epoch))),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(
          make_attestation_from_verifier(subject.identity.service_public_key, verifier_b, subject.identity.registration_epoch)))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  ASSERT_TRUE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(5u, summary.transactions_scanned);
  EXPECT_EQ(3u, summary.registrations_applied);
  EXPECT_EQ(2u, summary.attestations_applied);
  EXPECT_EQ(1u, state.qualified_service_nodes(subject.identity.registration_epoch).size());
}

TEST(epose, attestation_pool_accepts_valid_registration_relay_candidate)
{
  const auto service = make_relay_identity(1);

  qwertycoin::epose::attestation_pool pool(qwertycoin::epose::EPOSE_ATTESTATION_POOL_MAX_ENTRIES, 4);
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::accepted,
      pool.add_registration(service.identity, {}, 1, cryptonote::TESTNET, crypto::null_hash));
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::duplicate,
      pool.add_registration(service.identity, {}, 1, cryptonote::TESTNET, crypto::null_hash));

  const auto selected = pool.select_registrations_for_template({}, 1, cryptonote::TESTNET, crypto::null_hash, 1);
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(service.identity.service_public_key, selected.front().service_public_key);

  const auto stats = pool.stats(1);
  EXPECT_EQ(1u, stats.registrations);
  EXPECT_EQ(1u, stats.total);
  EXPECT_EQ(1u, stats.current_epoch);
}

TEST(epose, attestation_pool_rejects_registration_chain_duplicate_and_epoch_mismatch)
{
  const auto service = make_relay_identity(1);

  qwertycoin::epose::attestation_pool pool(qwertycoin::epose::EPOSE_ATTESTATION_POOL_MAX_ENTRIES, 4);
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::invalid,
      pool.add_registration(service.identity, {service.identity}, 1, cryptonote::TESTNET, crypto::null_hash));
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::stale_epoch,
      pool.add_registration(service.identity, {}, 2, cryptonote::TESTNET, crypto::null_hash));
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::future_epoch,
      pool.add_registration(service.identity, {}, 0, cryptonote::TESTNET, crypto::null_hash));
}

TEST(epose, attestation_pool_rejects_pending_duplicate_endpoint_registration)
{
  const auto first = make_test_identity_for_endpoint(
      "pending-endpoint.example:8196",
      1,
      4);
  const auto second = make_test_identity_for_endpoint(
      "pending-endpoint.example:8196",
      1,
      4);
  crypto::hash previous_epoch_hash{};

  qwertycoin::epose::attestation_pool pool(qwertycoin::epose::EPOSE_ATTESTATION_POOL_MAX_ENTRIES, 4);
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::accepted,
      pool.add_registration(first.identity, {}, 1, cryptonote::TESTNET, previous_epoch_hash));
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::duplicate,
      pool.add_registration(second.identity, {}, 1, cryptonote::TESTNET, previous_epoch_hash));
}

TEST(epose, attestation_pool_accepts_valid_relay_candidate)
{
  const auto subject = make_test_identity(1);
  const auto verifier = make_test_identity(1);
  const auto observer = make_test_identity(1);
  const std::vector<qwertycoin::epose::service_node_identity> nodes{
      subject.identity,
      verifier.identity,
      observer.identity};
  const auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, 1);

  qwertycoin::epose::attestation_pool pool;
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::accepted,
      pool.add(attestation, nodes, {}, 1, cryptonote::TESTNET, crypto::null_hash));
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::duplicate,
      pool.add(attestation, nodes, {}, 1, cryptonote::TESTNET, crypto::null_hash));

  const auto selected = pool.select_for_template(nodes, {}, 1, cryptonote::TESTNET, crypto::null_hash, 1);
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(attestation.subject_public_key, selected.front().subject_public_key);
  EXPECT_EQ(attestation.verifier_public_key, selected.front().verifier_public_key);
}

TEST(epose, attestation_pool_rejects_chain_duplicate_and_stale_candidate)
{
  const auto subject = make_test_identity(1);
  const auto verifier = make_test_identity(1);
  const auto observer = make_test_identity(1);
  const std::vector<qwertycoin::epose::service_node_identity> nodes{
      subject.identity,
      verifier.identity,
      observer.identity};
  const auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, 1);

  qwertycoin::epose::attestation_pool pool;
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::invalid,
      pool.add(attestation, nodes, {attestation}, 1, cryptonote::TESTNET, crypto::null_hash));
  EXPECT_EQ(qwertycoin::epose::attestation_pool_result::stale_epoch,
      pool.add(attestation, nodes, {}, 4, cryptonote::TESTNET, crypto::null_hash));
}

TEST(epose, chain_state_rejects_duplicate_attestation_vote_and_rolls_back)
{
  const auto subject = make_test_identity();
  const auto verifier = make_test_identity();
  const auto attestation = make_attestation_from_verifier(subject.identity.service_public_key, verifier, subject.identity.registration_epoch);
  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(verifier.identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_attestation_tx_extra_nonce(attestation))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rolls_back_valid_registration_when_same_tx_attestation_is_invalid)
{
  const auto subject = make_test_identity();
  auto invalid_attestation = make_attestation(subject.identity.service_public_key, subject.identity.registration_epoch);
  invalid_attestation.epoch += 1;

  const cryptonote::transaction mixed_tx = make_transaction_with_extra_nonces({
      qwertycoin::epose::make_registration_tx_extra_nonce(subject.identity),
      qwertycoin::epose::make_attestation_tx_extra_nonce(invalid_attestation)});

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::tx_extra_apply_summary summary{};

  EXPECT_FALSE(state.apply_transaction(mixed_tx, previous_epoch_hash, &summary, subject.identity.registration_epoch));
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, chain_state_rolls_back_invalid_transaction_batch)
{
  const auto identity = make_identity();
  std::vector<cryptonote::transaction> txs{
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(identity)),
      make_transaction_with_extra_nonce(qwertycoin::epose::make_registration_tx_extra_nonce(identity))};

  qwertycoin::epose::chain_state state(cryptonote::TESTNET, 4);
  crypto::hash previous_epoch_hash{};
  qwertycoin::epose::transaction_apply_summary summary{};

  EXPECT_FALSE(state.apply_transactions(txs, previous_epoch_hash, &summary));
  EXPECT_EQ(0u, summary.transactions_scanned);
  EXPECT_EQ(0u, summary.registrations_applied);
  EXPECT_EQ(0u, summary.attestations_applied);
  EXPECT_EQ(0u, state.registry().size());
  EXPECT_EQ(0u, state.attestations().size());
}

TEST(epose, epoch_seed_is_network_and_epoch_bound)
{
  crypto::hash seed_block_hash = crypto::cn_fast_hash("seed-block", 10);

  const auto testnet_seed = qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, 1, seed_block_hash);
  EXPECT_EQ(testnet_seed, qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, 1, seed_block_hash));
  EXPECT_NE(testnet_seed, qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, 2, seed_block_hash));
  EXPECT_NE(testnet_seed, qwertycoin::epose::calculate_epoch_seed(cryptonote::MAINNET, 1, seed_block_hash));
}

TEST(epose, verifier_selection_is_deterministic_and_order_independent)
{
  auto identities = make_identities(8);
  const auto subject = identities.front().service_public_key;
  crypto::hash seed_block_hash = crypto::cn_fast_hash("seed-block", 10);
  const auto epoch_seed = qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, 1, seed_block_hash);

  auto selected = qwertycoin::epose::select_verifiers(identities, subject, cryptonote::TESTNET, 1, epoch_seed, 3);
  std::reverse(identities.begin(), identities.end());
  auto selected_reversed = qwertycoin::epose::select_verifiers(identities, subject, cryptonote::TESTNET, 1, epoch_seed, 3);

  ASSERT_EQ(3u, selected.size());
  ASSERT_EQ(selected.size(), selected_reversed.size());
  for (size_t i = 0; i < selected.size(); ++i)
  {
    EXPECT_EQ(selected[i].verifier_public_key, selected_reversed[i].verifier_public_key);
    EXPECT_EQ(selected[i].selection_score, selected_reversed[i].selection_score);
    EXPECT_NE(subject, selected[i].verifier_public_key);
  }
}

TEST(epose, verifier_selection_uses_only_active_nodes)
{
  auto active = make_identity(1);
  auto expired = make_identity(1);
  expired.expiry_epoch = 1;
  auto future = make_identity(3);
  std::vector<qwertycoin::epose::service_node_identity> identities{active, expired, future};
  crypto::hash seed_block_hash = crypto::cn_fast_hash("seed-block", 10);
  const auto epoch_seed = qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, 2, seed_block_hash);

  auto selected = qwertycoin::epose::select_verifiers(identities, future.service_public_key, cryptonote::TESTNET, 2, epoch_seed, 5);

  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(active.service_public_key, selected.front().verifier_public_key);
}

TEST(epose, challenge_hash_binds_subject_verifier_epoch_and_round)
{
  auto identities = make_identities(2);
  crypto::hash seed_block_hash = crypto::cn_fast_hash("seed-block", 10);
  const auto epoch_seed = qwertycoin::epose::calculate_epoch_seed(cryptonote::TESTNET, 1, seed_block_hash);

  const auto challenge = qwertycoin::epose::calculate_challenge_hash(
      epoch_seed,
      identities[0].service_public_key,
      identities[1].service_public_key,
      1,
      0);

  EXPECT_EQ(challenge, qwertycoin::epose::calculate_challenge_hash(epoch_seed, identities[0].service_public_key, identities[1].service_public_key, 1, 0));
  EXPECT_NE(challenge, qwertycoin::epose::calculate_challenge_hash(epoch_seed, identities[1].service_public_key, identities[0].service_public_key, 1, 0));
  EXPECT_NE(challenge, qwertycoin::epose::calculate_challenge_hash(epoch_seed, identities[0].service_public_key, identities[1].service_public_key, 2, 0));
  EXPECT_NE(challenge, qwertycoin::epose::calculate_challenge_hash(epoch_seed, identities[0].service_public_key, identities[1].service_public_key, 1, 1));
}
