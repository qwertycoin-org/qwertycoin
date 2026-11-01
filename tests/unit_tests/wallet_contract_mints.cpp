#include <sstream>

#include "gtest/gtest.h"

#include <boost/archive/portable_binary_iarchive.hpp>
#include <boost/archive/portable_binary_oarchive.hpp>

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "cryptonote_core/contract_state.h"
#include "device/device.hpp"
#include "wallet/wallet2.h"

using namespace cryptonote;

namespace
{
  struct mint_block_fixture
  {
    hw::device &hwdev = hw::get_device("default");
    account_base recipient;
    std::unordered_map<crypto::public_key, subaddress_index> subaddresses;
    transaction miner_tx;
    crypto::public_key main_tx_key;
    crypto::key_derivation main_derivation;
    std::vector<crypto::key_derivation> additional_derivations;
    size_t mint_position = 0;

    explicit mint_block_fixture(bool mint_to_subaddress)
    {
      recipient.generate();
      const account_keys &keys = recipient.get_keys();
      subaddresses[keys.m_account_address.m_spend_public_key] = {0, 0};
      subaddresses[hwdev.get_subaddress_spend_public_key(keys, {0, 1})] = {0, 1};

      const account_keys other_keys = [] { account_base other; other.generate(); return other.get_keys(); }();
      const keypair main_pair = keypair::generate(hwdev);
      main_tx_key = main_pair.pub;
      miner_tx.version = 2;
      miner_tx.vin.push_back(txin_gen{1});
      crypto::key_derivation reward_derivation;
      crypto::generate_key_derivation(other_keys.m_account_address.m_view_public_key, main_pair.sec, reward_derivation);
      crypto::public_key reward_key;
      crypto::derive_public_key(reward_derivation, 0, other_keys.m_account_address.m_spend_public_key, reward_key);
      tx_out reward;
      set_tx_out(1000, reward_key, true, crypto::view_tag{}, reward);
      miner_tx.vout.push_back(reward);
      add_tx_pub_key_to_extra(miner_tx, main_tx_key);

      const account_public_address target = mint_to_subaddress ? hwdev.get_subaddress(keys, {0, 1}) : keys.m_account_address;
      tx_extra_vm_mint_destination destination{};
      EXPECT_TRUE(make_contract_mint_destination(target, mint_to_subaddress, destination));
      EXPECT_TRUE(add_contract_mints_to_miner_tx(miner_tx, {contract_mint{777, destination}}));
      mint_position = miner_tx.vout.size() - 1;

      crypto::generate_key_derivation(main_tx_key, keys.m_view_secret_key, main_derivation);
      for (const crypto::public_key &additional_key : get_additional_tx_pub_keys_from_extra(miner_tx))
      {
        additional_derivations.emplace_back();
        crypto::generate_key_derivation(additional_key, keys.m_view_secret_key, additional_derivations.back());
      }
    }

    boost::optional<subaddress_receive_info> scan(size_t output_index, bool miner_tx_flag) const
    {
      crypto::public_key output_key;
      EXPECT_TRUE(get_output_public_key(miner_tx.vout[output_index], output_key));
      return is_out_to_acc_or_mint_precomp(subaddresses, output_key, main_derivation, additional_derivations, output_index,
        miner_tx.vout.size(), miner_tx_flag, hwdev, get_output_view_tag(miner_tx.vout[output_index]));
    }
  };
}

class wallet_contract_mints : public ::testing::TestWithParam<bool> {};

TEST_P(wallet_contract_mints, mint_output_is_received_at_mint_derivation_index)
{
  const mint_block_fixture fixture(GetParam());
  const auto received = fixture.scan(fixture.mint_position, true);
  ASSERT_TRUE(received);
  EXPECT_EQ(QWC_MINT_DERIVATION_INDEX, received->derivation_index);
  const subaddress_index expected_index = GetParam() ? subaddress_index{0, 1} : subaddress_index{0, 0};
  EXPECT_TRUE(expected_index == received->index);
}

TEST_P(wallet_contract_mints, mint_output_is_ignored_outside_a_miner_tx)
{
  const mint_block_fixture fixture(GetParam());
  EXPECT_FALSE(fixture.scan(fixture.mint_position, false));
}

TEST_P(wallet_contract_mints, reward_output_of_another_account_is_not_received)
{
  const mint_block_fixture fixture(GetParam());
  EXPECT_FALSE(fixture.scan(0, true));
}

