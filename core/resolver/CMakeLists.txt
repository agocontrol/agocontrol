cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE AGORESOLVER_SOURCE_FILES *.cpp)

set (RESOLVER_LIBRARIES
    sqlite3
    agoclient
)

IF (FREEBSD)
	set (RESOLVER_LIBRARIES ${RESOLVER_LIBRARIES} yaml-cpp03)
ELSE(FREEBSD)
	set (RESOLVER_LIBRARIES ${RESOLVER_LIBRARIES} yaml-cpp)
ENDIF(FREEBSD)

# add the executable
add_executable (agoresolver ${AGORESOLVER_SOURCE_FILES})
target_link_libraries (agoresolver ${RESOLVER_LIBRARIES})

install (TARGETS agoresolver RUNTIME DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/db)
