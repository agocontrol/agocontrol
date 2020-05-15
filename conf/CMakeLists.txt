cmake_minimum_required (VERSION 3.0)

file (GLOB INIT_FILES *.in)

foreach (infile ${INIT_FILES})

    string(REGEX REPLACE ".in$" "" outfile ${infile})
    string(REGEX REPLACE ".*/" "" outfile ${outfile})


    configure_file(
        "${infile}"
        "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
        @ONLY
    )

endforeach (infile)

CopyFilesFromSource(augeas_lens "agocontrol.aug")

add_subdirectory (conf.d)
add_subdirectory (schema.d)
add_subdirectory (sysvinit)

if(OS_LINUX)
    install (FILES security-limits.conf DESTINATION ${ETCDIR}/security/limits.d RENAME agocontrol.conf)
    install (FILES sysctl.conf DESTINATION ${ETCDIR}/sysctl.d RENAME agocontrol.conf)
endif()

install (FILES rpc_cert.pem DESTINATION ${CONFDIR}/rpc)
install (FILES agocontrol.aug DESTINATION ${AUGEASLENSEDIR})
