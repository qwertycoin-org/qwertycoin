#include <utility>
#include <sstream>
#include <array>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "gtest/gtest.h"
#include "misc_language.h"

#include "serialization/string.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_core/contract_state.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "device/device.hpp"
#include "device/device_default.hpp"
#include "string_tools.h"
#include "vm/gas.h"
#include "vm/limits.h"
#include "vm_test_utils.h"
#include "wallet/contract_call_builder.h"
#include "wallet/wallet2.h"
#include "test_multisig_wallet.h"

namespace
{
  using namespace cryptonote;
  constexpr uint64_t fee = 1000000000;

  std::string signature_contract(size_t signer_count = 1)
  {
    vm_test::module_builder module;
    const uint32_t add_sig = module.import("AddSig", {vm_test::t_i32}, {});
    const uint32_t method = vm_test::method_type(module);
    vm_test::add_ctor_dtor(module, method);
    vm_test::code body;
    for (size_t i = 0; i < signer_count; ++i)
      body.arg_at(static_cast<int32_t>(i * sizeof(crypto::public_key))).call(add_sig);
    module.exp("Method_2", module.func(method, body.end()));
    return module.build();
  }

  struct fixture
  {
    vm::state_tree state;
    contract_block_context context;
    std::string code;
    crypto::hash cid = crypto::null_hash;

    explicit fixture(size_t signer_count = 1) : code(signature_contract(signer_count))
    {
      const vm::result deployed = vm_test::deploy(state, code);
      EXPECT_EQ(vm::status::ok, deployed.code);
      cid = deployed.contract_id;
    }

    tx_extra_vm_call call_for(const crypto::public_key& key) const
    {
      tx_extra_vm_call call{};
      call.kind = tx_extra_vm_call::kind_invoke;
      call.contract_id = cid;
      call.method = 2;
      call.args.assign(reinterpret_cast<const char*>(&key), sizeof(key));
      call.gas_limit = 1000000;
      call.gas_price = 1;
      return call;
    }
  };

  transaction make_tx(const tx_extra_vm_call& call)
  {
    transaction tx{};
    tx.version = 2;
    tx.rct_signatures.txnFee = fee;
    EXPECT_TRUE(add_vm_call_to_tx_extra(tx.extra, call));
    return tx;
  }

  void replace_call(transaction& tx, const tx_extra_vm_call& call)
  {
    ASSERT_TRUE(replace_contract_call(tx, call));
  }

  transaction signed_tx(tx_extra_vm_call call, const cryptonote::keypair& signer)
  {
    call.signers.push_back(signer.pub);
    call.signer_contracts.push_back(call.contract_id);
    call.signer_methods.push_back(call.method);
    call.signer_key_ids.push_back(crypto::null_hash);
    call.signer_nested.push_back(0);
    call.signatures.emplace_back();
    transaction tx = make_tx(call);
    crypto::hash hash;
    EXPECT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, call.contract_id, hash));
    crypto::generate_signature(hash, signer.pub, signer.sec, call.signatures.front().value);
    replace_call(tx, call);
    return tx;
  }

  void expect_both(const fixture& f, const transaction& tx, contract_call_verdict verdict)
  {
    contract_overlay overlay(f.state);
    contract_effects effects;
    EXPECT_EQ(verdict, execute_contract_tx(tx, fee, f.context, overlay, effects));
    if (verdict != contract_call_verdict::executed)
      EXPECT_TRUE(overlay.writes().empty());
    contract_block_executor block(f.state, f.context);
    EXPECT_EQ(verdict, block.execute(tx, fee));
    if (verdict != contract_call_verdict::executed)
      EXPECT_TRUE(block.writes().empty());
  }

  tools::wallet2::pending_tx external_pending(const fixture& f, const std::vector<crypto::public_key>& signers)
  {
    tx_extra_vm_call call = f.call_for(signers.front());
    for (size_t i = 1; i < signers.size(); ++i)
      call.args.append(reinterpret_cast<const char*>(&signers[i]), sizeof(signers[i]));
    for (const crypto::public_key& key : signers)
    {
      call.signers.push_back(key);
      call.signer_contracts.push_back(f.cid);
      call.signer_methods.push_back(call.method);
      call.signer_key_ids.push_back(crypto::null_hash);
      call.signer_nested.push_back(0);
      call.signatures.emplace_back();
    }
    tools::wallet2::pending_tx ptx{};
    ptx.tx = make_tx(call);
    return ptx;
  }

  std::pair<std::string, std::string> hex_signature(const tools::wallet2::pending_tx& ptx, const keypair& signer)
  {
    crypto::hash digest{};
    EXPECT_TRUE(contract_call_signature_hash(ptx.tx, MAINNET, digest));
    crypto::signature signature{};
    crypto::generate_signature(digest, signer.pub, signer.sec, signature);
    return {epee::string_tools::pod_to_hex(signer.pub), epee::string_tools::pod_to_hex(signature)};
  }

  void expect_wallet_error(const std::function<void()>& action, const char* message)
  {
    try { action(); FAIL(); }
    catch (const tools::error::wallet_internal_error& e) { EXPECT_STREQ(message, e.what()); }
  }
}

TEST(contract_addsig, signed_key_is_required_in_order)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  const transaction tx = signed_tx(f.call_for(signer.pub), signer);
  expect_both(f, tx, contract_call_verdict::executed);

  tx_extra_vm_call call{};
  ASSERT_EQ(1u, get_tx_vm_call(tx.extra, call));
  ASSERT_EQ(1u, call.signers.size());
  EXPECT_EQ(signer.pub, call.signers.front());
}

TEST(contract_addsig, dry_run_accepts_requested_key_metadata_before_signing)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  tools::contract_call_options options;
  options.gas_limit = 1000000;
  options.args_hex = epee::string_tools::buff_to_hex_nodelimer(
      std::string(reinterpret_cast<const char*>(&signer.pub), sizeof(signer.pub)));
  options.signer_key_ids.push_back(crypto::cn_fast_hash("key-id", 6));
  tx_extra_vm_call call{};
  ASSERT_EQ(tools::contract_call_build_result::ok,
      tools::build_contract_call(MAINNET, false, "", epee::string_tools::pod_to_hex(f.cid), 2, options, call));
  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  ASSERT_EQ(options.signer_key_ids, call.signer_key_ids);
  contract_overlay overlay(f.state);
  contract_effects effects;
  EXPECT_EQ(contract_call_verdict::executed, execute_contract_tx(make_tx(call), fee, f.context, overlay, effects, false));
  EXPECT_EQ(contract_call_verdict::malformed, execute_contract_tx(make_tx(call), fee, f.context, overlay, effects, true));
}

