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
    ssl
    crypto
    ${JSONCPP_LIBRARIES}
)

target_link_libraries (agohttp ${AGOHTTP_LIBRARIES})

set_property(
    TARGET agohttp
    PROPERTY COMPILE_DEFINITIONS
    MG_ENABLE_SSL=1
)

set_property(
    TARGET agohttp
    APPEND PROPERTY
    COMPILE_OPTIONS
    -Werror)

install (TARGETS agohttp LIBRARY DESTINATION ${LIBDIR})
