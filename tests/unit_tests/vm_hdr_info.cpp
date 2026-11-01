#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

#include "gtest/gtest.h"
#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/contract_state.h"
#include "vm/gas.h"
#include "vm/hdr_info_hf18.h"
#include "vm_test_utils.h"

namespace
{
  std::string shader(uint64_t height)
  {
    vm_test::module_builder module;
    const uint32_t header = module.import("get_HdrInfo", {vm_test::t_i32}, {vm_test::t_i32});
    const uint32_t save = module.import("SaveVar", {vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32}, {vm_test::t_i32});
    const uint32_t method = vm_test::method_type(module);
    module.exp("Ctor", module.func(method, vm_test::code().end()));
    module.exp("Dtor", module.func(method, vm_test::code().end()));
    vm_test::code body;
    body.arg_at(0).i64(height).op(0x37).raw({0x03, 0x00});
    body.arg_at(81).arg_at(0).call(header).op(0x3a).raw({0x00, 0x00});
    body.arg_at(80).i32(1).arg_at(0).i32(82).i32(0).call(save).drop().end();
    module.exp("Method_2", module.func(method, body));
    return module.build();
  }

  vm::parent_header at(uint64_t height)
  {
    const std::string value = std::to_string(height);
    const std::string previous = std::to_string(height - 1);
    return {height, crypto::cn_fast_hash(value.data(), value.size()),
        crypto::cn_fast_hash(previous.data(), previous.size()), height * 10};
  }

  uint64_t read_le(const char* bytes)
  {
    uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i)
      result |= uint64_t(static_cast<uint8_t>(bytes[i])) << (8 * i);
    return result;
  }

  vm::result run(uint64_t requested, bool active, vm::state_tree& state, std::string& stored,
      uint64_t gas_limit = vm_test::ample_gas)
  {
    const std::string wasm = shader(requested);
    vm::call deploy_call;
    deploy_call.kind = vm::call_kind::deploy;
    const vm::result deployed = vm::execute(wasm, deploy_call, state, vm_test::ample_gas);
    if (deployed.code != vm::status::ok)
      return deployed;
    state.apply(deployed.writes);
    vm::call call;
    call.contract_id = deployed.contract_id;
    call.method = 2;
    call.height = 10;
    call.parent = at(9);
    call.hdr_info_active = active;
    call.headers = [](uint64_t height, vm::parent_header& header) {
      header = at(height);
      return true;
    };
    call.args = std::string(82, '\x5a');
    call.args[80] = 'k';
    const vm::result result = vm::execute({}, call, state, gas_limit);
    const auto found = result.writes.find(vm_test::var_key(call.contract_id, 0, "k"));
    if (found != result.writes.end() && found->second)
      stored = *found->second;
    return result;
  }
}

TEST(vm_hdr_info, parent_deep_ancestor_and_height_one)
{
  for (uint64_t height : {9u, 4u, 1u})
  {
    vm::state_tree state;
    std::string stored;
    const vm::result result = run(height, true, state, stored);
    ASSERT_EQ(vm::status::ok, result.code);
    ASSERT_EQ(82u, stored.size());
    EXPECT_EQ(1, static_cast<uint8_t>(stored[81]));
    const vm::parent_header expected = at(height);
    EXPECT_EQ(height, read_le(stored.data()));
    EXPECT_EQ(0, std::memcmp(stored.data() + 8, &expected.hash, 32));
    EXPECT_EQ(0, std::memcmp(stored.data() + 40, &expected.prev, 32));
    EXPECT_EQ(expected.timestamp, read_le(stored.data() + 72));
    EXPECT_GE(result.gas_used, vm::gas::host_call_cost(vm::host_call::get_HdrInfo));
  }
}

TEST(vm_hdr_info, invalid_heights_zero_the_header)
{
  for (uint64_t height : {uint64_t(0), uint64_t(10), uint64_t(11), std::numeric_limits<uint64_t>::max()})
  {
    vm::state_tree state;
    std::string stored;
    const vm::result result = run(height, true, state, stored);
    ASSERT_EQ(vm::status::ok, result.code);
    ASSERT_EQ(82u, stored.size());
    EXPECT_EQ(0, static_cast<uint8_t>(stored[81]));
    EXPECT_EQ(std::string(80, '\0'), stored.substr(0, 80));
  }
}

TEST(vm_hdr_info, preactivation_is_unknown_host_call)
{
  vm::state_tree state;
  std::string stored;
  EXPECT_EQ(vm::status::trap, run(4, false, state, stored).code);
}

TEST(vm_hdr_info, gas_limit_is_enforced)
{
  vm::state_tree state;
  std::string stored;
  const vm::result measured = run(4, true, state, stored);
  ASSERT_EQ(vm::status::ok, measured.code);
  EXPECT_EQ(15884u, measured.gas_used);
  vm::state_tree starved;
  std::string unused;
  EXPECT_EQ(vm::status::out_of_gas, run(4, true, starved, unused, measured.gas_used - 1).code);
}

