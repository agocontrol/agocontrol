cmake_minimum_required (VERSION 3.0)
project (agocontrol)

# The version number.
set (AGOCONTROL_VERSION 0.1)

find_package(PkgConfig REQUIRED)

# FreeBSD have some differences in building, mainly 
# that non-base libs/includes are located in /usr/local.
IF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	set (FREEBSD YES)
	set (LOCALDIR /usr/local)
	include_directories("${LOCALDIR}/include")
	LINK_DIRECTORIES(${LOCALDIR}/lib)
ELSE (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	set (LOCALDIR /usr)
ENDIF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

# See README.OSX
IF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
	include_directories("./local-include")
ENDIF ()

# some configurable values
IF (FREEBSD)
	set (BINDIR ${LOCALDIR}/bin CACHE STRING "Directory to store Binaries")
	set (ETCDIR ${LOCALDIR}/etc CACHE STRING "Directory for system-wide settings")
	set (LIBDIR /usr/lib CACHE STRING "Directory for libraries")
	set (CONFDIR ${ETCDIR}/agocontrol CACHE STRING "Directory for agocontrol settings")
	set (INCDIR ${LOCALDIR}/include/agocontrol CACHE STRING "Directory for include files")
	# XXX: Not sure what "variable" data is, but only .sql files are installed currently,
	# and those should be stored in share.
	set (DATADIR ${LOCALDIR}/share/agocontrol CACHE STRING "Directory for variable data")
	set (LOCALSTATEDIR ${LOCALDIR}/var/agocontrol CACHE STRING "Directory for state info")
	set (HTMLDIR ${LOCALDIR}/share/agocontrol/html CACHE STRING "Directory for rpc HTML files")
	set (AUGEASLENSEDIR ${LOCALDIR}/share/augeas/lenses CACHE STRING "Directory for augeas lenses")
ELSE (FREEBSD)
	set (BINDIR /opt/agocontrol/bin CACHE STRING "Directory to store Binaries")
	set (ETCDIR /etc CACHE STRING "Directory for system-wide settings")
	set (LIBDIR /usr/lib CACHE STRING "Directory for libraries")
	set (CONFDIR ${ETCDIR}/opt/agocontrol CACHE STRING "Directory for agocontrol settings")
	set (INCDIR /usr/include/agocontrol CACHE STRING "Directory for include files")
	set (DATADIR /var/opt/agocontrol CACHE STRING "Directory for variable data")
	set (LOCALSTATEDIR /var/opt/agocontrol CACHE STRING "Directory for state info")
	set (HTMLDIR /opt/agocontrol/html CACHE STRING "Directory for rpc HTML files")
	set (AUGEASLENSEDIR /usr/share/augeas/lenses CACHE STRING "Directory for augeas lenses")
	set (PYTHON2DIST /usr/lib/python2.7/dist-packages CACHE STRING "Directory for Python 2.7 packages")
	set (PYTHON3DIST /usr/lib/python3/dist-packages CACHE STRING "Directory for Python 3 packages")
	set (CMAKE_EXE_LINKER_FLAGS "-Wl,-z,now")
ENDIF (FREEBSD)

set_property( DIRECTORY
	APPEND PROPERTY
	COMPILE_OPTIONS
	-Wall -DBOOST_LOG_DYN_LINK)

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
	-DBOOST_FILESYSTEM_VERSION=3
	-DBOOST_FILESYSTEM_NO_DEPRECATED
	)

# Workaround for bug in Boost on Clang which tries to add pragma warning group keyward-warning
# which does not exists in clang:
# http://lists.boost.org/Archives/boost/2016/07/230350.php
set_property( DIRECTORY
	APPEND PROPERTY
	COMPILE_OPTIONS
	-Wno-error=unknown-pragmas)

# add the binary tree to the search path for include files
# so that we will find version.h and other configured files
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_BINARY_DIR}/shared")
include_directories("${PROJECT_SOURCE_DIR}/shared")

# We have an amalgamated version of jsoncpp, ensure we can include <json/json.h>
include_directories("${PROJECT_SOURCE_DIR}/shared/jsoncpp")

# fix augeas bug
include_directories("${LOCALDIR}/include/libxml2")

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(CppDb REQUIRED)
if (CPPDB_FOUND)
	include_directories(${CPPDB_INCLUDE_DIRS})
else()
	IF (NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
		# TEMP: Disable this requirement on OS X, not in brew.
		message(FATAL_ERROR "Could not find cppdb")
	endif ()
endif (CPPDB_FOUND)

# FreeBSD user might only have 0.3 installed.
pkg_search_module(YAML_CPP REQUIRED yaml-cpp yaml-cpp03)
message(STATUS "yaml-cpp version: ${YAML_CPP_VERSION}")

# Linux/debian has lua5.x, FreeBSD has lua-5.x
pkg_search_module(LUA REQUIRED  lua5.3 lua-5.3 lua5.2 lua-5.2 lua5.1 lua-5.1)
message(STATUS "lua version: ${LUA_VERSION}")

find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system filesystem thread program_options date_time regex chrono REQUIRED)

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

FIND_PACKAGE(Boost COMPONENTS log)
IF (Boost_LOG_FOUND)
	ADD_DEFINITIONS(-DHAVE_BOOST_LOG)
ELSE()
	FIND_PACKAGE(Boost COMPONENTS REQUIRED date_time)
ENDIF()

FIND_PACKAGE(CppUnit)
IF (CPPUNIT_FOUND)
    message(STATUS "CppUnit found, building unit-tests by default")
    set(BUILD_TESTS_DEFAULT ON)
ELSE()
    message(STATUS "CppUnit not found, NOT building unit-tests by default")
    set(BUILD_TESTS_DEFAULT OFF)
ENDIF()

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

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
		"${PROJECT_SOURCE_DIR}/build_config.h.in"
		"${PROJECT_BINARY_DIR}/build_config.h"
		@ONLY
)

# add sub directories
OPTION(BUILD_SHARED "Build the agoclient shared library" ON)
if (BUILD_SHARED)
  add_subdirectory (shared) 
endif()

OPTION(BUILD_TESTS "Build the unittests" ${BUILD_TESTS_DEFAULT})
if (BUILD_TESTS)
	# These are only for Shared right now, so add them right after.
	enable_testing()
	add_subdirectory (unittest)
endif()

OPTION(BUILD_CORE "Build the CORE" ON)
if (BUILD_CORE)
  add_subdirectory (core) 
  install (PROGRAMS gateways/agomeloware.py DESTINATION ${BINDIR})
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