TEST(contract_addsig, rejects_missing_wrong_extra_duplicate_and_changed_prefix)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  const keypair other = keypair::generate(hw::get_device("default"));
  const tx_extra_vm_call requested = f.call_for(signer.pub);
  expect_both(f, make_tx(requested), contract_call_verdict::signer_mismatch);

  transaction wrong = signed_tx(requested, other);
  expect_both(f, wrong, contract_call_verdict::signer_mismatch);

  wrong = signed_tx(requested, signer);
  tx_extra_vm_call wrong_signature{};
  ASSERT_EQ(1u, get_tx_vm_call(wrong.extra, wrong_signature));
  crypto::hash hash;
  ASSERT_TRUE(get_contract_signature_hash(wrong, MAINNET, fee, requested.contract_id, hash));
  crypto::generate_signature(hash, other.pub, other.sec, wrong_signature.signatures.front().value);
  replace_call(wrong, wrong_signature);
  expect_both(f, wrong, contract_call_verdict::invalid_signature);

  transaction good = signed_tx(requested, signer);
  tx_extra_vm_call call{};
  ASSERT_EQ(1u, get_tx_vm_call(good.extra, call));
  call.signers.push_back(other.pub);
  call.signatures.push_back(call.signatures.front());
  replace_call(good, call);
  expect_both(f, good, contract_call_verdict::malformed);

  call.signers.back() = signer.pub;
  replace_call(good, call);
  expect_both(f, good, contract_call_verdict::malformed);

  call.signers.pop_back();
  replace_call(good, call);
  expect_both(f, good, contract_call_verdict::malformed);

  transaction changed = signed_tx(requested, signer);
  tx_out output{};
  set_tx_out(5, other.pub, false, crypto::view_tag{}, output);
  changed.vout.push_back(output);
  expect_both(f, changed, contract_call_verdict::invalid_signature);
}

TEST(contract_addsig, rejects_unknown_call_version)
{
  fixture f;
  tx_extra_vm_call call = f.call_for(keypair::generate(hw::get_device("default")).pub);
  call.version = 2;
  expect_both(f, make_tx(call), contract_call_verdict::malformed);
}

TEST(contract_addsig, rejects_signer_shape_before_execution)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(signer.pub);
  call.args = "invalid module arguments";
  call.signers.push_back(signer.pub);
  expect_both(f, make_tx(call), contract_call_verdict::malformed);

  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  expect_both(f, make_tx(call), contract_call_verdict::malformed);

  call.signatures.resize(1);
  call.signer_nested.front() = 2;
  expect_both(f, make_tx(call), contract_call_verdict::malformed);
}

TEST(contract_addsig, signature_order_matches_vm_requests)
{
  fixture f(2);
  const keypair first = keypair::generate(hw::get_device("default"));
  const keypair second = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(first.pub);
  call.args.append(reinterpret_cast<const char*>(&second.pub), sizeof(second.pub));
  call.signers = {first.pub, second.pub};
  call.signer_contracts = {f.cid, f.cid};
  call.signer_methods = {call.method, call.method};
  call.signer_key_ids = {crypto::null_hash, crypto::null_hash};
  call.signer_nested = {0, 0};
  call.signatures.resize(2);
  transaction tx = make_tx(call);
  crypto::hash hash;
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, call.contract_id, hash));
  crypto::generate_signature(hash, first.pub, first.sec, call.signatures[0].value);
  crypto::generate_signature(hash, second.pub, second.sec, call.signatures[1].value);
  replace_call(tx, call);
  expect_both(f, tx, contract_call_verdict::executed);

  std::swap(call.signatures[0], call.signatures[1]);
  replace_call(tx, call);
  expect_both(f, tx, contract_call_verdict::invalid_signature);
}

TEST(contract_addsig, gas_is_charged)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  vm::call request;
  request.contract_id = f.cid;
  request.method = 2;
  request.args.assign(reinterpret_cast<const char*>(&signer.pub), sizeof(signer.pub));
  const vm::result executed = vm::execute({}, request, f.state, vm_test::ample_gas);
  ASSERT_EQ(vm::status::ok, executed.code);
  EXPECT_EQ((std::vector<crypto::public_key>{signer.pub}), executed.required_signers);
  EXPECT_GE(executed.gas_used, vm::gas::host_call_cost(vm::host_call::AddSig));
  EXPECT_EQ(vm::status::out_of_gas, vm::execute({}, request, f.state, executed.gas_used - 1).code);
}

TEST(contract_addsig, gas_charges_each_requested_key)
{
  fixture single;
  fixture double_request(2);
  const keypair first = keypair::generate(hw::get_device("default"));
  const keypair second = keypair::generate(hw::get_device("default"));
  vm::call request;
  request.contract_id = single.cid;
  request.method = 2;
  request.args.assign(reinterpret_cast<const char*>(&first.pub), sizeof(first.pub));
  const vm::result one = vm::execute({}, request, single.state, vm_test::ample_gas);
  ASSERT_EQ(vm::status::ok, one.code);
  request.contract_id = double_request.cid;
  request.args.append(reinterpret_cast<const char*>(&second.pub), sizeof(second.pub));
  const vm::result two = vm::execute({}, request, double_request.state, vm_test::ample_gas);
  ASSERT_EQ(vm::status::ok, two.code);
  EXPECT_GE(two.gas_used - one.gas_used, vm::gas::host_call_cost(vm::host_call::CheckSignature));
}

TEST(contract_addsig, repeated_key_requires_one_signature)
{
  fixture f(2);
  const keypair signer = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(signer.pub);
  call.args.append(reinterpret_cast<const char*>(&signer.pub), sizeof(signer.pub));
  const transaction tx = signed_tx(call, signer);
  expect_both(f, tx, contract_call_verdict::executed);

  vm::call request;
  request.contract_id = f.cid;
  request.method = 2;
  request.args = call.args;
  const vm::result executed = vm::execute({}, request, f.state, vm_test::ample_gas);
  ASSERT_EQ(vm::status::ok, executed.code);
  EXPECT_EQ((std::vector<crypto::public_key>{signer.pub}), executed.required_signers);
}

