#include "gtest/gtest.h"

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "device/device.hpp"
#include "string_tools.h"
#include "wallet/contract_call_builder.h"
#include "wallet/wallet2.h"

namespace
{
  const std::string contract_id_hex(64, 'a');
  const std::string code_hex = "0061736d";

  std::string recipient_address()
  {
    cryptonote::account_base account;
    account.generate();
    return account.get_public_address_str(cryptonote::MAINNET);
  }

  tools::contract_call_options default_options()
  {
    return tools::contract_call_options{};
  }
}

TEST(contract_call_options, extracts_known_options_and_keeps_the_rest)
{
  std::vector<std::string> tokens = {"args=0102", "deposit=1.5", "index=0", "gas_price=7", "QWC1address", "gas_limit=900", "2"};
  tools::contract_call_options options;
  std::string error;
  ASSERT_TRUE(tools::extract_contract_call_options(tokens, options, error)) << error;
  EXPECT_EQ(options.args_hex, "0102");
  EXPECT_EQ(options.gas_price, 7u);
  EXPECT_EQ(options.gas_limit, 900u);
  EXPECT_NE(options.deposit, 0u);
  EXPECT_EQ(tokens, (std::vector<std::string>{"index=0", "QWC1address", "2"}));
}

TEST(contract_call_options, defaults_apply_when_options_are_absent)
{
  std::vector<std::string> tokens = {"address", "1"};
  tools::contract_call_options options;
  std::string error;
  ASSERT_TRUE(tools::extract_contract_call_options(tokens, options, error));
  EXPECT_EQ(options.gas_price, tools::default_contract_gas_price);
  EXPECT_EQ(options.gas_limit, 0u);
  EXPECT_EQ(options.deposit, 0u);
  EXPECT_TRUE(options.args_hex.empty());
  EXPECT_EQ(tokens.size(), 2u);
}

TEST(contract_call_options, parses_contract_key_ids)
{
  const crypto::hash first = crypto::cn_fast_hash("first", 5);
  const crypto::hash second = crypto::cn_fast_hash("second", 6);
  std::vector<std::string> tokens{"key_ids=" + epee::string_tools::pod_to_hex(first) + "," + epee::string_tools::pod_to_hex(second)};
  tools::contract_call_options options;
  std::string error;
  ASSERT_TRUE(tools::extract_contract_call_options(tokens, options, error));
  EXPECT_TRUE(tokens.empty());
  EXPECT_EQ((std::vector<crypto::hash>{first, second}), options.signer_key_ids);
  tokens = {"key_ids=broken"};
  EXPECT_FALSE(tools::extract_contract_call_options(tokens, options, error));
}

TEST(contract_call_options, parses_external_signers)
{
  const cryptonote::keypair first = cryptonote::keypair::generate(hw::get_device("default"));
  const cryptonote::keypair second = cryptonote::keypair::generate(hw::get_device("default"));
  const std::string a = epee::string_tools::pod_to_hex(first.pub);
  const std::string b = epee::string_tools::pod_to_hex(second.pub);
  std::vector<std::string> tokens{"external_signers=" + a + "," + b};
  tools::contract_call_options options;
  std::string error;
  ASSERT_TRUE(tools::extract_contract_call_options(tokens, options, error));
  EXPECT_TRUE(tokens.empty());
  EXPECT_EQ((std::vector<crypto::public_key>{first.pub, second.pub}), options.external_signers);
  EXPECT_FALSE(tools::parse_contract_public_keys({"bad"}, options.external_signers));
  for (const std::string& bad : {std::string(64, 'f'), a.substr(1), a + "0", std::string(64, 'z')})
    EXPECT_FALSE(tools::parse_contract_public_keys({bad}, options.external_signers));
  tokens = {"external_signers=" + a + ","};
  EXPECT_FALSE(tools::extract_contract_call_options(tokens, options, error));
}

TEST(contract_call_options, signer_authorization_matches_origin)
{
  const crypto::hash called = crypto::cn_fast_hash("called", 6);
  const crypto::hash foreign = crypto::cn_fast_hash("foreign", 7);
  EXPECT_FALSE(tools::contract_signer_requires_authorization(called, 2, called, 2, false));
  EXPECT_TRUE(tools::contract_signer_requires_authorization(called, 2, foreign, 2, false));
  EXPECT_TRUE(tools::contract_signer_requires_authorization(called, 2, called, 3, false));
  EXPECT_TRUE(tools::contract_signer_requires_authorization(called, 2, called, 2, true));
}

