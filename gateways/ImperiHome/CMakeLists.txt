cmake_minimum_required (VERSION 3.0)

IF(COMMAND cmake_policy)
    cmake_policy(SET CMP0009 NEW)
ENDIF(COMMAND cmake_policy)

set (IMPERIHOME_LIBRARIES
    agoclient
    agohttp
)

IF (NOT FREEBSD)
    list(APPEND IMPERIHOME_LIBRARIES dl )
ENDIF (NOT FREEBSD)

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
