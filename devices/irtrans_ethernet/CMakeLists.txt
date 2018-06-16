cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     agoclient
)

set (DEVICE "agoirtrans_ethernet")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})

AgoService(irtrans_ethernet DESCRIPTION "ago control IRTrans Ethernet infrared blaster Device")
