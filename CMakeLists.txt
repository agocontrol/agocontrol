cmake_minimum_required (VERSION 3.0)
project (agocontrol)

# The version number.
set (AGOCONTROL_VERSION 0.1)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(PkgConfig REQUIRED)

include (CheckIncludeFiles)

include(cmake/AgoFileHelpers.cmake)

find_package(PythonNaive)


set (INSTALL_PREFIX /usr)

# FreeBSD have some differences in building, mainly
# that non-base libs/includes are located in /usr/local.
if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set (OS_FREEBSD YES)
    set (INSTALL_PREFIX /usr/local)
    include_directories("${INSTALL_PREFIX}/include")
    link_directories(${INSTALL_PREFIX}/lib)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set (OS_LINUX YES)
    set (INSTALL_PREFIX /usr)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # See README.OSX. Does not actually build.
    set (OS_DARWIN YES)
    include_directories("./local-include")
else()
    message(STATUS "Unknown OS ${CMAKE_SYSTEM_NAME}, expect issues!")
endif()

# some configurable values
IF (OS_FREEBSD)
    set (BASEDIR ${INSTALL_PREFIX}/agocontrol CACHE STRING "Directory to store agocontrol installation")
    set (ETCDIR ${INSTALL_PREFIX}/etc CACHE STRING "Directory for system-wide settings")
    set (CONFDIR ${ETCDIR}/agocontrol CACHE STRING "Directory for agocontrol settings")
    # XXX: Not sure what "variable" data is, but only .sql files are installed currently,
    # and those should be stored in share.
    set (DATADIR ${INSTALL_PREFIX}/share/agocontrol CACHE STRING "Directory for variable data")
    set (LOCALSTATEDIR ${INSTALL_PREFIX}/var/agocontrol CACHE STRING "Directory for state info")
ELSE()
    set (BASEDIR /opt/agocontrol CACHE STRING "Directory to store agocontrol installation")
    set (ETCDIR /etc CACHE STRING "Directory for system-wide settings")
    set (CONFDIR ${ETCDIR}/opt/agocontrol CACHE STRING "Directory for agocontrol settings")
    set (DATADIR /var/opt/agocontrol CACHE STRING "Directory for variable data")
    set (LOCALSTATEDIR /var/opt/agocontrol CACHE STRING "Directory for state info")
    set (CMAKE_EXE_LINKER_FLAGS "-Wl,-z,now")
ENDIF (OS_FREEBSD)

# Common structure
set (BINDIR ${BASEDIR}/bin CACHE STRING "Directory to store Binaries")
set (LIBDIR ${INSTALL_PREFIX}/lib CACHE STRING "Directory for libraries")
set (INCDIR ${INSTALL_PREFIX}/include/agocontrol CACHE STRING "Directory for include files")
set (HTMLDIR ${BASEDIR}/html CACHE STRING "Directory for rpc HTML files")
set (AUGEASLENSEDIR ${INSTALL_PREFIX}/share/augeas/lenses CACHE STRING "Directory for augeas lenses")

set (PYTHON2DIST ${INSTALL_PREFIX}/${Python2_SITELIB} CACHE STRING "Directory for Python 2.7 packages")
set (PYTHON3DIST ${INSTALL_PREFIX}/${Python3_SITELIB} CACHE STRING "Directory for Python 3 packages")

# Enable C++11
if (CMAKE_VERSION VERSION_LESS "3.1")
    # Used on Debian Jessie builders
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Note: must ont set for CFLAGS!
        set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
    endif ()
else ()
    set (CMAKE_CXX_STANDARD 11)
endif ()

set_property( DIRECTORY
    APPEND PROPERTY
    COMPILE_OPTIONS
    -Wall
    -DBOOST_LOG_DYN_LINK
    -DBOOST_FILESYSTEM_VERSION=3
    -DBOOST_FILESYSTEM_NO_DEPRECATED

    # Workaround for bug in Boost on Clang which tries to add pragma warning group keyward-warning
    # which does not exists in clang:
    # http://lists.boost.org/Archives/boost/2016/07/230350.php
    -Wno-error=unknown-pragmas
    )

# add the binary tree to the search path for include files
# so that we will find version.h and other configured files
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_BINARY_DIR}/shared")
include_directories("${PROJECT_SOURCE_DIR}/shared")

# We have an amalgamated version of jsoncpp, ensure we can include <json/json.h>
include_directories("${PROJECT_SOURCE_DIR}/shared/jsoncpp")

macro(log_pkg_found PKG_NAME PKG_FOUND PKG_VERSION NOTFOUND_LEVEL NOTFOUND_EXTRA)
    if("${PKG_FOUND}" AND "${PKG_VERSION}" STREQUAL "")
        message(STATUS "${PKG_NAME} found")
    elseif(${PKG_FOUND})
        message(STATUS "${PKG_NAME} version: ${PKG_VERSION}")
    else()
        message(${NOTFOUND_LEVEL} "${PKG_NAME} not found${NOTFOUND_EXTRA}")
    endif()
endmacro()


find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system filesystem thread program_options date_time regex chrono REQUIRED)
find_package(Boost COMPONENTS log)
IF (Boost_LOG_FOUND)
    ADD_DEFINITIONS(-DHAVE_BOOST_LOG)
ELSE()
    find_package(Boost COMPONENTS REQUIRED date_time)
ENDIF()

