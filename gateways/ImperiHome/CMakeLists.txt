cmake_minimum_required (VERSION 3.0)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0009 NEW)
endif()

set (IMPERIHOME_LIBRARIES
    agoclient
    agohttp
)

# add the executable
add_executable (agoImperiHome agoImperiHome.cpp)
target_link_libraries (agoImperiHome ${IMPERIHOME_LIBRARIES})

set_property(
    TARGET agoImperiHome
    PROPERTY COMPILE_DEFINITIONS
    DEFAULT_HTMLDIR=${HTMLDIR}
)

install (TARGETS agoImperiHome RUNTIME DESTINATION ${BINDIR})

AgoService(ImperiHome DESCRIPTION "ago control ImperiHome App gateway")
