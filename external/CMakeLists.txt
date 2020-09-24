add_subdirectory(lmdb)

if(MSVC)
    set_property(TARGET lmdb PROPERTY FOLDER "external")
elseif(NOT MSVC)
    set_property(TARGET lmdb PROPERTY FOLDER "external")
endif()
