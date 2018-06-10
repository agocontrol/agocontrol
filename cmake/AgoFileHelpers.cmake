# Helper function to copy non-compiled files from source directory to binary directory
# as-is, without having to re-run cmake every time to copy (as is requried by configure_file(...COPYONLY))
# This is mostly useful for developer when running from binary dir without "make install" step.
#
# Usage:
#
#   CopyFilesFromSource(someTarget "FILE1 FILE2 FILE3...")
#
#       someTarget is a unique cmake target name to create
#       This will be added as a default target ("ALL") for the current directory
#       The FILE... argument should be a list of filenames, relative to current source directory,
#       which are to be copied to the build dir, keeping the same relative directory structure.
function(CopyFilesFromSource TARGET SOURCE_FILES)
    if (NOT IN_SOURCE_BUILD)
        add_custom_target(${TARGET} ALL SOURCES ${SOURCE_FILES})
        foreach (infile ${SOURCE_FILES})
            string(REPLACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} outfile ${infile})
            #message("-- staging for copy: ${CMAKE_CURRENT_SOURCE_DIR}/${infile} -> ${CMAKE_CURRENT_BINARY_DIR}/${outfile}")
            add_custom_command(
                    TARGET ${TARGET}
                    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${infile} ${outfile}
                    VERBATIM
            )
        endforeach()
    endif()
endfunction()

# Helper to create "install" jobs for multiple files in the binary directory
function(InstallFiles DESTINATION SOURCE_FILES)
    foreach (infile ${SOURCE_FILES})
        install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${infile} DESTINATION ${DESTINATION})
    endforeach()
endfunction()


# Helper to define and install a Python program.
# With defaults, this will install the ago${PROGRAM_BASE_NAME}.py file into
# /opt/agocontrol/python/ago${PROGRAM_BASE_NAME}/
# and also create a symlink at /opt/agocontrol/bin/${PROGRAM_BASE_NAME} pointing to the main .py file
# Any extra files will be copied to the same "private" directory, to avoid poluting the main bin dir
# and to avoid conflicts.
#
# Usage:
#   AgoPythonProgram(myapp DESCRIPTION "ago control blaha handler" [WITH_CONFIGDIR ] [EXTRA_FILES "..."])
#
#   Will setup targets to copy agomyapp.py and any extra files to the CMAKE_CURRENT_BINARY_DIR,
#   and setup install to then copy these files to the appropriate installation dir.
#   Also prepares a conf/systemd/agomyapp.service file and installs it, unless a .in with same name
#   exists in the source dir.
#
#   If WITH_CONFIGDIR is set, it will enable installation of directory /etc/opt/agocontrol/myapp.
#   If EXTRA_FILES is specified, these will be copied to the "private" dir next to the .py file.
#   If RUN_AS is specified, systemd will have that User instead of default user.
#
# Note that PROGRAM_BASE_NAME should not have ago prefix, nor .py suffix.
# The directory containing this directive must have a file named ago<name>.py.
#
function(AgoPythonProgram PROGRAM_BASE_NAME)
    set(options WITH_CONFIGDIR)
    set(oneValueArgs DESCRIPTION RUN_AS)
    set(multiValueArgs EXTRA_FILES)
    cmake_parse_arguments(PYPROG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROGRAM_NAME "ago${PROGRAM_BASE_NAME}")
    set(APP_DIR "${BASEDIR}/python/${PROGRAM_NAME}")

    CopyFilesFromSource("${PROGRAM_NAME}" "${PROGRAM_NAME}.py;${PYPROG_EXTRA_FILES}")

    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PROGRAM_NAME}.py DESTINATION ${APP_DIR})
    if(PYPROG_WITH_CONFIGDIR)
        install(DIRECTORY DESTINATION ${CONFDIR}/${PROGRAM_BASE_NAME})
    endif()

    AgoService(${PROGRAM_BASE_NAME} DESCRIPTION "${PYPROG_DESCRIPTION}" RUN_AS "${PYPROG_RUN_AS}")

    install(CODE "
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ../python/${PROGRAM_NAME}/${PROGRAM_NAME}.py \$ENV{DESTDIR}${BINDIR}/${PROGRAM_NAME})
        message(\"-- Installing symlink: \$ENV{DESTDIR}${BINDIR}/${PROGRAM_NAME} -> ../python/${PROGRAM_NAME}/${PROGRAM_NAME}.py\")
    ")

    foreach (infile ${PYPROG_EXTRA_FILES})
        get_filename_component(OUTDIR "${APP_DIR}/${infile}" DIRECTORY)
        install (FILES "${CMAKE_CURRENT_BINARY_DIR}/${infile}" DESTINATION "${OUTDIR}")
    endforeach()

endfunction()


function(AgoService PROGRAM_BASE_NAME)
    set(options )
    set(oneValueArgs DESCRIPTION RUN_AS)
    set(multiValueArgs )
    cmake_parse_arguments(PROG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROGRAM_NAME "ago${PROGRAM_BASE_NAME}")
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/conf/systemd/${PROGRAM_NAME}.service.in")
        if(NOT PROG_RUN_AS)
            set(PROG_RUN_AS "agocontrol")
        endif()
        #message("Creating service file for ${PROGRAM_NAME} ${PROG_DESCRIPTION} AND ${PROG_RUN_AS}")
        configure_file("${PROJECT_SOURCE_DIR}/conf/systemd/template.service.in" ${PROJECT_BINARY_DIR}/conf/systemd/${PROGRAM_NAME}.service)
        install (FILES "${PROJECT_BINARY_DIR}/conf/systemd/${PROGRAM_NAME}.service" DESTINATION "/lib/systemd/system/")
    endif()
endfunction()