TEST(contract_addsig, nested_call_records_request_origin)
{
  fixture callee;
  vm_test::module_builder module;
  const uint32_t far_call = module.import("CallFar", {vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32}, {});
  const uint32_t method = vm_test::method_type(module);
  vm_test::add_ctor_dtor(module, method);
  vm_test::code body;
  body.arg_at(0).i32(2).arg_at(32).i32(32).i32(0).call(far_call).end();
  module.exp("Method_2", module.func(method, body));
  const vm::result deployed = vm_test::deploy(callee.state, module.build());
  ASSERT_EQ(vm::status::ok, deployed.code);

  const keypair signer = keypair::generate(hw::get_device("default"));
  vm::call request;
  request.contract_id = deployed.contract_id;
  request.method = 2;
  request.args.assign(reinterpret_cast<const char*>(&callee.cid), sizeof(callee.cid));
  request.args.append(reinterpret_cast<const char*>(&signer.pub), sizeof(signer.pub));
  const vm::result result = vm::execute({}, request, callee.state, vm_test::ample_gas);
  ASSERT_EQ(vm::status::ok, result.code);
  ASSERT_EQ(1u, result.signer_origins.size());
  EXPECT_EQ(callee.cid, result.signer_origins.front().contract_id);
  EXPECT_EQ(2u, result.signer_origins.front().method);
  EXPECT_TRUE(result.signer_origins.front().nested);
}

TEST(contract_addsig, message_binds_context_and_rct_base)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(signer.pub);
  transaction tx = make_tx(call);
  crypto::hash original{};
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, f.cid, original));

  crypto::hash changed{};
  EXPECT_TRUE(get_contract_signature_hash(tx, TESTNET, fee, f.cid, changed));
  EXPECT_NE(original, changed);
  EXPECT_TRUE(get_contract_signature_hash(tx, MAINNET, fee + 1, f.cid, changed));
  EXPECT_NE(original, changed);
  const crypto::hash other_contract = crypto::cn_fast_hash("other", 5);
  EXPECT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, other_contract, changed));
  EXPECT_NE(original, changed);

  ++call.method;
  replace_call(tx, call);
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, f.cid, changed));
  EXPECT_NE(original, changed);

  --call.method;
  replace_call(tx, call);
  tx_out output{};
  set_tx_out(0, signer.pub, false, crypto::view_tag{}, output);
  tx.vout.push_back(output);
  tx.rct_signatures.type = rct::RCTTypeSimple;
  tx.rct_signatures.outPk.resize(1);
  tx.rct_signatures.ecdhInfo.resize(1);
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, f.cid, original));
  tx.rct_signatures.outPk.front().mask.bytes[0] ^= 1;
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, f.cid, changed));
  EXPECT_NE(original, changed);
  tx.rct_signatures.outPk.front().mask.bytes[0] ^= 1;
  tx.rct_signatures.ecdhInfo.front().amount.bytes[0] ^= 1;
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, f.cid, changed));
  EXPECT_NE(original, changed);
}

TEST(contract_addsig, reserve_proof_signature_is_not_call_signature)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  transaction tx = make_tx(f.call_for(signer.pub));
  account_base victim;
  victim.generate();
  crypto::key_image image{};
  memset(&image, 0x42, sizeof(image));
  std::string suffix(reinterpret_cast<const char*>(&victim.get_keys().m_account_address), sizeof(account_public_address));
  suffix.append(reinterpret_cast<const char*>(&image), sizeof(image));
  tx_extra_field field = tx_extra_mysterious_minergate{suffix};
  std::ostringstream encoded;
  binary_archive<true> archive(encoded);
  ASSERT_TRUE(::do_serialize(archive, field));
  const std::string bytes = encoded.str();
  tx.extra.insert(tx.extra.end(), bytes.begin(), bytes.end());
  ASSERT_TRUE(sort_tx_extra(tx.extra, tx.extra, false, true, true));
  const blobdata prefix = t_serializable_object_to_blob(static_cast<const transaction_prefix&>(tx));
  ASSERT_GE(prefix.size(), suffix.size());
  ASSERT_EQ(suffix, prefix.substr(prefix.size() - suffix.size()));
  const std::string attacker_message = prefix.substr(0, prefix.size() - suffix.size());
  const std::string proof_payload = attacker_message + suffix;
  const crypto::hash reserve_hash = crypto::cn_fast_hash(proof_payload.data(), proof_payload.size());
  crypto::hash normalized{};
  ASSERT_TRUE(get_contract_signing_prefix_hash(tx, normalized));
  ASSERT_EQ(normalized, reserve_hash);
  crypto::hash call_hash{};
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, f.cid, call_hash));
  crypto::signature reserve_signature{};
  crypto::generate_signature(reserve_hash, signer.pub, signer.sec, reserve_signature);
  EXPECT_FALSE(crypto::check_signature(call_hash, signer.pub, reserve_signature));
}

TEST(contract_addsig, derived_key_is_scoped_to_contract_and_key_id)
{
  account_base account;
  account.generate();
  const account_keys& keys = account.get_keys();
  const crypto::hash first_contract = crypto::cn_fast_hash("contract-a", 10);
  const crypto::hash second_contract = crypto::cn_fast_hash("contract-b", 10);
  const crypto::hash first_id = crypto::cn_fast_hash("key-a", 5);
  const crypto::hash second_id = crypto::cn_fast_hash("key-b", 5);
  crypto::public_key first{};
  crypto::public_key repeat{};
  crypto::public_key other_contract{};
  crypto::public_key other_id{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(keys, first_contract, first_id, first, secret));
  ASSERT_TRUE(derive_contract_key(keys, first_contract, first_id, repeat, secret));
  ASSERT_TRUE(derive_contract_key(keys, second_contract, first_id, other_contract, secret));
  ASSERT_TRUE(derive_contract_key(keys, first_contract, second_id, other_id, secret));
  EXPECT_EQ(first, repeat);
  EXPECT_NE(first, other_contract);
  EXPECT_NE(first, other_id);
  EXPECT_NE(first, keys.m_account_address.m_spend_public_key);
  EXPECT_NE(first, keys.m_account_address.m_view_public_key);
}

TEST(contract_addsig, non_software_device_cannot_derive_contract_key)
{
  struct fake_hardware_device : hw::core::device_default
  {
    device_type get_type() const override { return device_type::LEDGER; }
  } device;
  account_base account;
  account.generate();
  account_keys keys = account.get_keys();
  keys.set_device(device);
  memset(&keys.m_spend_secret_key, 0xff, sizeof(keys.m_spend_secret_key));
  crypto::public_key public_key{};
  crypto::secret_key secret_key{};
  EXPECT_FALSE(derive_contract_key(keys, crypto::null_hash, crypto::null_hash, public_key, secret_key));
  EXPECT_EQ(crypto::null_pkey, public_key);
  EXPECT_EQ(crypto::null_skey, secret_key);
}

