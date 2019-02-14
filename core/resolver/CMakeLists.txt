cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE AGORESOLVER_SOURCE_FILES *.cpp)

set (RESOLVER_LIBRARIES
    sqlite3
    agoclient
    ${YAML_CPP_LIBRARIES}
)

string(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*" "\\1" YAML_CPP_MAJOR_VERSION "${YAML_CPP_VERSION}")
string(REGEX REPLACE "^([0-9]+)\\.([0-9]+).*" "\\2" YAML_CPP_MINOR_VERSION "${YAML_CPP_VERSION}")
add_definitions(-DYAML_CPP_VERSION=${YAML_CPP_MAJOR_VERSION}${YAML_CPP_MINOR_VERSION})
add_definitions(-DYAML_CPP_MAJOR_VERSION=${YAML_CPP_MAJOR_VERSION}
   -DYAML_CPP_MINOR_VERSION=${YAML_CPP_MINOR_VERSION})

# add the executable
add_executable (agoresolver ${AGORESOLVER_SOURCE_FILES})
target_link_libraries (agoresolver ${RESOLVER_LIBRARIES})

install (TARGETS agoresolver RUNTIME DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/db)

AgoService(resolver DESCRIPTION "ago control Resolver")
