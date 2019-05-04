cmake_minimum_required (VERSION 3.0)

file (GLOB CONFD_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.in)

foreach (infile ${CONFD_FILES})

    string(REGEX REPLACE ".in$" "" outfile ${infile})
    string(REGEX REPLACE ".*/" "" outfile ${outfile})

    configure_file(
        "${infile}"
        "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
        @ONLY
    )

    LIST(APPEND CONFD_FILES_DONE ${outfile})

endforeach (infile)

file (GLOB CONFD_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.conf)

CopyFilesFromSource(conf_files "${CONFD_FILES}")

InstallFiles(${CONFDIR}/conf.d "${CONFD_FILES_DONE};${CONFD_FILES}")