TEST(contract_addsig, zero_signer_call_preparation_allows_cold_wallets)
{
  struct fake_hardware_device : hw::core::device_default
  {
    device_type get_type() const override { return device_type::LEDGER; }
  } device;
  account_base account;
  account.generate();
  account_keys hardware = account.get_keys();
  hardware.set_device(device);
  account_keys watch_only = account.get_keys();
  watch_only.m_spend_secret_key = crypto::null_skey;
  EXPECT_TRUE(tools::can_prepare_contract_signers(hardware, 0));
  EXPECT_TRUE(tools::can_prepare_contract_signers(watch_only, 0));
  EXPECT_FALSE(tools::can_prepare_contract_signers(hardware, 1));
  EXPECT_FALSE(tools::can_prepare_contract_signers(watch_only, 1));
  EXPECT_TRUE(tools::can_prepare_contract_signers(account.get_keys(), 1));
}

TEST(contract_addsig, invalid_public_keys_trap)
{
  fixture f;
  vm::call request;
  request.contract_id = f.cid;
  request.method = 2;
  for (const std::string& key : {std::string(32, '\0'), std::string(32, '\xff'), std::string("\x01" + std::string(31, '\0'))})
  {
    request.args = key;
    EXPECT_EQ(vm::status::trap, vm::execute({}, request, f.state, vm_test::ample_gas).code);
  }
}

TEST(contract_addsig, rejected_signature_keeps_effects_clean)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  transaction tx = signed_tx(f.call_for(signer.pub), signer);
  tx_extra_vm_call call{};
  ASSERT_EQ(1u, get_tx_vm_call(tx.extra, call));
  call.signatures.front().value.c.data[0] ^= 1;
  replace_call(tx, call);
  contract_overlay overlay(f.state);
  contract_effects effects;
  EXPECT_EQ(contract_call_verdict::invalid_signature, execute_contract_tx(tx, fee, f.context, overlay, effects));
  EXPECT_TRUE(effects.required_signers.empty());
  EXPECT_TRUE(overlay.writes().empty());
  contract_block_executor template_calls(f.state, f.context);
  EXPECT_EQ(contract_call_verdict::executed, template_calls.execute(signed_tx(f.call_for(signer.pub), signer), fee));
  ASSERT_EQ(1u, template_calls.effects().required_signers.size());
  EXPECT_EQ(contract_call_verdict::invalid_signature, template_calls.execute(tx, fee));
  EXPECT_TRUE(template_calls.effects().required_signers.empty());
  transaction unrelated{};
  EXPECT_EQ(contract_call_verdict::absent, template_calls.execute(unrelated, fee));
  EXPECT_TRUE(template_calls.effects().required_signers.empty());
}

TEST(contract_addsig, transaction_builder_signs_derived_key_only)
{
  fixture f;
  account_base account;
  account.generate();
  const account_keys& keys = account.get_keys();
  crypto::public_key derived{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(keys, f.cid, crypto::null_hash, derived, secret));
  tx_extra_vm_call call = f.call_for(derived);
  call.signers.push_back(derived);
  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  call.signatures.emplace_back();
  transaction tx{};
  tx.version = 2;
  ASSERT_TRUE(add_vm_call_to_tx_extra(tx.extra, call));
  ASSERT_TRUE(sign_contract_call(tx, keys, MAINNET, fee));
  expect_both(f, tx, contract_call_verdict::executed);

  tx_extra_vm_call unavailable = f.call_for(keys.m_account_address.m_spend_public_key);
  unavailable.signers.push_back(keys.m_account_address.m_spend_public_key);
  unavailable.signer_contracts.push_back(f.cid);
  unavailable.signer_methods.push_back(unavailable.method);
  unavailable.signer_key_ids.push_back(crypto::null_hash);
  unavailable.signer_nested.push_back(0);
  unavailable.signatures.emplace_back();
  transaction other{};
  other.version = 2;
  ASSERT_TRUE(add_vm_call_to_tx_extra(other.extra, unavailable));
  EXPECT_FALSE(sign_contract_call(other, keys, MAINNET, fee));

  unavailable.signers.front() = derived;
  unavailable.signer_contracts.front() = crypto::cn_fast_hash("foreign", 7);
  replace_call(other, unavailable);
  EXPECT_FALSE(sign_contract_call(other, keys, MAINNET, fee));
}

TEST(contract_addsig, external_signer_digest_matches_node_hash)
{
  fixture f(2);
  account_base account;
  account.generate();
  const keypair external = keypair::generate(hw::get_device("default"));
  crypto::public_key derived{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(account.get_keys(), f.cid, crypto::null_hash, derived, secret));
  tx_extra_vm_call call = f.call_for(external.pub);
  call.args.assign(reinterpret_cast<const char*>(&derived), sizeof(derived));
  call.args.append(reinterpret_cast<const char*>(&external.pub), sizeof(external.pub));
  for (const crypto::public_key& key : {derived, external.pub})
  {
    call.signers.push_back(key);
    call.signer_contracts.push_back(f.cid);
    call.signer_methods.push_back(call.method);
    call.signer_key_ids.push_back(crypto::null_hash);
    call.signer_nested.push_back(0);
    call.signatures.emplace_back();
  }
  transaction tx = make_tx(call);
  ASSERT_TRUE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {external.pub}));
  ASSERT_EQ(1u, get_tx_vm_call(tx.extra, call));
  const crypto::signature empty{};
  EXPECT_EQ(0, memcmp(&call.signatures[1].value, &empty, sizeof(empty)));
  EXPECT_NE(0, memcmp(&call.signatures[0].value, &empty, sizeof(empty)));
  crypto::hash digest{};
  crypto::hash node_hash{};
  ASSERT_TRUE(contract_call_signature_hash(tx, MAINNET, digest));
  ASSERT_EQ(fee, get_tx_fee(tx));
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, fee, called_contract_id(call), node_hash));
  EXPECT_TRUE(crypto::check_signature(digest, derived, call.signatures[0].value));
  EXPECT_EQ(digest, node_hash);
  EXPECT_FALSE(contract_call_signed(tx));
  expect_both(f, tx, contract_call_verdict::invalid_signature);
  EXPECT_FALSE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {external.pub, external.pub}));
  const keypair other = keypair::generate(hw::get_device("default"));
  EXPECT_FALSE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {other.pub}));
}

