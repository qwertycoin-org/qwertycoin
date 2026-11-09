monero_add_minimal_executable(vm_gas_bench vm_gas_bench.cpp)
target_include_directories(vm_gas_bench PRIVATE ${CMAKE_SOURCE_DIR}/tests/unit_tests)
target_link_libraries(vm_gas_bench PRIVATE vm cncrypto ${SODIUM_LIBRARY} ${EXTRA_LIBRARIES})
set_property(TARGET vm_gas_bench PROPERTY FOLDER "tests")
