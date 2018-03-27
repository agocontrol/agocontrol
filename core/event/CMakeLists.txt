cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE AGOEVENT_SOURCE_FILES *.cpp)
set (EVENT_LIBRARIES
    agoclient
)

# add the executable
add_executable (agoevent ${AGOEVENT_SOURCE_FILES})
target_link_libraries (agoevent ${EVENT_LIBRARIES})

install (TARGETS agoevent RUNTIME DESTINATION ${BINDIR})