TEST(contract_call_options, rejects_duplicates_and_bad_numbers)
{
  for (const std::vector<std::string>& bad : {
         std::vector<std::string>{"gas_limit=1", "gas_limit=2"},
         std::vector<std::string>{"gas_limit=abc"},
         std::vector<std::string>{"gas_price=-1"},
         std::vector<std::string>{"gas_price="},
         std::vector<std::string>{"deposit=not_money"}})
  {
    std::vector<std::string> tokens = bad;
    tools::contract_call_options options;
    std::string error;
    EXPECT_FALSE(tools::extract_contract_call_options(tokens, options, error));
    EXPECT_FALSE(error.empty());
  }
}

TEST(contract_call_builder, builds_deploy)
{
  tools::contract_call_options options = default_options();
  options.args_hex = "ff";
  options.gas_limit = 1234;
  cryptonote::tx_extra_vm_call call;
  ASSERT_EQ(tools::build_contract_call(cryptonote::MAINNET, true, code_hex, "", 2, options, call), tools::contract_call_build_result::ok);
  EXPECT_EQ(call.kind, cryptonote::tx_extra_vm_call::kind_deploy);
  EXPECT_EQ(call.code, std::string("\x00\x61\x73\x6d", 4));
  EXPECT_EQ(call.args, std::string("\xff", 1));
  EXPECT_EQ(call.gas_limit, 1234u);
  EXPECT_EQ(call.gas_price, tools::default_contract_gas_price);
  EXPECT_TRUE(call.mint.empty());
}

TEST(contract_call_builder, builds_invoke_with_mint_destination)
{
  tools::contract_call_options options = default_options();
  options.mint_address = recipient_address();
  options.deposit = 55;
  cryptonote::tx_extra_vm_call call;
  ASSERT_EQ(tools::build_contract_call(cryptonote::MAINNET, false, "", contract_id_hex, 5, options, call), tools::contract_call_build_result::ok);
  EXPECT_EQ(call.kind, cryptonote::tx_extra_vm_call::kind_invoke);
  EXPECT_EQ(call.method, 5u);
  EXPECT_EQ(call.deposit, 55u);
  EXPECT_TRUE(call.code.empty());
  EXPECT_EQ(call.mint.size(), 1u);
  EXPECT_EQ(epee::string_tools::pod_to_hex(call.contract_id), contract_id_hex);
}

TEST(contract_call_builder, rejects_malformed_calls)
{
  cryptonote::tx_extra_vm_call call;
  const tools::contract_call_options options = default_options();
  EXPECT_EQ(tools::build_contract_call(cryptonote::MAINNET, true, "", "", 2, options, call), tools::contract_call_build_result::bad_call);
  EXPECT_EQ(tools::build_contract_call(cryptonote::MAINNET, true, "zz", "", 2, options, call), tools::contract_call_build_result::bad_call);
  EXPECT_EQ(tools::build_contract_call(cryptonote::MAINNET, false, code_hex, contract_id_hex, 2, options, call), tools::contract_call_build_result::bad_call);
  EXPECT_EQ(tools::build_contract_call(cryptonote::MAINNET, false, "", "not_hex", 2, options, call), tools::contract_call_build_result::bad_call);

  tools::contract_call_options bad_args = options;
  bad_args.args_hex = "xyz";
  EXPECT_EQ(tools::build_contract_call(cryptonote::MAINNET, false, "", contract_id_hex, 2, bad_args, call), tools::contract_call_build_result::bad_call);
}

TEST(contract_call_builder, rejects_bad_mint_address)
{
  tools::contract_call_options options = default_options();
  options.mint_address = "not an address";
  cryptonote::tx_extra_vm_call call;
  EXPECT_EQ(tools::build_contract_call(cryptonote::MAINNET, false, "", contract_id_hex, 2, options, call), tools::contract_call_build_result::bad_mint_address);
}

TEST(contract_call_options, validates_external_signers)
{
  const cryptonote::keypair requested = cryptonote::keypair::generate(hw::get_device("default"));
  const cryptonote::keypair unknown = cryptonote::keypair::generate(hw::get_device("default"));
  EXPECT_NO_THROW(tools::wallet2::validate_external_contract_signers({requested.pub}, {requested.pub}));
  try { tools::wallet2::validate_external_contract_signers({requested.pub}, {unknown.pub}); FAIL(); }
  catch (const tools::error::wallet_internal_error& e) { EXPECT_NE(std::string::npos, std::string(e.what()).find("External signer is not requested by the contract")); }
  try { tools::wallet2::validate_external_contract_signers({requested.pub}, {requested.pub, requested.pub}); FAIL(); }
  catch (const tools::error::wallet_internal_error& e) { EXPECT_NE(std::string::npos, std::string(e.what()).find("Duplicate external signer")); }
}
