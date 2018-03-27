cmake_minimum_required (VERSION 3.0)

file (GLOB CONFD_FILES *.in)

foreach (infile ${CONFD_FILES})

    string(REGEX REPLACE ".in$" "" outfile ${infile})
    string(REGEX REPLACE ".*/" "" outfile ${outfile})


    configure_file(
        "${infile}"
        "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
        @ONLY
    )

    LIST(APPEND CONFD_FILES_DONE ${CMAKE_CURRENT_BINARY_DIR}/${outfile})

endforeach (infile)

file (GLOB CONFD_FILES *.conf)
install (FILES ${CONFD_FILES_DONE} ${CONFD_FILES} DESTINATION ${CONFDIR}/conf.d)
