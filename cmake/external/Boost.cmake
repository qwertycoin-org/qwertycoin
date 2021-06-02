# Boost

set(Boost_COMPONENTS atomic chrono date_time filesystem program_options serialization thread)

if(MSVC)
    add_definitions(-DBOOST_ALL_NO_LIB)
    add_definitions(-DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE)
endif()

# If Boost_DIR is not set, look for BOOST_ROOT and BOOSTROOT as alternatives,
# since these are more conventional for Boost.
if("$ENV{Boost_DIR}" STREQUAL "")
    if(EXISTS "$ENV{BOOST_ROOT}" AND IS_DIRECTORY "$ENV{BOOST_ROOT}")
        set(ENV{Boost_DIR} $ENV{BOOST_ROOT})
    elseif(EXISTS "$ENV{BOOSTROOT}" AND IS_DIRECTORY "$ENV{BOOSTROOT}")
        set(ENV{Boost_DIR} $ENV{BOOSTROOT})
    endif()
    # NOTE: Don't forget to set BOOST_LIBRARYDIR if you are using Visual Studio!
endif()

if(EXISTS "$ENV{Boost_DIR}" AND IS_DIRECTORY "$ENV{Boost_DIR}")
    message(STATUS "Boost_DIR is set. Using precompiled Boost.")
    message(STATUS "Boost path: $ENV{Boost_DIR}")
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_NO_SYSTEM_PATHS ON)
    find_package(Boost REQUIRED COMPONENTS ${Boost_COMPONENTS})
else()
    message(STATUS "Boost_DIR is not set. Using Hunter (package manager) to install Boost.")
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    hunter_add_package(Boost COMPONENTS ${Boost_COMPONENTS})
    find_package(Boost QUIET REQUIRED COMPONENTS ${Boost_COMPONENTS})
endif()