TEST(contract_addsig, external_signature_attaches_and_executes)
{
  fixture f(2);
  account_base account;
  account.generate();
  const keypair external = keypair::generate(hw::get_device("default"));
  crypto::public_key derived{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(account.get_keys(), f.cid, crypto::null_hash, derived, secret));
  tx_extra_vm_call call = f.call_for(derived);
  call.args.append(reinterpret_cast<const char*>(&external.pub), sizeof(external.pub));
  for (const crypto::public_key& key : {derived, external.pub})
  {
    call.signers.push_back(key);
    call.signer_contracts.push_back(f.cid);
    call.signer_methods.push_back(call.method);
    call.signer_key_ids.push_back(crypto::null_hash);
    call.signer_nested.push_back(0);
    call.signatures.emplace_back();
  }
  transaction tx = make_tx(call);
  tx.version = 1;
  txin_to_key input{};
  input.amount = fee;
  tx.vin.push_back(input);
  tx.signatures.resize(1);
  ASSERT_TRUE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {external.pub}));
  EXPECT_FALSE(contract_call_signed(tx));
  const crypto::hash old_tx_hash = get_transaction_hash(tx);
  crypto::hash digest{};
  crypto::hash ring_hash{};
  ASSERT_TRUE(contract_call_signature_hash(tx, MAINNET, digest));
  ASSERT_TRUE(get_contract_ring_hash(tx, ring_hash, HF_VERSION_QWC_CONTRACTS));
  const size_t old_size = tx_to_blob(tx).size();
  crypto::signature signature{};
  crypto::generate_signature(digest, external.pub, external.sec, signature);
  const std::string digest_hex = epee::string_tools::pod_to_hex(digest);
  const std::string signature_hex = epee::string_tools::pod_to_hex(signature);
  EXPECT_EQ(64u, digest_hex.size());
  EXPECT_EQ(128u, signature_hex.size());
  EXPECT_EQ(std::string::npos, digest_hex.find_first_not_of("0123456789abcdef"));
  EXPECT_EQ(std::string::npos, signature_hex.find_first_not_of("0123456789abcdef"));
  crypto::hash decoded_digest{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(digest_hex, decoded_digest));
  EXPECT_EQ(digest, decoded_digest);
  crypto::signature decoded{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(signature_hex, decoded));
  EXPECT_EQ(0, memcmp(&signature, &decoded, sizeof(signature)));
  tools::wallet2::pending_tx ptx{};
  ptx.tx = tx;
  tools::wallet2::attach_contract_signatures(ptx, MAINNET, {{external.pub, decoded}});
  tx = ptx.tx;
  EXPECT_TRUE(contract_call_signed(tx));
  EXPECT_NE(old_tx_hash, get_transaction_hash(tx));
  EXPECT_EQ(old_size, tx_to_blob(tx).size());
  crypto::hash after{};
  ASSERT_TRUE(contract_call_signature_hash(tx, MAINNET, after));
  EXPECT_EQ(digest, after);
  ASSERT_TRUE(get_contract_ring_hash(tx, after, HF_VERSION_QWC_CONTRACTS));
  EXPECT_EQ(ring_hash, after);
  expect_both(f, tx, contract_call_verdict::executed);
}

TEST(contract_addsig, external_signature_wrong_key_is_rejected)
{
  fixture f;
  const keypair external = keypair::generate(hw::get_device("default"));
  const keypair other = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(external.pub);
  call.signers.push_back(external.pub);
  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  call.signatures.emplace_back();
  transaction tx = make_tx(call);
  crypto::hash digest{};
  ASSERT_TRUE(contract_call_signature_hash(tx, MAINNET, digest));
  crypto::signature wrong{};
  crypto::generate_signature(digest, other.pub, other.sec, wrong);
  const crypto::signature wrong_key = wrong;
  const auto original_extra = tx.extra;
  EXPECT_EQ(contract_signature_attach_result::bad_signature, attach_contract_signature(tx, MAINNET, external.pub, wrong));
  EXPECT_EQ(contract_signature_attach_result::not_pending, attach_contract_signature(tx, MAINNET, other.pub, wrong));
  EXPECT_EQ(original_extra, tx.extra);
  tools::wallet2::pending_tx ptx{};
  ptx.tx = tx;
  EXPECT_THROW(tools::wallet2::attach_contract_signatures(ptx, MAINNET, {{external.pub, wrong}}), tools::error::wallet_internal_error);
  EXPECT_EQ(original_extra, ptx.tx.extra);
  crypto::generate_signature(digest, external.pub, external.sec, call.signatures.front().value);
  crypto::signature valid = call.signatures.front().value;
  EXPECT_EQ(contract_signature_attach_result::attached, attach_contract_signature(tx, MAINNET, external.pub, valid));
  const auto filled_extra = tx.extra;
  EXPECT_EQ(contract_signature_attach_result::not_pending, attach_contract_signature(tx, MAINNET, external.pub, valid));
  EXPECT_EQ(filled_extra, tx.extra);
  ptx.tx = tx;
  try { tools::wallet2::attach_contract_signatures(ptx, MAINNET, {{external.pub, valid}}); FAIL(); }
  catch (const tools::error::wallet_internal_error& e) { EXPECT_NE(std::string::npos, std::string(e.what()).find("Contract signer is not pending")); }
  crypto::hash different = crypto::cn_fast_hash("different", 9);
  crypto::generate_signature(different, external.pub, external.sec, wrong);
  transaction unsigned_tx = make_tx(f.call_for(external.pub));
  tx_extra_vm_call unsigned_call = call;
  unsigned_call.signatures.front().value = crypto::signature{};
  replace_call(unsigned_tx, unsigned_call);
  const auto unsigned_extra = unsigned_tx.extra;
  EXPECT_EQ(contract_signature_attach_result::bad_signature, attach_contract_signature(unsigned_tx, MAINNET, external.pub, wrong));
  EXPECT_EQ(unsigned_extra, unsigned_tx.extra);
  transaction pending = make_tx(f.call_for(external.pub));
  replace_call(pending, call);
  expect_both(f, pending, contract_call_verdict::executed);
  call.signatures.front().value = wrong_key;
  replace_call(pending, call);
  expect_both(f, pending, contract_call_verdict::invalid_signature);
  ptx.tx = unsigned_tx;
  const auto before_two = tx_to_blob(ptx.tx);
  EXPECT_THROW(tools::wallet2::attach_contract_signatures(ptx, MAINNET, {{external.pub, valid}, {external.pub, wrong}}), tools::error::wallet_internal_error);
  EXPECT_EQ(before_two, tx_to_blob(ptx.tx));
}

