# Find the OpenZWave includes and library
#
# This module defines
# OpenZWave_INCLUDE_DIRS, where to find openzwave/Manager.h, etc.
# OpenZWave_LIBRARIES, the libraries to link against to use it
# OpenZWave_FOUND, If false, do not try to use it

FIND_PATH(OpenZWave_INCLUDE_BASE_DIR openzwave/Manager.h
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(OpenZWave_LIBRARIES openzwave
   ${OpenZWave_INCLUDE_BASE_DIR}/../lib
   /usr/local/lib
   /usr/local/lib64
   /usr/lib)

if(OpenZWave_INCLUDE_BASE_DIR)
    IF(OpenZWave_LIBRARIES)
        set(OpenZWave_FOUND "YES")
        set(OpenZWave_LIBRARIES ${OpenZWave_LIBRARIES})
        set(OpenZWave_INCLUDE_DIRS ${OpenZWave_INCLUDE_BASE_DIR} ${OpenZWave_INCLUDE_BASE_DIR}/openzwave)

        IF(NOT DEFINED "OpenZWave_VERSION")
            # This mimics CHECK_CXX_SOURCE_RUNS, but need to do manually to use RUN_OUTPUT_VARIABLE
            # Determine which version of OpenZWave we have
            set(CMAKE_REQUIRED_INCLUDES "${OpenZWave_INCLUDE_DIRS}")
            set(CMAKE_REQUIRED_LIBRARIES "${OpenZWave_LIBRARIES}")

            set(SOURCE "#include <iostream>
    #include <stdint.h>
    extern uint16_t ozw_vers_major;
    extern uint16_t ozw_vers_minor;
    extern uint16_t ozw_vers_revision;
    int main(void){
        std::cout << ozw_vers_major << ';' << ozw_vers_minor << ';' << ozw_vers_revision;
        return 0;
    }")

            set(MACRO_CHECK_FUNCTION_DEFINITIONS "-DOpenZWave_VERSION ${CMAKE_REQUIRED_FLAGS}")
            set(CHECK_CXX_SOURCE_COMPILES_ADD_LIBRARIES
                    LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
            set(CHECK_CXX_SOURCE_COMPILES_ADD_INCLUDES
                    "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}")
            file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.cxx"
                    "${SOURCE}\n")

            if(OS_DARWIN)
                # On Darwin it tries to search system paths for library, even if given explicit path
                # On FreeBSD (and probably linux?) it uses the rpath flag to link to specific library instead.
                set(RESTORE_LIBRARY_PATH "$ENV{DYLD_LIBRARY_PATH}")
                get_filename_component(OpenZWave_LIBRARY_PATH "${OpenZWave_LIBRARIES}" DIRECTORY)
                set(ENV{DYLD_LIBRARY_PATH} "${OpenZWave_LIBRARY_PATH}:$ENV{DYLD_LIBRARY_PATH}")
            endif()

            try_run(OpenZWave_VERSION_EXITCODE OpenZWave_VERSION_COMPILED
                    ${CMAKE_BINARY_DIR}
                    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/src.cxx
                    COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
                    ${CHECK_CXX_SOURCE_COMPILES_ADD_LIBRARIES}
                    CMAKE_FLAGS -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_FUNCTION_DEFINITIONS}
                    -DCMAKE_SKIP_RPATH:BOOL=${CMAKE_SKIP_RPATH}
                    "${CHECK_CXX_SOURCE_COMPILES_ADD_INCLUDES}"
                    COMPILE_OUTPUT_VARIABLE BUILD_OUTPUT
                    RUN_OUTPUT_VARIABLE OpenZWave_VERSION)

            if(OS_DARWIN)
                set(ENV{DYLD_LIBRARY_PATH} "${RESTORE_DYLD_LIBRARY_PATH}")
            endif()

            if(NOT OpenZWave_VERSION_COMPILED OR OpenZWave_VERSION_EXITCODE)
                file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
                        "Performing OpenZWave_VERSION test failed.\n"
                        "Compile result: ${OpenZWave_VERSION_COMPILED}\n"
                        "Return value: ${OpenZWave_VERSION_EXITCODE}\n"
                        "Compile output:\n"
                        "${BUILD_OUTPUT}\n"
                        "Run output:\n"
                        "${OpenZWave_VERSION}\n"
                        "Source file was:\n${SOURCE}\n")
                message(FATAL_ERROR "OpenZWave found but failed to determine version.\n"
                        "See CMakeError.log for more details.")
            endif()

            set(CMAKE_REQUIRED_INCLUDES)
            set(CMAKE_REQUIRED_LIBRARIES)

            set( OpenZWave_VERSION_LIST ${OpenZWave_VERSION} )
            separate_arguments(OpenZWave_VERSION_LIST)
            list(GET OpenZWave_VERSION_LIST 0 OpenZWave_VERSION_MAJOR)
            list(GET OpenZWave_VERSION_LIST 1 OpenZWave_VERSION_MINOR)
            list(GET OpenZWave_VERSION_LIST 2 OpenZWave_VERSION_REVISION)

            set(OpenZWave_VERSION "${OpenZWave_VERSION}" CACHE INTERNAL "OpenZWave version")
            set(OpenZWave_VERSION_MAJOR "${OpenZWave_VERSION_MAJOR}" CACHE INTERNAL "OpenZWave major version")
            set(OpenZWave_VERSION_MINOR "${OpenZWave_VERSION_MINOR}" CACHE INTERNAL "OpenZWave minor version")
            set(OpenZWave_VERSION_REVISION "${OpenZWave_VERSION_REVISION}" CACHE INTERNAL "OpenZWave GIT revision")
        endif()

        message(STATUS "Found OpenZWave version: ${OpenZWave_VERSION}")
    endif()
endif()

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenZWave DEFAULT_MSG OpenZWave_LIBRARIES OpenZWave_INCLUDE_DIRS)
