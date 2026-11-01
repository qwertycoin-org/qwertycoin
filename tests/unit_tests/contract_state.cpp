// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <limits>
#include <boost/optional/optional_io.hpp>

#include "gtest/gtest.h"

#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "cryptonote_core/contract_state.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_core/tx_pool.h"
#include "device/device.hpp"
#include "vm_test_utils.h"

namespace
{
  using namespace cryptonote;

  constexpr uint64_t gas = 1000000;
  constexpr uint64_t ample_fee = 1000000000;

  struct fixture
  {
    vm::state_tree base;
    contract_overlay overlay{base};
    contract_effects effects;
    contract_block_context context;
    std::string wasm = vm_test::test_contract();
    crypto::hash cid = vm::contract_id_for(wasm, {});
  };

  tx_extra_vm_call deploy(const fixture& f)
  {
    tx_extra_vm_call call{};
    call.kind = tx_extra_vm_call::kind_deploy;
    call.contract_id = crypto::null_hash;
    call.code = f.wasm;
    call.gas_limit = gas;
    call.gas_price = 1;
    return call;
  }

  tx_extra_vm_call invoke(const fixture& f, uint32_t method, const std::string& args = {}, uint64_t deposit = 0)
  {
    tx_extra_vm_call call = deploy(f);
    call.kind = tx_extra_vm_call::kind_invoke;
    call.contract_id = f.cid;
    call.code.clear();
    call.method = method;
    call.args = args;
    call.deposit = deposit;
    return call;
  }

  transaction tx_with(const std::vector<tx_extra_vm_call>& calls)
  {
    transaction tx;
    for (const tx_extra_vm_call& call : calls)
      EXPECT_TRUE(add_vm_call_to_tx_extra(tx.extra, call));
    return tx;
  }

  contract_call_verdict run(fixture& f, const tx_extra_vm_call& call, uint64_t fee = ample_fee)
  {
    return execute_contract_tx(tx_with({call}), fee, f.context, f.overlay, f.effects);
  }
}

TEST(contract_state, absent_and_malformed)
{
  fixture f;
  EXPECT_EQ(contract_call_verdict::absent, execute_contract_tx(tx_with({}), ample_fee, f.context, f.overlay, f.effects));
  EXPECT_EQ(contract_call_verdict::malformed, execute_contract_tx(tx_with({deploy(f), deploy(f)}), ample_fee, f.context, f.overlay, f.effects));
  tx_extra_vm_call bad_kind = deploy(f);
  bad_kind.kind = 7;
  EXPECT_EQ(contract_call_verdict::malformed, run(f, bad_kind));
  EXPECT_TRUE(f.overlay.writes().empty());
}

TEST(contract_state, shared_call_metadata)
{
  fixture f;
  tx_extra_vm_call call = deploy(f);
  EXPECT_EQ(f.cid, called_contract_id(call));
  call.kind = tx_extra_vm_call::kind_invoke;
  call.contract_id = crypto::cn_fast_hash("contract", 8);
  EXPECT_EQ(call.contract_id, called_contract_id(call));

  call.signers.push_back(keypair::generate(hw::get_device("default")).pub);
  call.signer_contracts.push_back(call.contract_id);
  call.signer_methods.push_back(7);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  EXPECT_FALSE(contract_signer_shape_valid(call, false));
  call.signatures.emplace_back();
  EXPECT_TRUE(contract_signer_shape_valid(call, false));

  vm::parent_header parent;
  parent.height = 9;
  parent.hash = crypto::cn_fast_hash("parent", 6);
  const vm::call request = make_contract_vm_call(call, 10, parent);
  EXPECT_EQ(vm::call_kind::invoke, request.kind);
  EXPECT_EQ(call.contract_id, request.contract_id);
  EXPECT_EQ(call.args, request.args);
  EXPECT_EQ(call.deposit, request.deposit);
  EXPECT_EQ(10u, request.height);
  EXPECT_EQ(parent.height, request.parent.height);
  EXPECT_EQ(parent.hash, request.parent.hash);
}