TEST(contract_addsig, external_signature_entry_points_round_trip)
{
  fixture f;
  account_base account;
  account.generate();
  const keypair external = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(external.pub);
  call.signers.push_back(external.pub);
  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  call.signatures.emplace_back();
  transaction tx = make_tx(call);
  tx.version = 1;
  txin_to_key input{};
  input.amount = fee;
  tx.vin.push_back(input);
  tx.signatures.resize(1);
  ASSERT_TRUE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {external.pub}));
  tools::wallet2::pending_tx ptx{};
  ptx.tx = tx;
  const std::string hex = tools::wallet2::pending_tx_to_hex(ptx);
  tools::wallet2::pending_tx decoded{};
  tools::wallet2::pending_tx_decode_error error{};
  ASSERT_TRUE(tools::wallet2::pending_tx_from_hex(hex, decoded, false, error));
  EXPECT_EQ(tools::wallet2::pending_tx_decode_error::none, error);
  EXPECT_EQ(get_transaction_hash(ptx.tx), get_transaction_hash(decoded.tx));
  const std::string digest_hex = tools::wallet2::contract_signature_hash_hex(decoded.tx, MAINNET);
  EXPECT_EQ(64u, digest_hex.size());
  EXPECT_EQ(std::string::npos, digest_hex.find_first_not_of("0123456789abcdef"));
  crypto::hash digest{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(digest_hex, digest));
  crypto::signature signature{};
  crypto::generate_signature(digest, external.pub, external.sec, signature);
  tools::wallet2::attach_contract_signatures_hex(decoded, MAINNET,
      {{epee::string_tools::pod_to_hex(external.pub), epee::string_tools::pod_to_hex(signature)}}, true);
  EXPECT_TRUE(contract_call_signatures_valid(decoded.tx, MAINNET));
  expect_both(f, decoded.tx, contract_call_verdict::executed);
  std::string prompt;
  tools::wallet2::collect_external_contract_signatures(ptx, MAINNET, {external.pub}, [&](const std::string& text) {
    prompt = text;
    return epee::string_tools::pod_to_hex(signature);
  });
  EXPECT_EQ("Contract signature for " + epee::string_tools::pod_to_hex(external.pub), prompt);
  EXPECT_TRUE(contract_call_signatures_valid(ptx.tx, MAINNET));
  expect_both(f, ptx.tx, contract_call_verdict::executed);
}

TEST(contract_addsig, pending_tx_hex_rejects_bad_input)
{
  tools::wallet2::pending_tx ptx{};
  tools::wallet2::pending_tx_decode_error error{};
  EXPECT_FALSE(tools::wallet2::pending_tx_from_hex("xyz", ptx, false, error));
  EXPECT_EQ(tools::wallet2::pending_tx_decode_error::bad_hex, error);
  EXPECT_FALSE(tools::wallet2::pending_tx_from_hex("deadbeef", ptx, false, error));
  EXPECT_EQ(tools::wallet2::pending_tx_decode_error::bad_metadata, error);
}

TEST(contract_addsig, external_signature_entry_points_reject_wrong_key)
{
  fixture f;
  account_base account;
  account.generate();
  const keypair external = keypair::generate(hw::get_device("default"));
  const keypair other = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(external.pub);
  call.signers.push_back(external.pub);
  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  call.signatures.emplace_back();
  transaction tx = make_tx(call);
  ASSERT_TRUE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {external.pub}));
  tools::wallet2::pending_tx ptx{};
  ptx.tx = tx;
  const auto original = tx_to_blob(ptx.tx);
  const std::string digest_hex = tools::wallet2::contract_signature_hash_hex(tx, MAINNET);
  crypto::hash digest{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(digest_hex, digest));
  crypto::signature wrong{};
  crypto::generate_signature(digest, other.pub, other.sec, wrong);
  const std::string key_hex = epee::string_tools::pod_to_hex(external.pub);
  const std::string wrong_hex = epee::string_tools::pod_to_hex(wrong);
  EXPECT_THROW(tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {{key_hex, wrong_hex}}, true), tools::error::wallet_internal_error);
  EXPECT_EQ(original, tx_to_blob(ptx.tx));
  EXPECT_THROW(tools::wallet2::collect_external_contract_signatures(ptx, MAINNET, {external.pub},
      [&](const std::string&) { return wrong_hex; }), tools::error::wallet_internal_error);
  EXPECT_EQ(original, tx_to_blob(ptx.tx));
  expect_wallet_error([&] { tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {}, true); },
      "Invalid contract signature count");
  std::vector<std::pair<std::string, std::string>> too_many(vm::limits::signers + 1, {key_hex, wrong_hex});
  expect_wallet_error([&] { tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, too_many, true); },
      "Invalid contract signature count");
  for (const std::string& bad : {wrong_hex.substr(1), wrong_hex + "0", std::string(128, 'z')})
    EXPECT_THROW(tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {{key_hex, bad}}, true), tools::error::invalid_contract_signature_encoding);
  EXPECT_THROW(tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {{std::string(64, 'f'), wrong_hex}}, true), tools::error::invalid_contract_signature_encoding);
  EXPECT_EQ(original, tx_to_blob(ptx.tx));
}

TEST(contract_addsig, complete_gate_verifies_existing_slots)
{
  fixture f(2);
  account_base account;
  account.generate();
  const keypair external = keypair::generate(hw::get_device("default"));
  const keypair other = keypair::generate(hw::get_device("default"));
  crypto::public_key derived{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(account.get_keys(), f.cid, crypto::null_hash, derived, secret));
  tx_extra_vm_call call = f.call_for(derived);
  call.args.append(reinterpret_cast<const char*>(&external.pub), sizeof(external.pub));
  for (const crypto::public_key& key : {derived, external.pub})
  {
    call.signers.push_back(key);
    call.signer_contracts.push_back(f.cid);
    call.signer_methods.push_back(call.method);
    call.signer_key_ids.push_back(crypto::null_hash);
    call.signer_nested.push_back(0);
    call.signatures.emplace_back();
  }
  transaction tx = make_tx(call);
  ASSERT_TRUE(sign_contract_call(tx, account.get_keys(), MAINNET, fee, {external.pub}));
  crypto::hash digest{};
  ASSERT_TRUE(contract_call_signature_hash(tx, MAINNET, digest));
  crypto::generate_signature(digest, other.pub, other.sec, call.signatures.front().value);
  replace_call(tx, call);
  crypto::signature valid{};
  crypto::generate_signature(digest, external.pub, external.sec, valid);
  tools::wallet2::pending_tx ptx{};
  ptx.tx = tx;
  const auto before = tx_to_blob(tx);
  try
  {
    tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET,
        {{epee::string_tools::pod_to_hex(external.pub), epee::string_tools::pod_to_hex(valid)}}, true);
    FAIL();
  }
  catch (const tools::error::wallet_internal_error& e)
  {
    EXPECT_NE(std::string::npos, std::string(e.what()).find("Contract call still lacks signatures"));
  }
  EXPECT_EQ(before, tx_to_blob(ptx.tx));
  ASSERT_EQ(contract_signature_attach_result::attached, attach_contract_signature(tx, MAINNET, external.pub, valid));
  EXPECT_TRUE(contract_call_signed(tx));
  EXPECT_FALSE(contract_call_signatures_valid(tx, MAINNET));
}

