cmake_minimum_required (VERSION 3.0)


set(LIBAGOCLIENT_SOURCE_FILES
    agoclient.cpp
    agoproto.cpp
    agojson.cpp
    agojson-qpid.cpp
    jsoncpp/jsoncpp.cpp
    agolog.cpp
    agoapp.cpp
    agoconfig.cpp
    base64.cpp
)

IF (Boost_LOG_FOUND)
    list(APPEND LIBAGOCLIENT_SOURCE_FILES agolog_boost.cpp)
else()
    list(APPEND LIBAGOCLIENT_SOURCE_FILES agolog_basic.cpp)
endif()

add_library (agoclient SHARED ${LIBAGOCLIENT_SOURCE_FILES})
set_target_properties (agoclient PROPERTIES VERSION 1 SOVERSION 1)

set (AGOCLIENT_LIBRARIES
    qpidmessaging
    qpidtypes
    uuid
    augeas
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT} # for pthread
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

IF (Boost_LOG_FOUND)
    LIST(APPEND AGOCLIENT_LIBRARIES ${Boost_LOG_LIBRARY})
ELSE()
    LIST(APPEND AGOCLIENT_LIBRARIES ${Boost_DATE_TIME_LIBRARY})
ENDIF()

target_link_libraries (agoclient ${AGOCLIENT_LIBRARIES})

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