TEST(contract_state, stored_signer_changes_with_state)
{
  vm::state_tree state;
  const vm::result deployed = vm_test::deploy(state, vm_test::stored_signer_contract());
  ASSERT_EQ(vm::status::ok, deployed.code);
  const crypto::public_key original = keypair::generate(hw::get_device("default")).pub;
  const crypto::public_key replacement = keypair::generate(hw::get_device("default")).pub;
  ASSERT_EQ(vm::status::ok, vm_test::invoke(state, deployed.contract_id, 2, vm_test::stored_owner_args(original)).code);
  const vm::result first = vm_test::invoke(state, deployed.contract_id, 3, vm_test::stored_owner_lookup_args());
  ASSERT_EQ(vm::status::ok, first.code);
  ASSERT_EQ(1u, first.required_signers.size());
  EXPECT_EQ(original, first.required_signers.front());
  ASSERT_EQ(vm::status::ok, vm_test::invoke(state, deployed.contract_id, 2, vm_test::stored_owner_args(replacement)).code);
  const vm::result second = vm_test::invoke(state, deployed.contract_id, 3, vm_test::stored_owner_lookup_args());
  ASSERT_EQ(vm::status::ok, second.code);
  ASSERT_EQ(1u, second.required_signers.size());
  EXPECT_EQ(replacement, second.required_signers.front());
}

TEST(contract_state, signed_call_uses_stored_signer)
{
  vm::state_tree state;
  const std::string code = vm_test::stored_signer_contract();
  const crypto::hash contract_id = vm::contract_id_for(code, {});
  contract_block_context context{};
  context.network = FAKECHAIN;
  tx_extra_vm_call deployment{};
  deployment.kind = tx_extra_vm_call::kind_deploy;
  deployment.code = code;
  deployment.gas_limit = gas;
  deployment.gas_price = 1;
  contract_block_executor deployment_executor(state, context);
  ASSERT_EQ(contract_call_verdict::executed, deployment_executor.execute(tx_with({deployment}), ample_fee));
  state.apply(deployment_executor.writes());
  account_base account;
  account.generate();
  crypto::public_key signer{};
  crypto::secret_key secret{};
  ASSERT_TRUE(derive_contract_key(account.get_keys(), contract_id, crypto::null_hash, signer, secret));
  tx_extra_vm_call set_owner{};
  set_owner.kind = tx_extra_vm_call::kind_invoke;
  set_owner.contract_id = contract_id;
  set_owner.method = 2;
  set_owner.args = vm_test::stored_owner_args(signer);
  set_owner.gas_limit = gas;
  set_owner.gas_price = 1;
  contract_block_executor owner_executor(state, context);
  ASSERT_EQ(contract_call_verdict::executed, owner_executor.execute(tx_with({set_owner}), ample_fee));
  state.apply(owner_executor.writes());

  tx_extra_vm_call call{};
  call.kind = tx_extra_vm_call::kind_invoke;
  call.contract_id = contract_id;
  call.method = 3;
  call.args = vm_test::stored_owner_lookup_args();
  call.gas_limit = gas;
  call.gas_price = 1;
  call.signers.push_back(signer);
  call.signatures.emplace_back();
  call.signer_contracts.push_back(contract_id);
  call.signer_methods.push_back(call.method);
  call.signer_key_ids.push_back(crypto::null_hash);
  call.signer_nested.push_back(0);
  transaction tx;
  tx.version = 1;
  ASSERT_TRUE(add_vm_call_to_tx_extra(tx.extra, call));
  ASSERT_TRUE(sign_contract_call(tx, account.get_keys(), FAKECHAIN, ample_fee));

  contract_overlay original_overlay(state);
  contract_effects original_effects{};
  EXPECT_EQ(contract_call_verdict::executed,
      execute_contract_tx(tx, ample_fee, context, original_overlay, original_effects));

  const crypto::public_key replacement = keypair::generate(hw::get_device("default")).pub;
  set_owner.args = vm_test::stored_owner_args(replacement);
  contract_block_executor replacement_executor(state, context);
  ASSERT_EQ(contract_call_verdict::executed, replacement_executor.execute(tx_with({set_owner}), ample_fee));
  state.apply(replacement_executor.writes());
  contract_overlay changed_overlay(state);
  contract_effects changed_effects{};
  EXPECT_EQ(contract_call_verdict::signer_mismatch,
      execute_contract_tx(tx, ample_fee, context, changed_overlay, changed_effects));
}

