# codecov

add_library(codecov INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(STATUS "Compiler ${CMAKE_CXX_COMPILER_ID} supports code-coverage.")

    target_compile_options(codecov
        INTERFACE
        -O0 # no optimization
        -g # generate debug info
        --coverage # sets all required flags
    )

    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(codecov INTERFACE --coverage)
    else()
        target_link_libraries(codecov INTERFACE --coverage)
    endif()
else()
    message(STATUS "Compiler ${CMAKE_CXX_COMPILER_ID} doesn't support code-coverage.")
endif()
