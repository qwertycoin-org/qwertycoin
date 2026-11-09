# Copyright (c) 2014-2022, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set(unit_tests_sources
  account.cpp
  apply_permutation.cpp
  address_from_url.cpp
  base58.cpp
  blockchain_db.cpp
  block_queue.cpp
  block_reward.cpp
  blockchain_cleanup.cpp
  bootstrap_node_selector.cpp
  bulletproofs.cpp
  bulletproofs_plus.cpp
  canonical_amounts.cpp
  chacha.cpp
  chainlock_v1.cpp
  chainlock_rpc.cpp
  chainlock_membership_retention.cpp
  chainlock_rpc_vector.cpp
  contract_addsig.cpp
  checkpoints.cpp
  command_line.cpp
  crypto.cpp
  decompose_amount_into_digits.cpp
  device.cpp
  difficulty.cpp
  dns_resolver.cpp
  epee_boosted_tcp_server.cpp
  epee_levin_protocol_handler_async.cpp
  epee_http_server.cpp
  epee_serialization.cpp
  epose.cpp
  epose_canonical_service_v2.cpp
  epose_coordinator_v2.cpp
  epose_diagnostics_v2.cpp
  epose_http_client.cpp
  epose_block_transition_v2.cpp
  epose_state_retention.cpp
  epose_memory_soak.cpp
  epose_memory_soak_rolling.cpp
  epose_receipt_launch.cpp
  epose_replay_v2.cpp
  epose_blockchain_recovery.cpp
  epose_rpc_handler.cpp
  epee_utils.cpp
  expect.cpp
  fee.cpp
  json_serialization.cpp
  get_xtype_from_string.cpp
  hashchain.cpp
  hmac_keccak.cpp
  http.cpp
  keccak.cpp
  levin.cpp
  logging.cpp
  long_term_block_weight.cpp
  lmdb.cpp
  lmdb_remove_data_file.cpp
  main.cpp
  memwipe.cpp
  mlocker.cpp
  mnemonics.cpp
  mul_div.cpp
  multiexp.cpp
  multisig.cpp
  net.cpp
  network_banner.cpp
  node_server.cpp
  notify.cpp
  output_distribution.cpp
  parse_amount.cpp
  pruning.cpp
  qms.cpp
  random.cpp
  randomx_large_pages.cpp
  rolling_median.cpp
  scaling_2021.cpp
  serialization.cpp
  sha256.cpp
  slow_memmem.cpp
  subaddress.cpp
  test_tx_utils.cpp
  test_peerlist.cpp
  test_protocol_pack.cpp
  threadpool.cpp
  tx_proof.cpp
  hardfork.cpp
  contracts_fork.cpp
  devnet_profile.cpp
  height_window.cpp
  contract_state.cpp
  contract_tx_extra.cpp
  unbound.cpp
  uri.cpp
  updates.cpp
  util.cpp
  varint.cpp
  vm_app_shader.cpp
  vm_calibration.cpp
  vm_determinism.cpp
  vm_float_rejection.cpp
  vm_gas.cpp
  vm_hdr_info.cpp
  vm_limits.cpp
  vm_state_tree.cpp
  vm_trap.cpp
  ver_rct_non_semantics_simple_cached.cpp
  ringct.cpp
  output_selection.cpp
  vercmp.cpp
  ringdb.cpp
  wallet_contract_call_builder.cpp
  wallet_contract_mints.cpp
  wallet_replay_hook.cpp
  wallet_storage.cpp
  wipeable_string.cpp
  is_hdd.cpp
  aligned.cpp
  rpc_version_str.cpp
  rpc_args.cpp
  zmq_rpc.cpp)

set(unit_tests_headers
  unit_tests_utils.h
  vm_test_utils.h)

monero_add_minimal_executable(unit_tests
  ${unit_tests_sources}
  ${unit_tests_headers})
