if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Don't use e.g. GNU extension (like -std=gnu++11) for portability
set(CMAKE_CXX_EXTENSIONS OFF)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # Clang and AppleClang
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # GCC

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")

    # With GCC 6.1.1 the compiled binary malfunctions due to aliasing.
    # Until that is fixed in the code (Issue #847), force compiler to be conservative.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # Intel C++
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # Visual Studio C++

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /bigobj")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()
