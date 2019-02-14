cmake_minimum_required (VERSION 3.0)

set(LIBAGOHTTP_SOURCE_FILES
    agohttp.cpp
    mongoose.c
)

add_library (agohttp SHARED ${LIBAGOHTTP_SOURCE_FILES})
set_target_properties (agohttp PROPERTIES VERSION 1 SOVERSION 1)

set (AGOHTTP_LIBRARIES
    agoclient # Mainly for logging
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT} # for pthread
    ${JSONCPP_LIBRARIES}
)

if(OPENSSL_FOUND)
    list(APPEND AGOHTTP_LIBRARIES
        ssl
        crypto
    )

    set(MG_ENABLE_SSL 1)
else()
    set(MG_ENABLE_SSL 0)
endif()

# configure a header file to pass some of the CMake settings
# to the source code
# Note that MG_ flags must be set when building mongoose.c too
configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/agohttp_config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/agohttp_config.h"
        @ONLY
)

# SSI fails to build on GCC 8, we dont use it anyway
# https://github.com/cesanta/mongoose/issues/929
set_property(
        TARGET agohttp
        APPEND PROPERTY COMPILE_DEFINITIONS
        MG_ENABLE_HTTP_SSI=0
        MG_ENABLE_SSL=${MG_ENABLE_SSL}
)
target_link_libraries (agohttp ${AGOHTTP_LIBRARIES})

set_property(
    TARGET agohttp
    APPEND PROPERTY
    COMPILE_OPTIONS
    -Werror)

install (TARGETS agohttp LIBRARY DESTINATION ${LIBDIR})
