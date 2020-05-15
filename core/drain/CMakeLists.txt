cmake_minimum_required (VERSION 3.0)

set (DRAIN_LIBRARIES
    agoclient
    ${QPID_LIBRARIES}
)

# add the executable
add_executable (agodrain agodrain.cpp)
target_link_libraries (agodrain ${DRAIN_LIBRARIES})

install (TARGETS agodrain RUNTIME DESTINATION ${BINDIR})