TEST(contract_state, peer_offense_tracks_state_dependent_verdicts)
{
  for (contract_call_verdict verdict : {
      contract_call_verdict::malformed, contract_call_verdict::underpaid,
      contract_call_verdict::over_tx_gas_cap,
      contract_call_verdict::invalid_signature})
    EXPECT_FALSE(contract_call_verdict_can_change_with_chain(verdict));
  for (contract_call_verdict verdict : {
      contract_call_verdict::failed, contract_call_verdict::signer_mismatch,
      contract_call_verdict::over_block_gas_limit,
      contract_call_verdict::over_block_mint_limit, contract_call_verdict::unbalanced_funds})
    EXPECT_TRUE(contract_call_verdict_can_change_with_chain(verdict));
}

TEST(contract_state, deploy_then_invoke_writes_overlay_only)
{
  fixture f;
  ASSERT_EQ(contract_call_verdict::executed, run(f, deploy(f)));
  ASSERT_EQ(contract_call_verdict::executed, run(f, invoke(f, vm_test::m_store, "k1v1")));
  std::string value;
  ASSERT_TRUE(f.overlay.get(vm_test::var_key(f.cid, 0, "k1"), value));
  EXPECT_EQ("v1", value);
  EXPECT_FALSE(f.base.get(vm_test::var_key(f.cid, 0, "k1"), value));
  EXPECT_GT(f.effects.gas_used, 0u);
  EXPECT_EQ(0u, f.effects.deposited);
  EXPECT_TRUE(f.effects.mints.empty());
}

TEST(contract_state, fee_must_cover_deposit_and_gas)
{
  fixture f;
  EXPECT_EQ(contract_call_verdict::underpaid, run(f, deploy(f), gas - 1));
  tx_extra_vm_call overflow = deploy(f);
  overflow.gas_price = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(contract_call_verdict::underpaid, run(f, overflow));
  tx_extra_vm_call with_deposit = deploy(f);
  with_deposit.deposit = 10;
  EXPECT_EQ(contract_call_verdict::underpaid, run(f, with_deposit, gas + 9));
  EXPECT_TRUE(f.overlay.writes().empty());
}

TEST(contract_state, fee_split_leaves_miner_remainder)
{
  tx_extra_vm_call call{};
  call.gas_limit = 100;
  call.gas_price = 3;
  call.deposit = 50;
  contract_call_fee split{};
  ASSERT_TRUE(split_contract_call_fee(call, 360, split));
  EXPECT_EQ(50u, split.deposit);
  EXPECT_EQ(3u, split.gas_price);
  EXPECT_EQ(10u, split.remainder);
  ASSERT_TRUE(split_contract_call_fee(call, 350, split));
  EXPECT_EQ(0u, split.remainder);
  EXPECT_FALSE(split_contract_call_fee(call, 349, split));
  call.deposit = 400;
  EXPECT_FALSE(split_contract_call_fee(call, 360, split));
  call.deposit = 0;
  call.gas_price = std::numeric_limits<uint64_t>::max();
  EXPECT_FALSE(split_contract_call_fee(call, std::numeric_limits<uint64_t>::max(), split));
}

TEST(contract_state, pool_orders_by_fee_then_gas_price)
{
  const crypto::hash id_a = crypto::cn_fast_hash("a", 1), id_b = crypto::cn_fast_hash("b", 1);
  std::set<tx_by_fee_and_receive_time_entry, txCompare> order;
  order.emplace(tx_pool_order_key(2.0, 1, 10), id_a);
  order.emplace(tx_pool_order_key(2.0, 5, 20), id_b);
  order.emplace(tx_pool_order_key(3.0, 0, 30), id_a);
  order.emplace(tx_pool_order_key(1.0, 9, 0), id_b);
  std::vector<uint64_t> gas_prices;
  for (const auto& entry : order)
    gas_prices.push_back(std::get<1>(entry.first));
  EXPECT_EQ((std::vector<uint64_t>{0, 5, 1, 9}), gas_prices);
}

