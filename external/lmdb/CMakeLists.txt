# vim: set ai ts=4 expandtab:
# ::=========================================================================::

cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(lmdb VERSION 0.9.23)
find_package(Threads REQUIRED)
SET(HEADERS_LMDB
    lmdb.h
    midl.h
    lmdbpp.h
)

SET(SOURCES_LMDB
    mdb.c
    midl.c
)

SET(LMDB_STATIC_LIB lmdb)

add_definitions("-DMDB_FDATASYNC_WORKS")

add_library(${LMDB_STATIC_LIB} STATIC ${HEADERS_LMDB} ${SOURCES_LMDB})
