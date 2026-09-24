set(qms_sources
  protocol.cpp
  secure_store.cpp
  transport_policy.cpp)

option(QWC_ENABLE_QMS2_CRYPTO "Build the pinned Rust/libsignal QMS2 backend" OFF)

if(QWC_ENABLE_QMS2_CRYPTO)
  find_program(QWC_CARGO_EXECUTABLE cargo REQUIRED)
  set(qms_crypto_manifest "${CMAKE_CURRENT_SOURCE_DIR}/crypto/Cargo.toml")
  set(qms_crypto_lock "${CMAKE_CURRENT_SOURCE_DIR}/crypto/Cargo.lock")
  set(qms_crypto_target_dir "${CMAKE_CURRENT_BINARY_DIR}/rust-target")
  set(qms_crypto_library "${qms_crypto_target_dir}/release/${CMAKE_STATIC_LIBRARY_PREFIX}qwertycoin_qms_crypto${CMAKE_STATIC_LIBRARY_SUFFIX}")

  add_custom_command(
    OUTPUT "${qms_crypto_library}"
    COMMAND "${CMAKE_COMMAND}" -E env
      "CARGO_TARGET_DIR=${qms_crypto_target_dir}"
      "${QWC_CARGO_EXECUTABLE}" build --release --locked --manifest-path "${qms_crypto_manifest}"
    DEPENDS
      "${qms_crypto_manifest}"
      "${qms_crypto_lock}"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto/rust-toolchain.toml"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto/src/lib.rs"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto/src/ffi.rs"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto/src/package.rs"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto/src/store.rs"
      "${CMAKE_CURRENT_SOURCE_DIR}/crypto/src/wasm.rs"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/crypto"
    COMMENT "Building pinned QMS2 Rust/libsignal backend"
    VERBATIM)
  add_custom_target(qms_crypto_rust_build DEPENDS "${qms_crypto_library}")
  add_library(qms_crypto_rust STATIC IMPORTED GLOBAL)
  set_target_properties(qms_crypto_rust PROPERTIES IMPORTED_LOCATION "${qms_crypto_library}")
  add_dependencies(qms_crypto_rust qms_crypto_rust_build)
  list(APPEND qms_sources crypto_backend.cpp)
endif()

monero_find_all_headers(qms_private_headers "${CMAKE_CURRENT_SOURCE_DIR}")
monero_private_headers(qms ${qms_private_headers})
monero_add_library(qms ${qms_sources} ${qms_private_headers})
target_link_libraries(qms
  PUBLIC cryptonote_basic common
  PRIVATE ${SODIUM_LIBRARY} ${EXTRA_LIBRARIES})

if(QWC_ENABLE_QMS2_CRYPTO)
  target_compile_definitions(qms PUBLIC QWC_ENABLE_QMS2_CRYPTO=1)
  target_link_libraries(qms PRIVATE qms_crypto_rust ${CMAKE_DL_LIBS})
endif()
