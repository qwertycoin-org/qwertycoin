# Copyright (c) 2026, Qwertycoin
#
# SPDX-License-Identifier: BSD-3-Clause

set(vm_sources
  bvm2/common.cpp
  bvm2/wasm_interpreter.cpp
  bvm2/bvm2.cpp
  app_shader.cpp
  host_api.cpp
  state_tree.cpp
  vm.cpp)

monero_find_all_headers(vm_headers "${CMAKE_CURRENT_SOURCE_DIR}")

monero_add_library(vm
  ${vm_sources}
  ${vm_headers})
target_link_libraries(vm
  PUBLIC
    cncrypto
  PRIVATE
    ${SODIUM_LIBRARY})
set_target_properties(obj_vm
  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