TEST(contract_addsig, commit_rejects_incomplete_and_wrong_signatures_offline)
{
  fixture f;
  account_base account;
  account.generate();
  const keypair external = keypair::generate(hw::get_device("default"));
  const keypair other = keypair::generate(hw::get_device("default"));
  tx_extra_vm_call call = f.call_for(external.pub);
  call.signers.push_back(external.pub);
  call.signer_contracts.push_back(f.cid);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  call.signatures.emplace_back();
  tools::wallet2::pending_tx ptx{};
  ptx.tx = make_tx(call);
  ASSERT_TRUE(sign_contract_call(ptx.tx, account.get_keys(), MAINNET, fee, {external.pub}));
  const epee::wipeable_string password("contract wallet test");
  tools::wallet2 wallet;
  wallet.generate("", password);
  const auto expect_rejected = [&] {
    try { wallet.commit_tx(ptx); FAIL(); }
    catch (const tools::error::wallet_internal_error& e)
    {
      EXPECT_STREQ("Contract call still lacks signatures", e.what());
    }
  };
  expect_rejected();
  crypto::hash digest{};
  ASSERT_TRUE(contract_call_signature_hash(ptx.tx, MAINNET, digest));
  crypto::generate_signature(digest, other.pub, other.sec, call.signatures.front().value);
  replace_call(ptx.tx, call);
  expect_rejected();
}

TEST(contract_addsig, external_signers_require_spendable_wallet)
{
  account_base account;
  account.generate();
  const epee::wipeable_string password("contract wallet test");
  tools::wallet2 normal;
  normal.generate("", password);
  EXPECT_TRUE(tools::wallet2::external_contract_signers_allowed(normal));
  tools::wallet2 watch;
  watch.generate("", password, account.get_keys().m_account_address, account.get_keys().m_view_secret_key);
  EXPECT_FALSE(tools::wallet2::external_contract_signers_allowed(watch));
  std::array<tools::wallet2, 2> multisig_wallets;
  make_two_of_two_multisig_wallets(multisig_wallets);
  bool ready = false;
  uint32_t threshold = 0;
  uint32_t total = 0;
  ASSERT_TRUE(multisig_wallets.front().multisig(&ready, &threshold, &total));
  EXPECT_TRUE(ready);
  EXPECT_EQ(2u, threshold);
  EXPECT_EQ(2u, total);
  EXPECT_FALSE(tools::wallet2::external_contract_signers_allowed(multisig_wallets.front()));
}

TEST(contract_addsig, external_signers_reject_cold_signing_wallet)
{
  struct cold_signing_device : hw::core::device_default
  {
    bool has_tx_cold_sign() const override { return true; }
  } device;
  const epee::wipeable_string password("contract wallet test");
  tools::wallet2 wallet;
  wallet.generate("", password);
  wallet.get_account().set_device(device);
  EXPECT_FALSE(tools::wallet2::external_contract_signers_allowed(wallet));
}

TEST(contract_addsig, regtest_wallet_network_selection)
{
  const auto parse = [](const std::vector<std::string>& args) {
    boost::program_options::options_description options("wallet");
    tools::wallet2::init_options(options);
    boost::program_options::variables_map values;
    boost::program_options::store(boost::program_options::command_line_parser(args).options(options).run(), values);
    boost::program_options::notify(values);
    return values;
  };
  const auto regtest = parse({"--regtest"});
  EXPECT_TRUE(tools::wallet2::network_selection_valid(regtest));
  EXPECT_EQ(cryptonote::FAKECHAIN, tools::wallet2::network_type_from_options(regtest));
  EXPECT_FALSE(tools::wallet2::network_selection_valid(parse({"--regtest", "--testnet"})));
  EXPECT_FALSE(tools::wallet2::network_selection_valid(parse({"--regtest", "--stagenet"})));
  EXPECT_FALSE(tools::wallet2::network_selection_valid(parse({"--regtest", "--devnet"})));
  const auto ringdb_base = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("wallet-regtest-%%%%%%%%");
  boost::filesystem::create_directories(ringdb_base);
  auto cleanup = epee::misc_utils::create_scope_leave_handler([&] {
    boost::system::error_code error;
    boost::filesystem::remove_all(ringdb_base, error);
  });
  const auto wallet = tools::wallet2::make_dummy(
      parse({"--regtest", "--daemon-ssl=disabled", "--shared-ringdb-dir=" + ringdb_base.string()}), true, {});
  ASSERT_TRUE(wallet);
  EXPECT_EQ((ringdb_base / "regtest").string(), wallet->get_ring_database());
}

TEST(contract_addsig, partial_attach_keeps_call_pending)
{
  fixture f(2);
  const keypair first = keypair::generate(hw::get_device("default"));
  const keypair second = keypair::generate(hw::get_device("default"));
  tools::wallet2::pending_tx ptx = external_pending(f, {first.pub, second.pub});
  const std::string digest = tools::wallet2::contract_signature_hash_hex(ptx.tx, MAINNET);
  const auto first_entry = hex_signature(ptx, first);
  const auto second_entry = hex_signature(ptx, second);
  tools::wallet2::pending_tx first_only = ptx;
  expect_wallet_error([&] { tools::wallet2::attach_contract_signatures_hex(first_only, MAINNET, {first_entry}, true); },
      "Contract call still lacks signatures");
  tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {first_entry}, false);
  EXPECT_FALSE(contract_call_signatures_valid(ptx.tx, MAINNET));
  tx_extra_vm_call call{};
  ASSERT_EQ(1u, get_tx_vm_call(ptx.tx.extra, call));
  const crypto::signature empty{};
  EXPECT_NE(0, memcmp(&call.signatures[0].value, &empty, sizeof(empty)));
  EXPECT_EQ(0, memcmp(&call.signatures[1].value, &empty, sizeof(empty)));
  EXPECT_EQ(digest, tools::wallet2::contract_signature_hash_hex(ptx.tx, MAINNET));
  tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {second_entry}, true);
  EXPECT_TRUE(contract_call_signatures_valid(ptx.tx, MAINNET));
  const epee::wipeable_string password("contract wallet test");
  tools::wallet2 wallet;
  wallet.generate("", password);
  EXPECT_THROW(wallet.commit_tx(ptx), tools::error::no_connection_to_daemon);
}

