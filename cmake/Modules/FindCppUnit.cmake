# http://www.cmake.org/pipermail/cmake/2006-October/011446.html
#
# Find the CppUnit includes and library
#
# This module defines
# CppUnit_INCLUDE_DIRS, where to find tiff.h, etc.
# CppUnit_LIBRARIES, the libraries to link against to use CppUnit.
# CppUnit_FOUND, If false, do not try to use CppUnit.

FIND_PATH(CppUnit_INCLUDE_DIRS cppunit/TestCase.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(CppUnit_LIBRARIES cppunit
   ${CppUnit_INCLUDE_DIRS}/../lib
   /usr/local/lib
   /usr/lib)

if(CppUnit_INCLUDE_DIRS)
    IF(CppUnit_LIBRARIES)
        SET(CppUnit_FOUND "YES")
        SET(CppUnit_LIBRARIES ${CppUnit_LIBRARIES} ${CMAKE_DL_LIBS})
    endif()
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CppUnit DEFAULT_MSG CppUnit_LIBRARIES CppUnit_INCLUDE_DIRS)

# Modern CMake suggests that variable name is named after FindXxx i.e. same case.
# Older cmake (3.0) ignores case and always registers UPPERCASE_FOUND
if(NOT DEFINED CppUnit_FOUND)
    set(CppUnit_FOUND ${CPPUNIT_FOUND})
endif()