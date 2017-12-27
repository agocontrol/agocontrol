# Find the OpenLightning includes and library
#
# This module defines
# OpenLighting_INCLUDE_DIRS, where to find OpenLighting.h
# OpenLighting_LIBRARIES, the libraries to link against to use it
# OpenLighting_FOUND, If false, do not try to use it

FIND_PATH(OpenLighting_INCLUDE_DIRS ola/DmxBuffer.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(OpenLighting_LIBRARIES
    NAMES olaslpclient
    HINTS ${OpenLighting_INCLUDE_DIRS}/../lib
   /usr/local/lib
   /usr/lib)

IF(OpenLighting_INCLUDE_DIRS)
    IF(OpenLighting_LIBRARIES)
        SET(OpenLighting_FOUND "YES")
        SET(OpenLighting_LIBRARIES ${OpenLighting_LIBRARIES} ${CMAKE_DL_LIBS})
    ENDIF(OpenLighting_LIBRARIES)
ENDIF(OpenLighting_INCLUDE_DIRS)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenLighting DEFAULT_MSG OpenLighting_LIBRARIES OpenLighting_INCLUDE_DIRS)