pkg_search_module(YAML_CPP REQUIRED yaml-cpp yaml-cpp03)
log_pkg_found(yamlcpp "${YAML_CPP_FOUND}" "${YAML_CPP_VERSION}" FATAL_ERROR "")

pkg_search_module(AUGEAS REQUIRED augeas)
log_pkg_found(augeas "${AUGEAS_FOUND}" "${AUGEAS_VERSION}" FATAL_ERROR "")

# Linux/debian has lua5.x, FreeBSD has lua-5.x, fedora has just lua
pkg_search_module(LUA lua5.3 lua-5.3 lua5.2 lua-5.2 lua5.1 lua-5.1 lua)
log_pkg_found(lua "${LUA_FOUND}" "${LUA_VERSION}" WARNING ", parts of core will not be built")

find_package(CppDb)
log_pkg_found(cppdb "${CPPDB_FOUND}" "" WARNING ", parts of core will not be built")

pkg_search_module(LIBRRD librrd)
log_pkg_found(librrd "${LIBRRD_FOUND}" "${LIBRRD_VERSION}" WARNING ", parts of core will not be built")

if(OS_LINUX)
    # used by agosystem
    pkg_search_module(LIBPROCPS REQUIRED libprocps)
    log_pkg_found(libprocps "${LIBPROCPS_FOUND}" "${LIBPROCPS_VERSION}" WARNING "")

    find_package(LinuxI2C)
endif()

pkg_search_module(LIBCURL libcurl)
log_pkg_found(libcurl "${LIBCURL_FOUND}" "${LIBCURL_VERSION}" STATUS "")

find_package(OpenCV)
log_pkg_found(OpenCV "${OpenCV_FOUND}" "${OpenCV_VERSION}" STATUS "")

find_package(OpenZWave)
log_pkg_found(OpenZWave "${OpenZWave_FOUND}" "${OpenZWave_VERSION}" STATUS "")

find_package(EibClient)
log_pkg_found(EibClient "${EibClient_FOUND}" "${EibClient_VERSION}" STATUS "")

pkg_check_modules(OpenLighting libola)
log_pkg_found(OpenLighting "${OpenLighting_FOUND}" "${OpenLighting_VERSION}" STATUS "")

include_directories(${Boost_INCLUDE_DIRS})

# Fix for Boost < 1.56 built without C++, when we build with C++11
# https://stackoverflow.com/questions/35007134/c-boost-undefined-reference-to-boostfilesystemdetailcopy-file
# https://chaste.cs.ox.ac.uk/trac/wiki/ChasteGuides/ModernCpp#Boostscopedenumsforboost1.56
IF(Boost_VERSION VERSION_LESS 105600)
    message(STATUS "Boost version < 1.56, building with BOOST_NO_CXX11_SCOPED_ENUMS")
    set_property( DIRECTORY
        APPEND PROPERTY
        COMPILE_OPTIONS
        -DBOOST_NO_CXX11_SCOPED_ENUMS
    )
ENDIF()

find_package(CppUnit)
log_pkg_found(CppUnit "${CPPUNIT_FOUND}" "${CPPUNIT_VERSION}" STATUS "not building unit-tests")

check_include_files("xlocale.h" HAVE_XLOCALE_H)

set (ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${LIBDIR}")
set (LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${LIBDIR}")
set (RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BINDIR}")

get_filename_component(CMAKE_CURRENT_SOURCE_DIR_REAL "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(CMAKE_CURRENT_BINARY_DIR_REAL "${CMAKE_CURRENT_BINARY_DIR}" REALPATH)
if ("${CMAKE_CURRENT_SOURCE_DIR_REAL}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR_REAL}")
    set (IN_SOURCE_BUILD TRUE)
    message(WARNING "In-Source build is not recommended. Instead, create a separate directory and run cmake /path/to/source")
else()
    set (IN_SOURCE_BUILD FALSE)
endif()

# Shared for all
install(DIRECTORY DESTINATION ${BINDIR})

# add sub directories
OPTION(BUILD_SHARED "Build the agoclient shared library" ON)
if (BUILD_SHARED)
  add_subdirectory (shared)
endif()

OPTION(BUILD_TESTS "Build the unittests" ON)
if (BUILD_TESTS AND CPPUNIT_FOUND)
    # These are only for Shared right now, so add them right after.
    enable_testing()
    add_subdirectory (unittest)
endif()

OPTION(BUILD_CORE "Build the CORE" ON)
if (BUILD_CORE)
  add_subdirectory (core)
  install (FILES data/inventory.sql data/datalogger.sql DESTINATION ${DATADIR})
endif()

OPTION(BUILD_GATEWAYS "Build the gateways" ON)
if (BUILD_GATEWAYS)
  add_subdirectory (gateways)
endif()

OPTION(BUILD_DEVICES "Build the devices" ON)
if (BUILD_DEVICES)
  add_subdirectory (devices)
endif()

OPTION(BUILD_CONF "Build the configs" ON)
if (BUILD_CONF)
  add_subdirectory (conf)
endif()

OPTION(BUILD_SCRIPTS "Build the scripts" ON)
if (BUILD_SCRIPTS)
  add_subdirectory (scripts)
endif()


# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
        "${PROJECT_SOURCE_DIR}/build_config.h.in"
        "${PROJECT_BINARY_DIR}/build_config.h"
        @ONLY
)
