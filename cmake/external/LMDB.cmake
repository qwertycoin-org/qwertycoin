# LMDB

FetchContent_Declare(
        lmdb
        GIT_REPOSITORY https://github.com/qwertycoin-org/lmdb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS FALSE

        UPDATE_COMMAND ""
        PATCH_COMMAND ""
        )

FetchContent_Populate(lmdb)

set(lmdb_INCLUDE_DIRS "${lmdb_SOURCE_DIR}")

message(STATUS "Configuring Qwertycoin LMDB sources...")

set (LMDB_SOURCES
        "${lmdb_SOURCE_DIR}/lmdb.h"
        "${lmdb_SOURCE_DIR}/mdb.c"
        "${lmdb_SOURCE_DIR}/midl.c"
        "${lmdb_SOURCE_DIR}/midl.h"
        )