TEST(contract_state, gas_caps)
{
  fixture f;
  tx_extra_vm_call over_cap = deploy(f);
  over_cap.gas_limit = QWC_VM_TX_GAS_CAP + 1;
  EXPECT_EQ(contract_call_verdict::over_tx_gas_cap, run(f, over_cap, std::numeric_limits<uint64_t>::max()));
  f.effects.gas_used = QWC_VM_BLOCK_GAS_LIMIT - gas + 1;
  EXPECT_EQ(contract_call_verdict::over_block_gas_limit, run(f, deploy(f)));
  f.effects.gas_used = QWC_VM_BLOCK_GAS_LIMIT - gas;
  EXPECT_EQ(contract_call_verdict::executed, run(f, deploy(f)));
}

TEST(contract_state, failing_call_leaves_state_untouched)
{
  fixture f;
  ASSERT_EQ(contract_call_verdict::executed, run(f, deploy(f)));
  const vm::write_set before = f.overlay.writes();
  const uint64_t gas_before = f.effects.gas_used;
  EXPECT_EQ(contract_call_verdict::failed, run(f, invoke(f, vm_test::m_unreachable)));
  EXPECT_EQ(contract_call_verdict::failed, run(f, invoke(f, vm_test::m_spin)));
  tx_extra_vm_call missing = invoke(f, vm_test::m_store, "k1v1");
  missing.contract_id = crypto::null_hash;
  EXPECT_EQ(contract_call_verdict::failed, run(f, missing));
  EXPECT_EQ(before, f.overlay.writes());
  EXPECT_EQ(gas_before, f.effects.gas_used);
}

TEST(contract_state, locked_funds_must_match_deposit)
{
  fixture f;
  ASSERT_EQ(contract_call_verdict::executed, run(f, deploy(f)));
  EXPECT_EQ(contract_call_verdict::failed, run(f, invoke(f, vm_test::m_funds_lock, {}, vm_test::locked_per_call - 1)));
  EXPECT_EQ(contract_call_verdict::unbalanced_funds, run(f, invoke(f, vm_test::m_funds_lock, {}, vm_test::locked_per_call + 1)));
  EXPECT_EQ(contract_call_verdict::unbalanced_funds, run(f, invoke(f, vm_test::m_store, "k1v1", 5)));
  ASSERT_EQ(contract_call_verdict::executed, run(f, invoke(f, vm_test::m_funds_lock, {}, vm_test::locked_per_call)));
  EXPECT_EQ(static_cast<uint64_t>(vm_test::locked_per_call), f.effects.deposited);
  EXPECT_EQ(0u, f.effects.withdrawn);
}

TEST(contract_state, overlay_deletion_hides_base)
{
  fixture f;
  f.base.set("k", "v");
  vm::write_set writes;
  writes.emplace("k", boost::none);
  f.overlay.apply(writes);
  std::string value;
  EXPECT_FALSE(f.overlay.get("k", value));
  EXPECT_TRUE(f.base.get("k", value));
  const vm::write_set undo = vm::undo_for(f.base, f.overlay.writes());
  ASSERT_EQ(1u, undo.size());
  EXPECT_EQ(std::string("v"), *undo.at("k"));
}

namespace
{
  contract_mint random_mint(uint64_t amount)
  {
    contract_mint mint;
    mint.amount = amount;
    mint.destination.key = keypair::generate(hw::get_device("default")).pub;
    mint.destination.view_tag.data = static_cast<char>(amount);
    mint.destination.tx_key = keypair::generate(hw::get_device("default")).pub;
    return mint;
  }

