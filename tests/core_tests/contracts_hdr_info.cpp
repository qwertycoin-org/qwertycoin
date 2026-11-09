#include "chaingen.h"
#include "contracts_hdr_info.h"
#include "cryptonote_core/blockchain.h"
#include "blockchain_test_access.h"
#include "../unit_tests/vm_test_utils.h"
#include <cstring>
#include <stdexcept>

using namespace cryptonote;

namespace
{
  std::string header_shader()
  {
    vm_test::module_builder module;
    const uint32_t header = module.import("get_HdrInfo", {vm_test::t_i32}, {vm_test::t_i32});
    const uint32_t save = module.import("SaveVar", {vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32}, {vm_test::t_i32});
    const uint32_t method = vm_test::method_type(module);
    vm_test::add_ctor_dtor(module, method);
    vm_test::code body;
    body.arg_at(0).call(header).drop();
    body.arg_at(80).i32(1).arg_at(8).i32(32).i32(0).call(save).drop().end();
    module.exp("Method_2", module.func(method, body));
    return module.build();
  }

  crypto::hash shader_id()
  {
    return vm::contract_id_for(header_shader(), {});
  }

  std::string header_args(uint64_t height)
  {
    std::string args(81, '\0');
    for (unsigned i = 0; i < 8; ++i)
      args[i] = static_cast<char>(height >> (8 * i));
    args[80] = 'h';
    return args;
  }

  tx_extra_vm_call header_deploy_call()
  {
    tx_extra_vm_call call{};
    call.kind = tx_extra_vm_call::kind_deploy;
    call.code = header_shader();
    call.gas_limit = QWC_VM_TX_GAS_CAP;
    call.gas_price = 1;
    return call;
  }

  tx_extra_vm_call header_invoke_call(uint64_t height)
  {
    tx_extra_vm_call call{};
    call.kind = tx_extra_vm_call::kind_invoke;
    call.contract_id = shader_id();
    call.method = 2;
    call.args = header_args(height);
    call.gas_limit = QWC_VM_TX_GAS_CAP;
    call.gas_price = 1;
    return call;
  }

  bool check_stored_hash(core& c, const crypto::hash& expected)
  {
    std::string value;
    const auto& db = c.get_blockchain_storage().get_db();
    return db.get_vm_state_value(vm_test::var_key(shader_id(), 0, "h"), value)
        && value.size() == sizeof(expected)
        && std::memcmp(value.data(), &expected, sizeof(expected)) == 0;
  }

  const block* latest_block(const std::vector<test_event_entry>& events, size_t index)
  {
    while (index > 0)
      if (const block* found = boost::get<block>(&events[--index]))
        return found;
    return nullptr;
  }

  const block* ancestor(const std::vector<test_event_entry>& events, const block& tip, unsigned depth)
  {
    const block* current = &tip;
    for (unsigned i = 0; i < depth; ++i)
    {
      const block* previous = nullptr;
      for (const test_event_entry& event : events)
        if (const block* candidate = boost::get<block>(&event))
          if (get_block_hash(*candidate) == current->prev_id)
          {
            previous = candidate;
            break;
          }
      if (!previous)
        return nullptr;
      current = previous;
    }
    return current;
  }
}

gen_contracts_hdr_info_reorg::gen_contracts_hdr_info_reorg()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_hdr_info_reorg, check_main);
  REGISTER_CALLBACK_METHOD(gen_contracts_hdr_info_reorg, check_alt);
}

bool gen_contracts_hdr_info_reorg::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  transaction deploy_tx;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, header_deploy_call(), deploy_tx, 0), false, "deploy tx");
  push_kept_txs(events, {deploy_tx});
  block shared;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy_tx}, shared), false, "shared deploy");
  events.push_back(shared);

  block main_parent;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, shared, chain.miner, fork_version, {}, main_parent), false, "main parent");
  events.push_back(main_parent);
  const uint64_t target = get_block_height(main_parent);
  block main_middle;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, main_parent, chain.miner, fork_version, {}, main_middle), false, "main middle");
  events.push_back(main_middle);
  transaction invoke_tx;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, header_invoke_call(target), invoke_tx, 1), false, "invoke tx");
  push_kept_txs(events, {invoke_tx});
  block main_store;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, main_middle, chain.miner, fork_version, {invoke_tx}, main_store), false, "main store");
  events.push_back(main_store);
  DO_CALLBACK(events, "check_main");

  account_base alt_miner;
  alt_miner.generate();
  block alt_parent;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, shared, alt_miner, fork_version, {}, alt_parent), false, "alt parent");
  events.push_back(alt_parent);
  CHECK_AND_ASSERT_MES(get_block_hash(alt_parent) != get_block_hash(main_parent), false, "branches must differ");
  block alt_middle;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, alt_parent, alt_miner, fork_version, {}, alt_middle), false, "alt middle");
  events.push_back(alt_middle);
  block alt_store;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, alt_middle, alt_miner, fork_version, {invoke_tx}, alt_store), false, "alt store");
  events.push_back(alt_store);
  block alt_tip;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, alt_store, alt_miner, fork_version, {}, alt_tip), false, "alt tip");
  events.push_back(alt_tip);
  DO_CALLBACK(events, "check_alt");
  return true;
}

