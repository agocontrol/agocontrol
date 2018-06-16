cmake_minimum_required (VERSION 3.0)

IF(COMMAND cmake_policy)
    # FILE GLOB_RECURSE calls should not follow symlinks by default.
    cmake_policy(SET CMP0009 NEW)
ENDIF(COMMAND cmake_policy)

set (RPC_LIBRARIES
    agoclient
    agohttp
)

set (RPCPASSWD_LIBRARIES
    agohttp
)

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
AgoService(rpc DESCRIPTION "ago control HTTP RPC interface")


get_filename_component(CMAKE_CURRENT_SOURCE_DIR_REAL "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
get_filename_component(CMAKE_CURRENT_BINARY_DIR_REAL "${CMAKE_CURRENT_BINARY_DIR}" REALPATH)

# This is quite similar to our CopyFilesFromSource function, but does some extra filtering,
# since we do globbing and don't want to copy .in.html or dot-files.
if (NOT IN_SOURCE_BUILD)
    file(GLOB_RECURSE HTML_FILES ${CMAKE_CURRENT_SOURCE_DIR}/html/*)
    add_custom_target(html-ui ALL)
    add_dependencies(agorpc html-ui)
    foreach (infile ${HTML_FILES})
        # Almost all...
        if (NOT infile MATCHES ".in.html$"
                AND NOT infile MATCHES "/\\."
                AND NOT IS_SYMLINK "${infile}")
            string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} outfile ${infile})
            add_custom_command(
                TARGET html-ui DEPENDS ${infile}
                COMMAND ${CMAKE_COMMAND} -E copy ${infile} ${outfile})
            set (UI_FILES ${UI_FILES} ${outfile})
        endif()
    endforeach(infile)
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
add_dependencies(agorpc translations)

# These JS files are loaded on main page. Build a merged file so we can load quicker...
# Poor mans minifying...
set(BASE_JS_FILES
    libs/jquery-1.11.1.min.js libs/jquery-ui.min.js libs/jquery.blockUI.min.js libs/jquery.jeditable.min.js
    libs/d3.v3.min.js
    libs/agonotifications.js lang.js
    libs/knockout-3.4.2.js libs/koExternalTemplateEngine_all.js libs/knockout.agoGrid.1.0.js
    libs/bluebird.min.js libs/bootstrap.min.js libs/bootstrap-switch.min.js
    libs/bootstrap-slider.min.js libs/bootstrap-colorpicker.min.js
    libs/select2.full.min.js libs/head.load.min.js libs/sammy.min.js
    libs/jquery.datetimepicker.min.js
    agocontrol.js
    device.js
    details.js
    uihelpers.js
    plugins.js
    app.js
    libs/jquery.slimscroll.min.js
    libs/bootstrap-switch.min.js
    libs/adminLTE.min.js
)

foreach(infile ${BASE_JS_FILES})
    set (BASE_JS_FULL_FILES ${BASE_JS_FULL_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/html/js/${infile})
endforeach()

set(BASE_JS_OUTFILE ${CMAKE_CURRENT_BINARY_DIR}/html/js/merged.js)
add_custom_command(
        OUTPUT ${BASE_JS_OUTFILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mergesrc.sh ${BASE_JS_FULL_FILES}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mergesrc.sh ${BASE_JS_OUTFILE} ${BASE_JS_FULL_FILES})

add_custom_target(merged_js DEPENDS ${BASE_JS_OUTFILE})
add_dependencies(agorpc merged_js)

# Same ugly merging for CSS files
set(BASE_CSS_FILES
    fonts.css
    bootstrap.min.css
    agocontrol.css
    bootstrap-switch.min.css
    bootstrap-slider.min.css
    bootstrap-colorpicker.min.css
    select2.min.css
    font-awesome.min.css
    entypo-embedded.css
    jquery.datetimepicker.css
    adminLTE.min.css
    adminLTE.skins.min.css
)

foreach(infile ${BASE_CSS_FILES})
    set (BASE_CSS_FULL_FILES ${BASE_CSS_FULL_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/html/css/${infile})
endforeach()

set(BASE_CSS_OUTFILE ${CMAKE_CURRENT_BINARY_DIR}/html/css/merged.css)
add_custom_command(
        OUTPUT ${BASE_CSS_OUTFILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mergesrc.sh ${BASE_CSS_FULL_FILES}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/mergesrc.sh ${BASE_CSS_OUTFILE} ${BASE_CSS_FULL_FILES})

add_custom_target(merged_css DEPENDS ${BASE_CSS_OUTFILE})
add_dependencies(agorpc merged_css)

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