target_link_libraries(unit_tests
  PRIVATE
    ringct
    cryptonote_protocol
    cryptonote_core
    epose
    daemon_messages
    daemon_rpc_server
    blockchain_db
    lmdb_lib
    rpc
    net
    serialization
    wallet
    p2p
    qms
    vm
    version
    ${Boost_CHRONO_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${GTEST_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES}
    ${ZMQ_LIB})
set_property(TARGET unit_tests
  PROPERTY
    FOLDER "tests")

if (NOT MSVC)
  set_property(TARGET unit_tests
    APPEND_STRING
    PROPERTY
      COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif ()

SET_PROPERTY(SOURCE main.cpp PROPERTY COMPILE_FLAGS -DDEFAULT_DATA_DIR="\\"${CMAKE_SOURCE_DIR}/tests/data\\"")

SET_PROPERTY(SOURCE memwipe.cpp PROPERTY COMPILE_FLAGS -Ofast)

add_test(
  NAME    unit_tests
  COMMAND unit_tests --data-dir "${TEST_DATA_DIR}")

# Focused QMS suite: intentionally has no daemon/process/network harness and can
# be run under the offline execution boundary independently of the broad suite.
monero_add_minimal_executable(qms_unit_tests qms.cpp)
target_link_libraries(qms_unit_tests
  PRIVATE qms cryptonote_basic common cncrypto epee
    ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${SODIUM_LIBRARY})
add_test(NAME qms_unit_tests COMMAND qms_unit_tests)

monero_add_minimal_executable(epose_keystore_v2_probe
  epose_keystore_v2_probe.cpp)
target_link_libraries(epose_keystore_v2_probe
  PRIVATE
    epose
    cryptonote_basic
    cncrypto
    common
    epee
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${EXTRA_LIBRARIES})
set_property(TARGET epose_keystore_v2_probe PROPERTY FOLDER "tests")

monero_add_minimal_executable(epose_keystore_v2_tests
  epose_service_keystore_v2.cpp
  epose_main.cpp)
target_link_libraries(epose_keystore_v2_tests
  PRIVATE
    epose
    cryptonote_basic
    cncrypto
    common
    epee
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${GTEST_LIBRARIES}
    ${EXTRA_LIBRARIES})
set_property(TARGET epose_keystore_v2_tests PROPERTY FOLDER "tests")
add_test(
  NAME    epose_keystore_v2_tests
  COMMAND epose_keystore_v2_tests)

monero_add_minimal_executable(epose_unit_tests
  epose.cpp
  epose_blockchain_recovery.cpp
  epose_schema_one_golden.cpp
  epose_rolling_window.cpp
  epose_block_transition_v2.cpp
  epose_state_retention.cpp
  epose_memory_soak.cpp
  epose_memory_soak_rolling.cpp
  epose_receipt_launch.cpp
  epose_replay_v2.cpp
  epose_canonical_service_v2.cpp
  epose_coordinator_v2.cpp
  epose_diagnostics_v2.cpp
  epose_http_client.cpp
  epose_envelope_v2.cpp
  epose_hardfork_migration_v2.cpp
  epose_lifecycle_v2.cpp
  epose_reward_v2.cpp
  epose_reward_contract_mints.cpp
  epose_reward_status_name_v2.cpp
  epose_reward_miner_is_payee_v2.cpp
  epose_reward_service_start_pre_hf18.cpp
  epose_reward_service_start_post_hf18.cpp
  epose_reward_service_start_rejections.cpp
  epose_reward_service_zero_reward.cpp
  epose_reward_service_start_boundary.cpp
  epose_resource_policy_v2.cpp
  epose_service_keystore_v2.cpp
  epose_service_producer_v2.cpp
  epose_relay_pool_v2.cpp
  epose_relay_hardening_height.cpp
  epose_warning_throttle.cpp
  epose_rpc_qualification.cpp
  epose_local_ingress_origin.cpp
  epose_producer_step_counter.cpp
  epose_rpc_epoch_snapshot.cpp
  epose_rpc_followups.cpp
  epose_local_ingress_race.cpp
  epose_devnet_round_capacity.cpp
  epose_devnet_parameter_set_hash.cpp
  epose_semantic_batch_v2.cpp
  epose_state_index_v2.cpp
  epose_v2.cpp
  epose_receipts_v2.cpp
  epose_main.cpp)
target_link_libraries(epose_unit_tests
  PRIVATE
    ringct
    cryptonote_core
    epose
    cryptonote_basic
    blockchain_db
    serialization
    common
    epee
    version
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${GTEST_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})
set_property(TARGET epose_unit_tests
  PROPERTY
    FOLDER "tests")
if (NOT MSVC)
  set_property(TARGET epose_unit_tests
    APPEND_STRING
    PROPERTY
      COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif ()
add_test(
  NAME    epose_unit_tests
  COMMAND epose_unit_tests)
if (QWC_CHAINLOCK_REPLAY_AVAILABLE)
  add_test(
    NAME chainlock_archive_replay
    COMMAND "${CMAKE_COMMAND}" -E env
      ${QWC_CHAINLOCK_REPLAY_TEST_ENVIRONMENT}
      "${PYTHON_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tests/functional_tests/chainlock_archive_replay.py")
  add_test(
    NAME chainlock_archive_perf
    COMMAND "${CMAKE_COMMAND}" -E env
      ${QWC_CHAINLOCK_REPLAY_TEST_ENVIRONMENT}
      "${PYTHON_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/tests/functional_tests/chainlock_archive_perf.py")
endif ()
set(epose_probe_connector_wiring_command
  "${PYTHON_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/check_epose_probe_connector_wiring.py"
  "${CMAKE_SOURCE_DIR}")
add_custom_target(epose_probe_connector_wiring_check
  COMMAND ${epose_probe_connector_wiring_command}
  COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_check_epose_probe_connector_wiring.py"
  VERBATIM)
add_dependencies(epose_unit_tests epose_probe_connector_wiring_check)
add_test(NAME epose_probe_connector_wiring
  COMMAND ${epose_probe_connector_wiring_command})
add_test(NAME epose_probe_connector_wiring_test
  COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_check_epose_probe_connector_wiring.py")
add_test(
  NAME    epose_memory_soak
  COMMAND epose_unit_tests --gtest_filter=epose_memory_soak.*)
add_test(
  NAME    p2p_memory_soak
  COMMAND unit_tests --gtest_filter=p2p_memory_soak.failed_hosts_have_bounded_heap_growth)

monero_add_minimal_executable(epose_admission_bench
  epose_admission_bench.cpp)
target_link_libraries(epose_admission_bench
  PRIVATE
    epose
    cryptonote_basic
    common
    epee
    version
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})
set_property(TARGET epose_admission_bench
  PROPERTY
    FOLDER "tests")

# Reuse the upstream crypto-test RNG only in this synthetic probe: receipt
# commitments contain the randomized subject signature and must be reproducible.
monero_add_minimal_executable(epose_memory_probe
  epose_memory_probe.cpp ../crypto/random.c)
target_link_libraries(epose_memory_probe PRIVATE
  epose cryptonote_basic common epee version
  ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_SYSTEM_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_LIBRARIES})
set_property(TARGET epose_memory_probe PROPERTY FOLDER "tests")

monero_add_minimal_executable(epose_sybil_sim
  epose_sybil_sim.cpp)
target_link_libraries(epose_sybil_sim
  PRIVATE
    epose
    cryptonote_basic
    common
    epee
    version
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})
set_property(TARGET epose_sybil_sim
  PROPERTY
    FOLDER "tests")

monero_add_minimal_executable(test_notifier test_notifier.cpp)
target_link_libraries(test_notifier ${EXTRA_LIBRARIES})
set_property(TARGET test_notifier PROPERTY FOLDER "tests")
add_dependencies(unit_tests test_notifier)