TEST(vm_hdr_info, ancestry_uses_alternative_branch)
{
  const vm::parent_header fork = at(3);
  vm::parent_header main4 = at(4);
  vm::parent_header alt4 = at(4);
  alt4.hash = crypto::cn_fast_hash("alt4", 4);
  alt4.prev = fork.hash;
  vm::parent_header alt5 = at(5);
  alt5.hash = crypto::cn_fast_hash("alt5", 4);
  alt5.prev = alt4.hash;
  const auto read_main = [&](uint64_t height, vm::parent_header& out) {
    if (height == 4) out = main4;
    else if (height == 3) out = fork;
    else return false;
    return true;
  };
  const auto read_hash = [&](const crypto::hash& hash, uint64_t, vm::parent_header& out) {
    if (hash == alt4.hash) out = alt4;
    else if (hash == fork.hash) out = fork;
    else if (hash == fork.prev) out = at(2);
    else return false;
    return true;
  };
  vm::parent_header result;
  ASSERT_TRUE(vm::resolve_ancestor_header(alt5, 4, result, read_main, read_hash));
  EXPECT_EQ(alt4.hash, result.hash);
  ASSERT_TRUE(vm::resolve_ancestor_header(alt5, 3, result, read_main, read_hash));
  EXPECT_EQ(fork.hash, result.hash);
  ASSERT_TRUE(vm::resolve_ancestor_header(alt5, 2, result, read_main, read_hash));
  EXPECT_EQ(at(2).hash, result.hash);
}

TEST(vm_hdr_info, provider_failure_can_retry_contract_call)
{
  vm::state_tree state;
  cryptonote::contract_block_context context;
  context.height = 10;
  context.parent = at(9);
  context.hdr_info_active = true;
  bool unavailable = true;
  bool allocation_failure = false;
  context.headers = [&](uint64_t height, vm::parent_header& header) {
    if (unavailable)
    {
      if (allocation_failure)
        throw std::bad_alloc();
      throw cryptonote::DB_ERROR("header unavailable");
    }
    header = at(height);
    return true;
  };

  cryptonote::tx_extra_vm_call deploy{};
  deploy.kind = cryptonote::tx_extra_vm_call::kind_deploy;
  deploy.code = shader(4);
  deploy.gas_limit = 1000000;
  deploy.gas_price = 1;
  cryptonote::transaction deployment;
  ASSERT_TRUE(cryptonote::add_vm_call_to_tx_extra(deployment.extra, deploy));
  cryptonote::contract_block_executor deployment_executor(state, context);
  ASSERT_EQ(cryptonote::contract_call_verdict::executed, deployment_executor.execute(deployment, 1000000, false));
  state.apply(deployment_executor.writes());

  cryptonote::tx_extra_vm_call invoke = deploy;
  invoke.kind = cryptonote::tx_extra_vm_call::kind_invoke;
  invoke.contract_id = vm::contract_id_for(deploy.code, {});
  invoke.code.clear();
  invoke.method = 2;
  invoke.args = std::string(82, '\x5a');
  invoke.args[80] = 'k';
  cryptonote::transaction call;
  ASSERT_TRUE(cryptonote::add_vm_call_to_tx_extra(call.extra, invoke));
  cryptonote::contract_block_executor executor(state, context);
  EXPECT_THROW(executor.execute(call, 1000000, false), cryptonote::DB_ERROR);
  allocation_failure = true;
  EXPECT_THROW(executor.execute(call, 1000000, false), std::bad_alloc);
  EXPECT_TRUE(executor.writes().empty());
  unavailable = false;
  EXPECT_EQ(cryptonote::contract_call_verdict::executed, executor.execute(call, 1000000, false));
}

TEST(vm_hdr_info, repeated_alternative_ancestor_reads_are_bounded)
{
  std::vector<vm::parent_header> alt;
  for (uint64_t height = 1; height <= 64; ++height)
  {
    vm::parent_header header = at(height);
    const std::string name = "alt" + std::to_string(height);
    header.hash = crypto::cn_fast_hash(name.data(), name.size());
    if (!alt.empty())
      header.prev = alt.back().hash;
    alt.push_back(header);
  }
  vm::ancestor_header_cache cache(alt.back());
  size_t main_reads = 0;
  size_t hash_reads = 0;
  const auto read_main = [&](uint64_t height, vm::parent_header& header) {
    ++main_reads;
    header = at(height);
    return true;
  };
  const auto read_hash = [&](const crypto::hash& hash, uint64_t height, vm::parent_header& header) {
    ++hash_reads;
    header = alt[height - 1];
    return header.hash == hash;
  };
  for (unsigned repeat = 0; repeat < 3; ++repeat)
    for (uint64_t height = 1; height < 64; ++height)
    {
      vm::parent_header found;
      ASSERT_TRUE(cache.resolve(height, found, read_main, read_hash));
      EXPECT_EQ(alt[height - 1].hash, found.hash);
    }
  EXPECT_EQ(63u, hash_reads);
  EXPECT_EQ(63u, main_reads);
}
