if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # Clang and AppleClang
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # GCC
    # With GCC 6.1.1 the compiled binary malfunctions due to aliasing.
    # Until that is fixed in the code (Issue #847), force compiler to be conservative.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # Intel C++
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # Visual Studio C++
endif()
