#include "gtest/gtest.h"

#include "vm/gas.h"
#include "vm_test_utils.h"

namespace
{
  std::string local_module(uint32_t count)
  {
    vm_test::module_builder module;
    const uint32_t method = vm_test::method_type(module);
    vm_test::add_ctor_dtor(module, method);
    vm_test::locals locals;
    if (count)
      locals.push_back({count, vm_test::t_i32});
    module.exp("Method_2", module.func(method, vm_test::code().end(), locals));
    return module.build();
  }
}

TEST(vm_calibration, function_locals_are_metered)
{
  vm::state_tree plain_state, local_state;
  const vm::result plain_deploy = vm_test::deploy(plain_state, local_module(0));
  const vm::result local_deploy = vm_test::deploy(local_state, local_module(64));
  ASSERT_EQ(vm::status::ok, plain_deploy.code);
  ASSERT_EQ(vm::status::ok, local_deploy.code);
  const vm::result plain = vm_test::invoke(plain_state, plain_deploy.contract_id, 2);
  const vm::result with_locals = vm_test::invoke(local_state, local_deploy.contract_id, 2);
  ASSERT_EQ(vm::status::ok, plain.code);
  ASSERT_EQ(vm::status::ok, with_locals.code);
  EXPECT_GE(with_locals.gas_used - plain.gas_used, 64 * vm::gas::local_word);
}

TEST(vm_calibration, loading_one_byte_charges_for_the_stored_value)
{
  vm_test::module_builder module;
  const uint32_t load = module.import("LoadVar", {vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32, vm_test::t_i32}, {vm_test::t_i32});
  const uint32_t method = vm_test::method_type(module);
  vm_test::add_ctor_dtor(module, method);
  vm_test::code body;
  body.get(0).i32(8).op(0x6a).i32(8).get(0).i32(32).op(0x6a).i32(1).i32(0).call(load).drop().end();
  module.exp("Method_2", module.func(method, body));

  vm::state_tree state;
  const vm::result deployed = vm_test::deploy(state, module.build());
  ASSERT_EQ(vm::status::ok, deployed.code);
  std::string args(64, 0);
  args[8] = 1;
  std::string key(8, 0);
  key[0] = 1;
  state.set(vm_test::var_key(deployed.contract_id, 0, key), std::string(1, 'v'));
  const vm::result small = vm_test::invoke(state, deployed.contract_id, 2, args);
  ASSERT_EQ(vm::status::ok, small.code);
  state.set(vm_test::var_key(deployed.contract_id, 0, key), std::string(1024, 'v'));
  const vm::result present = vm_test::invoke(state, deployed.contract_id, 2, args);
  ASSERT_EQ(vm::status::ok, present.code);
  EXPECT_EQ(1023 * vm::gas::load_var_per_byte, present.gas_used - small.gas_used);
}
