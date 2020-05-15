# Find the CppDb includes and library
#
# This module defines
# CppDb_INCLUDE_DIRS, where to find frontend.h, etc.
# CppDb_LIBRARIES, the libraries to link against to use CppUnit.
# CppDb_FOUND, If false, do not try to use CppUnit.

FIND_PATH(CppDb_INCLUDE_DIRS cppdb/frontend.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(CppDb_LIBRARIES cppdb
   ${CppDb_INCLUDE_DIRS}/../lib
   /usr/local/lib
   /usr/lib)

if(CppDb_INCLUDE_DIRS)
    IF(CppDb_LIBRARIES)
        SET(CppDb_FOUND "YES")
        SET(CppDb_LIBRARIES ${CppDb_LIBRARIES} ${CMAKE_DL_LIBS})
    endif()
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CppDb DEFAULT_MSG CppDb_LIBRARIES CppDb_INCLUDE_DIRS)


# Modern CMake suggests that variable name is named after FindXxx i.e. same case.
# Older cmake (3.0) ignores case and always registers UPPERCASE_FOUND
if(NOT DEFINED CppDb_FOUND)
    set(CppDb_FOUND ${CPPDB_FOUND})
endif()