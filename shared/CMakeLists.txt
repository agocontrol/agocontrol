cmake_minimum_required (VERSION 3.0)

set(LIBAGOCLIENT_SOURCE_FILES
    agoclient.cpp
    agoapp.cpp
    agoconfig.cpp
    base64.cpp
)

set(LIBAGOCORE_SOURCE_FILES
    agoproto.cpp
    agojson.cpp
    jsoncpp/jsoncpp.cpp
    agoutils.cpp
    agolog.cpp
)

if(Boost_LOG_FOUND)
    list(APPEND LIBAGOCORE_SOURCE_FILES agolog_boost.cpp)
else()
    list(APPEND LIBAGOCORE_SOURCE_FILES agolog_basic.cpp)
endif()

set (AGOCLIENT_LIBRARIES
    agocore
    dl
    ${AUGEAS_LIBRARIES}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

set (AGOCORE_LIBRARIES
    uuid
    ${Boost_LOG_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT} # for pthread
)

if(Boost_LOG_FOUND)
    LIST(APPEND AGOCORE_LIBRARIES ${Boost_LOG_LIBRARY})
else()
    LIST(APPEND AGOCORE_LIBRARIES ${Boost_DATE_TIME_LIBRARY})
endif()


set_property(DIRECTORY
    APPEND PROPERTY COMPILE_DEFINITIONS
    DEFAULT_CONFDIR=${CONFDIR}
    DEFAULT_LOCALSTATEDIR=${LOCALSTATEDIR}
)

set_property(DIRECTORY
    APPEND PROPERTY
    COMPILE_OPTIONS
    -Werror)

include_directories("${AUGEAS_INCLUDE_DIRS}")

add_library (agoclient SHARED ${LIBAGOCLIENT_SOURCE_FILES})
set_target_properties (agoclient PROPERTIES VERSION 1 SOVERSION 1)
target_link_libraries (agoclient ${AGOCLIENT_LIBRARIES})

add_library (agocore SHARED ${LIBAGOCORE_SOURCE_FILES})
set_target_properties (agocore PROPERTIES VERSION 1 SOVERSION 1)
target_link_libraries(agocore ${AGOCORE_LIBRARIES})

set(AGOLIBS agoclient agocore)

if(WITH_MQTT)
    add_library (agotransport-mqtt SHARED agotransport-mqtt.cpp)
    set_target_properties (agotransport-mqtt PROPERTIES VERSION 1 SOVERSION 1)
    target_link_libraries(agotransport-mqtt agocore ${MOSQUITTOPP_LIBRARIES} ${Boost_THREAD_LIBRARY} ${Boost_LOG_LIBRARY})
    list(APPEND AGOLIBS agotransport-mqtt)
endif()

if(WITH_QPID)
    add_library (agotransport-qpid SHARED agotransport-qpid.cpp agojson-qpid.cpp)
    set_target_properties (agotransport-qpid PROPERTIES VERSION 1 SOVERSION 1)
    target_link_libraries(agotransport-qpid agocore ${QPID_LIBRARIES} ${Boost_LOG_LIBRARY})
    list(APPEND AGOLIBS agotransport-qpid)
endif()


install (TARGETS ${AGOLIBS} LIBRARY DESTINATION ${LIBDIR})


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
