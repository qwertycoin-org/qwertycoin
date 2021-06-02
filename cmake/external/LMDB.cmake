# LMDB

ExternalProject_Add(lmdb-adv
        GIT_REPOSITORY https://github.com/qwertycoin-org/lmdb.git
        GIT_TAG master
        GIT_SHALLOW ON
        GIT_PROGRESS OFF

        UPDATE_COMMAND ""
        PATCH_COMMAND ""

        CONFIGURE_COMMAND
            COMMAND ${CMAKE_COMMAND} -E make_directory <BINARY_DIR>/include
        #BUILD_COMMAND ${LMDB_BUILD_COMMAND}
        BUILD_ALWAYS OFF
        TEST_COMMAND ""
        INSTALL_COMMAND
            COMMAND ${CMAKE_COMMAND} -E copy_directory "<SOURCE_DIR>" "<BINARY_DIR>/include"
            COMMAND ${CMAKE_COMMAND} --build . --config Release
        )

ExternalProject_Get_property(lmdb-adv BINARY_DIR)
get_filename_component(LMDB_DIR "${BINARY_DIR}" ABSOLUTE CACHE)
set(LMDB_INCLUDE_DIRS "${LMDB_DIR}/include")
set(LMDB_STATIC_LIBRARY "${LMDB_DIR}/Release/lmdb-adv${CMAKE_STATIC_LIBRARY_SUFFIX}")
mark_as_advanced(LMDB_DIR LMDB_INCLUDE_DIRS LMDB_STATIC_LIBRARY)

add_library(lmdb::lmdb STATIC IMPORTED GLOBAL)
add_dependencies(lmdb::lmdb lmdb-adv)

set_target_properties(lmdb::lmdb PROPERTIES
    IMPORTED_LOCATION "${LMDB_STATIC_LIBRARY}"
    #INTERFACE_INCLUDE_DIRECTORIES ${LMDB_INCLUDE_DIRS}
    INTERFACE_COMPILE_DEFINITIONS "LMDB_STATICLIB")