# sparsehash

# NOTE: hunter_add_package(sparsehash) doesn't work (https://github.com/ruslo/hunter/issues/695)

if(WIN32)
    set(SPARSEHASH_CONFIGURE_COMMAND
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping CONFIGURE step... (not needed)")

    set(SPARSEHASH_BUILD_COMMAND
        COMMAND ${CMAKE_COMMAND} -E echo "Skipping BUILD step... (not needed)")

    set(SPARSEHASH_INSTALL_COMMAND
        COMMAND ${CMAKE_COMMAND} -E make_directory <INSTALL_DIR>/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/src/google <INSTALL_DIR>/include/google
        COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/src/sparsehash <INSTALL_DIR>/include/sparsehash
        COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/src/windows/google <INSTALL_DIR>/include/google
        COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/src/windows/sparsehash <INSTALL_DIR>/include/sparsehash)
else()
    set(SPARSEHASH_CONFIGURE_COMMAND
        COMMAND ${CMAKE_COMMAND} -E copy_directory "<SOURCE_DIR>" "<BINARY_DIR>"
        COMMAND cd "<BINARY_DIR>" && ./configure --prefix=<INSTALL_DIR> CXXFLAGS=-std=c++11)

    set(SPARSEHASH_BUILD_COMMAND
        cd "<BINARY_DIR>" && ${CMAKE_MAKE_PROGRAM})

    set(SPARSEHASH_INSTALL_COMMAND
        cd "<BINARY_DIR>" && ${CMAKE_MAKE_PROGRAM} install)
endif()

ExternalProject_Add(sparsehash
    GIT_REPOSITORY https://github.com/sparsehash/sparsehash.git
    GIT_TAG master
    GIT_SHALLOW ON
    GIT_PROGRESS OFF

    UPDATE_COMMAND ""
    PATCH_COMMAND ""

    CONFIGURE_COMMAND ${SPARSEHASH_CONFIGURE_COMMAND}
    BUILD_COMMAND ${SPARSEHASH_BUILD_COMMAND}
    BUILD_ALWAYS OFF
    TEST_COMMAND ""
    INSTALL_COMMAND ${SPARSEHASH_INSTALL_COMMAND}
)

ExternalProject_Get_property(sparsehash INSTALL_DIR)
set(SPARSEHASH_ROOT "${INSTALL_DIR}")
set(SPARSEHASH_INCLUDE_DIRS "${SPARSEHASH_ROOT}/include")
mark_as_advanced(SPARSEHASH_ROOT SPARSEHASH_INCLUDE_DIRS)

add_library(sparsehash::sparsehash INTERFACE IMPORTED GLOBAL)
add_dependencies(sparsehash::sparsehash sparsehash)
