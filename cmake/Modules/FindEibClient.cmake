# Find the EibClient includes and library
#
# This module defines
# EibClient_INCLUDE_DIRS, where to find eibclient.h
# EibClient_LIBRARIES, the libraries to link against to use it
# EibClient_FOUND, If false, do not try to use it

FIND_PATH(EibClient_INCLUDE_DIRS eibclient.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(EibClient_LIBRARIES eibclient
   ${EibClient_INCLUDE_DIRS}/../lib
   /usr/local/lib
   /usr/local/lib64
   /usr/lib)

if(EibClient_INCLUDE_DIRS)
    IF(EibClient_LIBRARIES)
        SET(EibClient_FOUND "YES")
        SET(EibClient_LIBRARIES ${EibClient_LIBRARIES} ${CMAKE_DL_LIBS})
    endif()
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EibClient DEFAULT_MSG EibClient_LIBRARIES EibClient_INCLUDE_DIRS)