  transaction miner_tx_with_outputs(size_t outputs)
  {
    transaction tx;
    add_tx_pub_key_to_extra(tx, keypair::generate(hw::get_device("default")).pub);
    for (size_t i = 0; i < outputs; ++i)
    {
      tx_out out;
      set_tx_out(1000 + i, keypair::generate(hw::get_device("default")).pub, true, crypto::view_tag{}, out);
      tx.vout.push_back(out);
    }
    return tx;
  }
}

TEST(contract_state, unlocks_mint_up_to_block_cap)
{
  fixture f;
  ASSERT_EQ(contract_call_verdict::executed, run(f, deploy(f)));
  for (size_t i = 0; i <= QWC_VM_MAX_MINTS_PER_BLOCK; ++i)
    ASSERT_EQ(contract_call_verdict::executed, run(f, invoke(f, vm_test::m_funds_lock, {}, vm_test::locked_per_call)));
  f.effects = {};

  tx_extra_vm_call unlock = invoke(f, vm_test::m_funds_unlock);
  EXPECT_EQ(contract_call_verdict::unbalanced_funds, run(f, unlock));
  unlock.mint.push_back(random_mint(0).destination);
  for (size_t i = 0; i < QWC_VM_MAX_MINTS_PER_BLOCK; ++i)
    ASSERT_EQ(contract_call_verdict::executed, run(f, unlock));
  EXPECT_EQ(contract_call_verdict::over_block_mint_limit, run(f, unlock));
  ASSERT_EQ(QWC_VM_MAX_MINTS_PER_BLOCK, f.effects.mints.size());
  EXPECT_EQ(static_cast<uint64_t>(vm_test::locked_per_call), f.effects.mints.front().amount);
  EXPECT_EQ(unlock.mint.front().key, f.effects.mints.front().destination.key);
  EXPECT_EQ(QWC_VM_MAX_MINTS_PER_BLOCK * vm_test::locked_per_call, f.effects.withdrawn);
  EXPECT_EQ(0u, f.effects.deposited);
}

TEST(contract_state, unlock_beyond_locked_balance_fails)
{
  fixture f;
  ASSERT_EQ(contract_call_verdict::executed, run(f, deploy(f)));
  tx_extra_vm_call unlock = invoke(f, vm_test::m_funds_unlock);
  unlock.mint.push_back(random_mint(0).destination);
  EXPECT_EQ(contract_call_verdict::failed, run(f, unlock));
  EXPECT_TRUE(f.effects.mints.empty());
}

TEST(contract_state, miner_fee_excludes_deposit)
{
  fixture f;
  tx_extra_vm_call call = invoke(f, vm_test::m_funds_lock, {}, 7);
  const transaction tx = tx_with({call});
  EXPECT_EQ(3u, contract_miner_fee(tx, 10, HF_VERSION_QWC_CONTRACTS));
  EXPECT_EQ(10u, contract_miner_fee(tx, 10, HF_VERSION_QWC_EPOSE));
  EXPECT_EQ(10u, contract_miner_fee(tx_with({}), 10, HF_VERSION_QWC_CONTRACTS));
}

TEST(contract_state, miner_tx_mints_follow_reward_outputs)
{
  transaction tx = miner_tx_with_outputs(2);
  const crypto::public_key tx_key = get_tx_pub_key_from_extra(tx);
  EXPECT_TRUE(check_contract_mints_in_miner_tx(tx, {}));

  const std::vector<contract_mint> mints = {random_mint(5), random_mint(6)};
  ASSERT_TRUE(add_contract_mints_to_miner_tx(tx, mints));
  ASSERT_EQ(4u, tx.vout.size());
  const std::vector<crypto::public_key> output_keys = get_additional_tx_pub_keys_from_extra(tx);
  ASSERT_EQ(4u, output_keys.size());
  EXPECT_EQ(tx_key, output_keys[0]);
  EXPECT_EQ(tx_key, output_keys[1]);
  EXPECT_EQ(mints[1].destination.tx_key, output_keys[3]);
  EXPECT_EQ(6u, tx.vout[3].amount);
  EXPECT_TRUE(check_contract_mints_in_miner_tx(tx, mints));

  EXPECT_FALSE(check_contract_mints_in_miner_tx(tx, {}));
  EXPECT_FALSE(check_contract_mints_in_miner_tx(tx, {mints[1], mints[0]}));
  EXPECT_FALSE(check_contract_mints_in_miner_tx(tx, {mints[0]}));
  EXPECT_FALSE(check_contract_mints_in_miner_tx(miner_tx_with_outputs(2), mints));
  transaction underpaid = tx;
  underpaid.vout[2].amount -= 1;
  EXPECT_FALSE(check_contract_mints_in_miner_tx(underpaid, mints));

  transaction capped = miner_tx_with_outputs(1);
  EXPECT_FALSE(add_contract_mints_to_miner_tx(capped, std::vector<contract_mint>(QWC_VM_MAX_MINTS_PER_BLOCK + 1, random_mint(1))));
}