TEST(contract_addsig, attach_and_commit_reject_malformed_call)
{
  fixture f(2);
  const keypair first = keypair::generate(hw::get_device("default"));
  const keypair second = keypair::generate(hw::get_device("default"));
  const tools::wallet2::pending_tx unsigned_ptx = external_pending(f, {first.pub, second.pub});
  const auto first_entry = hex_signature(unsigned_ptx, first);
  crypto::signature first_signature{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(first_entry.second, first_signature));
  tx_extra_vm_call original{};
  ASSERT_EQ(1u, get_tx_vm_call(unsigned_ptx.tx.extra, original));
  const epee::wipeable_string password("contract wallet test");
  tools::wallet2 wallet;
  wallet.generate("", password);
  for (size_t variant = 0; variant < 4; ++variant)
  {
    tx_extra_vm_call call = original;
    if (variant == 0)
      call.signatures.pop_back();
    else if (variant == 1)
    {
      call.signer_key_ids.pop_back();
      tools::wallet2::pending_tx signing_ptx = unsigned_ptx;
      replace_call(signing_ptx.tx, call);
      const auto first_valid = hex_signature(signing_ptx, first);
      const auto second_valid = hex_signature(signing_ptx, second);
      ASSERT_TRUE(epee::string_tools::hex_to_pod(first_valid.second, call.signatures[0].value));
      ASSERT_TRUE(epee::string_tools::hex_to_pod(second_valid.second, call.signatures[1].value));
    }
    else if (variant == 2)
      call.signers[1] = first.pub;
    else
      call.version = 2;
    tools::wallet2::pending_tx ptx = unsigned_ptx;
    replace_call(ptx.tx, call);
    EXPECT_EQ(contract_signature_attach_result::malformed_call,
        attach_contract_signature(ptx.tx, MAINNET, first.pub, first_signature));
    EXPECT_FALSE(contract_call_signatures_valid(ptx.tx, MAINNET));
    expect_wallet_error([&] { tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET, {first_entry}, false); },
        "Contract signer is not pending");
    expect_wallet_error([&] { wallet.commit_tx(ptx); }, "Contract call still lacks signatures");
  }
}

TEST(contract_addsig, sign_rejects_foreign_or_filled_external_slot)
{
  fixture f;
  account_base account;
  account.generate();
  const keypair foreign = keypair::generate(hw::get_device("default"));
  crypto::public_key derived{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(account.get_keys(), f.cid, crypto::null_hash, derived, secret));
  tools::wallet2::pending_tx ptx = external_pending(f, {derived});
  EXPECT_FALSE(sign_contract_call(ptx.tx, account.get_keys(), MAINNET, fee, {foreign.pub}));
  tx_extra_vm_call call{};
  ASSERT_EQ(1u, get_tx_vm_call(ptx.tx.extra, call));
  const auto entry = hex_signature(ptx, keypair{derived, secret});
  ASSERT_TRUE(epee::string_tools::hex_to_pod(entry.second, call.signatures[0].value));
  replace_call(ptx.tx, call);
  EXPECT_FALSE(sign_contract_call(ptx.tx, account.get_keys(), MAINNET, fee, {derived}));
}

TEST(contract_addsig, non_canonical_signatures_are_rejected)
{
  fixture f;
  const keypair signer = keypair::generate(hw::get_device("default"));
  const tools::wallet2::pending_tx unsigned_ptx = external_pending(f, {signer.pub});
  const auto entry = hex_signature(unsigned_ptx, signer);
  crypto::signature valid{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(entry.second, valid));
  constexpr std::array<uint8_t, 32> order = {
      0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
      0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0x10};
  for (size_t variant = 0; variant < 3; ++variant)
  {
    SCOPED_TRACE(variant);
    crypto::signature altered = valid;
    if (variant == 0)
      memset(&altered.c, 0, sizeof(altered.c));
    else if (variant == 1)
      memcpy(&altered.c, order.data(), order.size());
    else
    {
      auto* bytes = reinterpret_cast<uint8_t*>(&altered.r);
      uint16_t carry = 0;
      for (size_t i = 0; i < order.size(); ++i)
      {
        const uint16_t sum = static_cast<uint16_t>(bytes[i]) + order[i] + carry;
        bytes[i] = static_cast<uint8_t>(sum);
        carry = sum >> 8;
      }
      ASSERT_EQ(0u, carry);
    }
    tools::wallet2::pending_tx ptx = unsigned_ptx;
    expect_wallet_error([&] { tools::wallet2::attach_contract_signatures_hex(ptx, MAINNET,
        {{entry.first, epee::string_tools::pod_to_hex(altered)}}, true); }, "Contract signature does not verify");
    tx_extra_vm_call call{};
    ASSERT_EQ(1u, get_tx_vm_call(ptx.tx.extra, call));
    call.signatures[0].value = altered;
    replace_call(ptx.tx, call);
    EXPECT_FALSE(contract_call_signatures_valid(ptx.tx, MAINNET));
  }
}

TEST(contract_addsig, signature_hash_known_answer)
{
  crypto::hash contract_id{};
  crypto::public_key signer{};
  crypto::key_image image{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(std::string(64, '1'), contract_id));
  ASSERT_TRUE(epee::string_tools::hex_to_pod("5866666666666666666666666666666666666666666666666666666666666666", signer));
  ASSERT_TRUE(epee::string_tools::hex_to_pod(std::string(64, '2'), image));
  tx_extra_vm_call call{};
  call.kind = tx_extra_vm_call::kind_invoke;
  call.contract_id = contract_id;
  call.method = 0x01020304;
  call.args = "fixed";
  call.gas_limit = 120000;
  call.gas_price = 3;
  call.signers = {signer};
  call.signer_contracts = {contract_id};
  call.signer_methods = {call.method};
  call.signer_key_ids = {crypto::null_hash};
  call.signer_nested = {0};
  call.signatures.resize(1);
  transaction tx = make_tx(call);
  tx.rct_signatures.type = rct::RCTTypeNull;
  tx.rct_signatures.txnFee = 1000000;
  tx.unlock_time = 17;
  txin_to_key input{};
  input.amount = 1000040;
  input.key_offsets = {5, 11};
  input.k_image = image;
  tx.vin.push_back(input);
  tx_out output{};
  set_tx_out(40, signer, false, crypto::view_tag{}, output);
  tx.vout.push_back(output);
  EXPECT_EQ(1000000u, get_tx_fee(tx));
  crypto::hash prefix{};
  crypto::hash digest{};
  ASSERT_TRUE(get_contract_signing_prefix_hash(tx, prefix));
  ASSERT_TRUE(get_contract_signature_hash(tx, MAINNET, 1000000, contract_id, digest));
  EXPECT_EQ("a3e28b495271c815e4f1dc1df9ddbe375d18c02c522a8d25d0467315204e3423", epee::string_tools::pod_to_hex(prefix));
  EXPECT_EQ("9af435fac172ab29a07d5d25e120b018699b9ee9f048cd5d8086776aabde6897", epee::string_tools::pod_to_hex(digest));
}
