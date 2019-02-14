cmake_minimum_required (VERSION 3.0)

set (SECURITY_LIBRARIES
    agoclient
)

# add the executable
add_executable (agosecurity agosecurity.cpp)
target_link_libraries (agosecurity ${SECURITY_LIBRARIES})

install (TARGETS agosecurity RUNTIME DESTINATION ${BINDIR})
AgoService(security DESCRIPTION "ago control security system")