TEST_P(wallet_contract_mints, mint_key_source_yields_the_output_key_image)
{
  const mint_block_fixture fixture(GetParam());
  const account_keys &keys = fixture.recipient.get_keys();
  crypto::public_key output_key;
  ASSERT_TRUE(get_output_public_key(fixture.miner_tx.vout[fixture.mint_position], output_key));

  const output_key_source source = make_output_key_source(true, fixture.main_tx_key,
    get_additional_tx_pub_keys_from_extra(fixture.miner_tx), fixture.miner_tx.vout.size(), fixture.mint_position);
  EXPECT_EQ(QWC_MINT_DERIVATION_INDEX, source.derivation_index);
  EXPECT_TRUE(source.additional_keys.empty());
  EXPECT_NE(fixture.main_tx_key, source.tx_key);

  keypair ephemeral;
  crypto::key_image image;
  ASSERT_TRUE(generate_key_image_helper(keys, fixture.subaddresses, output_key, source.tx_key, source.additional_keys,
    source.derivation_index, ephemeral, image, fixture.hwdev));
  EXPECT_EQ(output_key, ephemeral.pub);
}

TEST_P(wallet_contract_mints, vout_index_derivation_cannot_spend_mint)
{
  const mint_block_fixture fixture(GetParam());
  crypto::public_key output_key;
  ASSERT_TRUE(get_output_public_key(fixture.miner_tx.vout[fixture.mint_position], output_key));
  keypair ephemeral;
  crypto::key_image image;
  EXPECT_FALSE(generate_key_image_helper(fixture.recipient.get_keys(), fixture.subaddresses, output_key, fixture.main_tx_key,
    get_additional_tx_pub_keys_from_extra(fixture.miner_tx), fixture.mint_position, ephemeral, image, fixture.hwdev));
}

INSTANTIATE_TEST_CASE_P(address_kinds, wallet_contract_mints, ::testing::Values(false, true));

TEST(wallet_contract_mints_source, reward_output_keeps_vout_index_derivation)
{
  const mint_block_fixture fixture(false);
  const std::vector<crypto::public_key> additional_keys = get_additional_tx_pub_keys_from_extra(fixture.miner_tx);
  const output_key_source source = make_output_key_source(true, fixture.main_tx_key, additional_keys, fixture.miner_tx.vout.size(), 0);
  EXPECT_EQ(0u, source.derivation_index);
  EXPECT_EQ(fixture.main_tx_key, source.tx_key);
  EXPECT_EQ(additional_keys, source.additional_keys);
}

TEST(wallet_contract_mints_source, ordinary_transaction_never_uses_mint_derivation)
{
  const mint_block_fixture fixture(false);
  const std::vector<crypto::public_key> additional_keys = get_additional_tx_pub_keys_from_extra(fixture.miner_tx);
  const output_key_source source = make_output_key_source(false, fixture.main_tx_key, additional_keys, fixture.miner_tx.vout.size(), fixture.mint_position);
  EXPECT_EQ(fixture.mint_position, source.derivation_index);
  EXPECT_EQ(fixture.main_tx_key, source.tx_key);
}

TEST(wallet_contract_mints_transfer, key_source_follows_derivation_index)
{
  const mint_block_fixture fixture(false);
  tools::wallet2::transfer_details mint_transfer{};
  mint_transfer.m_tx = fixture.miner_tx;
  mint_transfer.m_internal_output_index = fixture.mint_position;
  mint_transfer.m_derivation_index = QWC_MINT_DERIVATION_INDEX;
  const output_key_source mint_source = mint_transfer.get_key_source(fixture.main_tx_key);
  EXPECT_EQ(QWC_MINT_DERIVATION_INDEX, mint_source.derivation_index);
  EXPECT_EQ(get_additional_tx_pub_keys_from_extra(fixture.miner_tx)[fixture.mint_position], mint_source.tx_key);

  tools::wallet2::transfer_details reward_transfer = mint_transfer;
  reward_transfer.m_internal_output_index = 0;
  reward_transfer.m_derivation_index = 0;
  const output_key_source reward_source = reward_transfer.get_key_source(fixture.main_tx_key);
  EXPECT_EQ(0u, reward_source.derivation_index);
  EXPECT_EQ(fixture.main_tx_key, reward_source.tx_key);
}

TEST(wallet_contract_mints_transfer, mint_transfer_without_additional_keys_is_rejected)
{
  tools::wallet2::transfer_details broken{};
  broken.m_internal_output_index = 3;
  broken.m_derivation_index = QWC_MINT_DERIVATION_INDEX;
  EXPECT_THROW(broken.get_key_source(crypto::public_key{}), tools::error::wallet_internal_error);
}

TEST(wallet_contract_mints_transfer, derivation_index_survives_serialization)
{
  const mint_block_fixture fixture(false);
  tools::wallet2::transfer_details original{};
  original.m_tx = fixture.miner_tx;
  original.m_internal_output_index = fixture.mint_position;
  original.m_derivation_index = QWC_MINT_DERIVATION_INDEX;
  original.m_amount = 777;

  std::stringstream stream;
  {
    boost::archive::portable_binary_oarchive writer(stream);
    writer << original;
  }
  tools::wallet2::transfer_details restored{};
  {
    boost::archive::portable_binary_iarchive reader(stream);
    reader >> restored;
  }
  EXPECT_EQ(original.m_derivation_index, restored.m_derivation_index);
  EXPECT_EQ(original.m_internal_output_index, restored.m_internal_output_index);
}