bool gen_contracts_hdr_info_reorg::check_main(core& c, size_t index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_hdr_info_reorg::check_main");
  const block* tip = latest_block(events, index);
  CHECK_TEST_CONDITION(tip != nullptr);
  const block* parent = ancestor(events, *tip, 2);
  CHECK_TEST_CONDITION(parent != nullptr);
  CHECK_TEST_CONDITION(check_stored_hash(c, get_block_hash(*parent)));
  return true;
}

bool gen_contracts_hdr_info_reorg::check_alt(core& c, size_t index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_hdr_info_reorg::check_alt");
  const block* tip = latest_block(events, index);
  CHECK_TEST_CONDITION(tip != nullptr);
  const block* parent = ancestor(events, *tip, 3);
  CHECK_TEST_CONDITION(parent != nullptr);
  CHECK_EQ(get_block_hash(*tip), c.get_blockchain_storage().get_tail_id());
  CHECK_TEST_CONDITION(check_stored_hash(c, get_block_hash(*parent)));
  return true;
}

gen_contracts_hdr_info_retry::gen_contracts_hdr_info_retry()
{
  REGISTER_CALLBACK_METHOD(gen_contracts_hdr_info_retry, inject_failure);
  REGISTER_CALLBACK_METHOD(gen_contracts_hdr_info_retry, retry_block);
}

bool gen_contracts_hdr_info_retry::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");
  transaction deploy_tx;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, header_deploy_call(), deploy_tx, 0), false, "deploy tx");
  push_kept_txs(events, {deploy_tx});
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy_tx}, deployed), false, "deploy block");
  events.push_back(deployed);
  block parent;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, deployed, chain.miner, fork_version, {}, parent), false, "parent block");
  events.push_back(parent);

  transaction invoke_tx;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, header_invoke_call(get_block_height(deployed)), invoke_tx, 1), false, "invoke tx");
  push_kept_txs(events, {invoke_tx});
  block candidate;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, parent, chain.miner, fork_version, {invoke_tx}, candidate), false, "candidate block");
  DO_CALLBACK(events, "inject_failure");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(candidate);
  DO_CALLBACK(events, "retry_block");
  return true;
}

bool gen_contracts_hdr_info_retry::inject_failure(core& c, size_t, const std::vector<test_event_entry>&)
{
  blockchain_test_access::header_provider_override(c.get_blockchain_storage()) =
      [this](uint64_t, vm::parent_header&) -> bool {
        m_provider_called = true;
        throw std::runtime_error("header provider unavailable");
      };
  return true;
}

bool gen_contracts_hdr_info_retry::retry_block(core& c, size_t index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_contracts_hdr_info_retry::retry_block");
  const block* candidate = latest_block(events, index);
  const transaction* invoke_tx = find_contract_tx(events, 2);
  CHECK_TEST_CONDITION(candidate != nullptr && invoke_tx != nullptr);
  const block* target = ancestor(events, *candidate, 2);
  CHECK_TEST_CONDITION(target != nullptr);
  Blockchain& chain = c.get_blockchain_storage();
  const crypto::hash id = get_block_hash(*candidate);
  CHECK_TEST_CONDITION(m_provider_called);
  CHECK_EQ(candidate->prev_id, chain.get_tail_id());
  CHECK_TEST_CONDITION(!blockchain_test_access::invalid_block(chain, id));
  CHECK_TEST_CONDITION(!chain.have_block(id));
  const uint64_t height_before = chain.get_current_blockchain_height();
  blockchain_test_access::header_provider_override(chain) = {};
  block_verification_context bvc{};
  CHECK_TEST_CONDITION(handle_block_with_supplement(c, *candidate, *invoke_tx, false, bvc));
  CHECK_TEST_CONDITION(!bvc.m_verifivation_failed && !bvc.m_already_exists && bvc.m_added_to_main_chain);
  CHECK_EQ(height_before + 1, chain.get_current_blockchain_height());
  CHECK_EQ(id, chain.get_tail_id());
  CHECK_TEST_CONDITION(check_stored_hash(c, get_block_hash(*target)));
  return true;
}

bool gen_contracts_hdr_info_before_fork_rejected::generate(std::vector<test_event_entry>& events) const
{
  std::vector<uint8_t> extra;
  CHECK_AND_ASSERT_MES(add_vm_call_to_tx_extra(extra, header_deploy_call()), false, "header call extra");
  CHECK_AND_ASSERT_MES(generate_case(events, HF_VERSION_QWC_EPOSE, extra, false, false, false), false, "before fork");
  DO_CALLBACK(events, "check_pool_rejects_call");
  return true;
}