TEST(contract_state, call_fee_floor_is_deposit_plus_gas)
{
  tx_extra_vm_call call{};
  call.deposit = 7;
  call.gas_limit = 1000;
  call.gas_price = 3;
  uint64_t fee = 0;
  ASSERT_TRUE(contract_call_fee_floor(call, fee));
  EXPECT_EQ(3007u, fee);

  std::vector<uint8_t> extra;
  ASSERT_TRUE(contract_call_fee_floor(extra, fee));
  EXPECT_EQ(0u, fee);
  ASSERT_TRUE(add_vm_call_to_tx_extra(extra, call));
  ASSERT_TRUE(contract_call_fee_floor(extra, fee));
  EXPECT_EQ(3007u, fee);

  contract_call_fee split{};
  EXPECT_FALSE(split_contract_call_fee(call, 3006, split));
  ASSERT_TRUE(split_contract_call_fee(call, 3010, split));
  EXPECT_EQ(3u, split.remainder);

  call.deposit = std::numeric_limits<uint64_t>::max() - 2999;
  EXPECT_FALSE(contract_call_fee_floor(call, fee));
  call.deposit = 0;
  call.gas_limit = std::numeric_limits<uint64_t>::max();
  EXPECT_FALSE(contract_call_fee_floor(call, fee));
}

TEST(contract_state, gas_limit_adds_margin_within_cap)
{
  EXPECT_EQ(0u, contract_call_gas_limit(0));
  EXPECT_EQ(110u, contract_call_gas_limit(100));
  EXPECT_EQ(113u, contract_call_gas_limit(102));
  EXPECT_EQ(QWC_VM_TX_GAS_CAP, contract_call_gas_limit(QWC_VM_TX_GAS_CAP - 1));
  EXPECT_EQ(QWC_VM_TX_GAS_CAP, contract_call_gas_limit(std::numeric_limits<uint64_t>::max()));
}

TEST(contract_state, mint_destination_is_found_by_recipient)
{
  hw::device &hwdev = hw::get_device("default");
  account_base recipient;
  recipient.generate();
  const account_keys &keys = recipient.get_keys();
  const account_public_address subaddress = hwdev.get_subaddress(keys, {0, 1});

  for (const auto &target : {std::make_pair(keys.m_account_address, false), std::make_pair(subaddress, true)})
  {
    tx_extra_vm_mint_destination destination{};
    ASSERT_TRUE(make_contract_mint_destination(target.first, target.second, destination));
    crypto::key_derivation derivation;
    ASSERT_TRUE(crypto::generate_key_derivation(destination.tx_key, keys.m_view_secret_key, derivation));
    crypto::view_tag view_tag;
    crypto::derive_view_tag(derivation, QWC_MINT_DERIVATION_INDEX, view_tag);
    EXPECT_EQ(destination.view_tag, view_tag);
    crypto::public_key spend_key;
    ASSERT_TRUE(crypto::derive_subaddress_public_key(destination.key, derivation, QWC_MINT_DERIVATION_INDEX, spend_key));
    EXPECT_EQ(target.first.m_spend_public_key, spend_key);
  }
}
