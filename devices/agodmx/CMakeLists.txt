cmake_minimum_required (VERSION 2.6)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     pthread
     tinyxml2
     ${OpenLighting_LIBRARIES} # olaslpclient + any path
     olaslpcore
     ola
     olacommon
     agoclient
)

include_directories(${OpenLighting_INCLUDE_DIRS})

set (DEVICE "agodmx")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
