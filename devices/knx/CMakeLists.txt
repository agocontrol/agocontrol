cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
    ${EibClient_LIBRARIES}
    ${TinyXML2_LIBRARIES}
    agoclient
)

set (DEVICE "agoknx")

include_directories(${EibClient_INCLUDE_DIRS})

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})

AgoService(knx DESCRIPTION "ago control KNX Device")
