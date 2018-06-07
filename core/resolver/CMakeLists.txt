cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE AGORESOLVER_SOURCE_FILES *.cpp)

set (RESOLVER_LIBRARIES
    sqlite3
    agoclient
	 ${YAML_CPP_LIBRARIES}
)

# add the executable
add_executable (agoresolver ${AGORESOLVER_SOURCE_FILES})
target_link_libraries (agoresolver ${RESOLVER_LIBRARIES})

install (TARGETS agoresolver RUNTIME DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/db)
