# Find the CppDb includes and library
#
# This module defines
# CPPDB_INCLUDE_DIRS, where to find frontend.h, etc.
# CPPDB_LIBRARIES, the libraries to link against to use CppUnit.
# CPPDB_FOUND, If false, do not try to use CppUnit.

FIND_PATH(CPPDB_INCLUDE_DIRS cppdb/frontend.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(CPPDB_LIBRARIES cppdb
   ${CPPDB_INCLUDE_DIRS}/../lib
   /usr/local/lib
   /usr/lib)

if(CPPDB_INCLUDE_DIRS)
    IF(CPPDB_LIBRARIES)
        SET(CPPDB_FOUND "YES")
        SET(CPPDB_LIBRARIES ${CPPDB_LIBRARIES} ${CMAKE_DL_LIBS})
    endif()
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CPPDB DEFAULT_MSG CPPDB_LIBRARIES CPPDB_INCLUDE_DIRS)

