cmake_minimum_required (VERSION 3.0)


set(LIBAGOCLIENT_SOURCE_FILES
    agoclient.cpp
    agoproto.cpp
    agojson.cpp
    jsoncpp/jsoncpp.cpp
    agolog.cpp
    agoapp.cpp
    agoconfig.cpp
    base64.cpp
)

if(Boost_LOG_FOUND)
    list(APPEND LIBAGOCLIENT_SOURCE_FILES agolog_boost.cpp)
else()
    list(APPEND LIBAGOCLIENT_SOURCE_FILES agolog_basic.cpp)
endif()

if(WITH_MQTT)
    list(APPEND LIBAGOCLIENT_SOURCE_FILES agoconnection-mqtt.cpp)
endif()

if(WITH_QPID)
    list(APPEND LIBAGOCLIENT_SOURCE_FILES agoconnection-qpid.cpp agojson-qpid.cpp)
endif()

add_library (agoclient SHARED ${LIBAGOCLIENT_SOURCE_FILES})
set_target_properties (agoclient PROPERTIES VERSION 1 SOVERSION 1)

set (AGOCLIENT_LIBRARIES
    uuid
    ${QPID_LIBRARIES}
    ${MOSQUITTOPP_LIBRARIES}
    ${AUGEAS_LIBRARIES}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT} # for pthread
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

if(Boost_LOG_FOUND)
    LIST(APPEND AGOCLIENT_LIBRARIES ${Boost_LOG_LIBRARY})
else()
    LIST(APPEND AGOCLIENT_LIBRARIES ${Boost_DATE_TIME_LIBRARY})
endif()

target_link_libraries (agoclient ${AGOCLIENT_LIBRARIES})

include_directories("${AUGEAS_INCLUDE_DIRS}")

set_property(
    TARGET agoclient
    PROPERTY COMPILE_DEFINITIONS
    DEFAULT_CONFDIR=${CONFDIR}
    DEFAULT_LOCALSTATEDIR=${LOCALSTATEDIR}
)

set_property(
    TARGET agoclient
    APPEND PROPERTY
    COMPILE_OPTIONS
    -Werror)

install (TARGETS agoclient LIBRARY DESTINATION ${LIBDIR})
add_subdirectory (agohttp)
install (DIRECTORY DESTINATION ${CONFDIR}/uuidmap)

####### Python version of agoclient

configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/agoclient/_directories.py.in"
        "${CMAKE_CURRENT_BINARY_DIR}/agoclient/_directories.py"
        @ONLY
)

set(PYTHON_AGOCLIENT_SOURCE_FILES
    agoclient/agoapp.py
    agoclient/agoconnection.py
    agoclient/config.py
    agoclient/__init__.py
    agoclient/_logging.py
)

CopyFilesFromSource(agoclient-python "${PYTHON_AGOCLIENT_SOURCE_FILES}")

InstallFiles(${PYTHON2DIST}/agoclient "${PYTHON_AGOCLIENT_SOURCE_FILES};agoclient/_directories.py")
