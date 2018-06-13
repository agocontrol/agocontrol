cmake_minimum_required (VERSION 3.0)

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/command.lua.in"
    "${CMAKE_CURRENT_BINARY_DIR}/command.lua"
    @ONLY
)

file (GLOB_RECURSE AGOLUA_SOURCE_FILES *.cpp)
set (LUA_LIBRARIES
    agoclient
	${Boost_REGEX_LIBRARY}
	${LUA_LIBRARIES}
)

include_directories(${LUA_INCLUDE_DIRS})

# add the executable
add_executable (agolua ${AGOLUA_SOURCE_FILES})
target_link_libraries (agolua ${LUA_LIBRARIES})

install (TARGETS agolua RUNTIME DESTINATION ${BINDIR})
AgoService(lua DESCRIPTION "ago control LUA script event handler")
