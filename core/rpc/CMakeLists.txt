cmake_minimum_required (VERSION 2.6)

IF(COMMAND cmake_policy)
    cmake_policy(SET CMP0009 NEW)
ENDIF(COMMAND cmake_policy)

set (RPC_LIBRARIES
    agoclient
    agohttp
    pthread
)

set (RPCPASSWD_LIBRARIES
    pthread
    agohttp
)

IF (NOT FREEBSD)
	list(APPEND RPC_LIBRARIES dl )
	list(APPEND RPCPASSWD_LIBRARIES dl )
ENDIF (NOT FREEBSD)

# add the executable
add_executable (agorpc agorpc.cpp)
target_link_libraries (agorpc ${RPC_LIBRARIES})

add_executable (get_table_lang.cgi get_table_lang.cpp)

add_executable (rpc-htpasswd rpc-htpasswd.cpp)
target_link_libraries (rpc-htpasswd ${RPCPASSWD_LIBRARIES})

set_property(
	TARGET agorpc
	PROPERTY COMPILE_DEFINITIONS
	DEFAULT_HTMLDIR=${HTMLDIR}
)


# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/activate.cgi.in"
    "${CMAKE_CURRENT_BINARY_DIR}/html/cgi-bin/activate.cgi"
    @ONLY
)

install (TARGETS agorpc rpc-htpasswd RUNTIME DESTINATION ${BINDIR})
install (TARGETS get_table_lang.cgi RUNTIME DESTINATION ${HTMLDIR}/cgi-bin)


get_filename_component(CMAKE_CURRENT_SOURCE_DIR_REAL "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(CMAKE_CURRENT_BINARY_DIR_REAL "${CMAKE_CURRENT_BINARY_DIR}" REALPATH)

# If we are NOT in-source build, we have to copy all files below html/ to the build dir.
if (NOT "${CMAKE_CURRENT_SOURCE_DIR_REAL}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR_REAL}")
    file(GLOB_RECURSE HTML_FILES ${CMAKE_CURRENT_SOURCE_DIR}/html/*)
    foreach (infile ${HTML_FILES})
        # Almost all...
        if (NOT infile MATCHES ".in.html$"
                AND NOT infile MATCHES "/\\."
                AND NOT IS_SYMLINK "${infile}")
            # This is quite verbose, logging every file, but have not found any way to
            # have a dependency on input file -> output file without this...
            string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} outfile ${infile})
            add_custom_command(
                OUTPUT ${outfile} DEPENDS ${infile}
                COMMAND ${CMAKE_COMMAND} -E copy ${infile} ${outfile})
            set (UI_FILES ${UI_FILES} ${outfile})
        endif()
    endforeach(infile)
    add_custom_target(html-ui DEPENDS ${UI_FILES})
endif()


# For every .in.html file, pass it through the mergetranslation.sh script
file(GLOB_RECURSE HTML_IN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/html/*.in.html)
foreach (infile ${HTML_IN_FILES})
        string(REGEX REPLACE ".in.html$" ".html" outfile ${infile})
        string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} outfile ${outfile})
        add_custom_command(
                OUTPUT ${outfile}
                DEPENDS ${infile}
                COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mergetranslation.sh ${infile} ${outfile})
        set (OUT_FILES ${OUT_FILES} ${outfile})
endforeach (infile)

add_custom_target(translations DEPENDS ${OUT_FILES})
add_dependencies(agorpc html-ui translations)


install(
   CODE "
   file(GLOB CGI_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/html/cgi-bin/*.cgi)
   file(INSTALL \${CGI_SCRIPTS} DESTINATION ${HTMLDIR}/cgi-bin/ FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
   file(REMOVE \${CGI_SCRIPTS})
   file(GLOB HTML_FILES ${CMAKE_CURRENT_BINARY_DIR}/html/*) \n
   file(INSTALL DESTINATION ${HTMLDIR}
   TYPE FILE FILES \${HTML_FILES})
   "
)

