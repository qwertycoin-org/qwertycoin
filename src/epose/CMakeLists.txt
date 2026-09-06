# Copyright (c) 2026, Qwertycoin

set(epose_sources
  attestation_pool.cpp
  chain_state.cpp
  membership_v2.cpp
  service_epoch.cpp
  service_registry.cpp
  service_node.cpp
  service_node_config.cpp)

set(epose_headers
  attestation_pool.h
  chain_state.h
  membership_v2.h
  service_epoch.h
  service_registry.h
  service_node.h
  service_node_config.h)

monero_add_library(epose
  ${epose_sources}
  ${epose_headers})
target_link_libraries(epose
  PUBLIC
    cncrypto
    cryptonote_basic
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY})
