cmake_minimum_required (VERSION 3.0)

set (SYSTEM_LIBRARIES
    agoclient
    ${Boost_PROGRAM_DATE_TIME_LIBRARY}
    )

set (AGOSYSTEM_SOURCE_FILES agosystem.cpp)
IF (FREEBSD)
    list(APPEND AGOSYSTEM_SOURCE_FILES processinfo_freebsd.cpp)
    list(APPEND SYSTEM_LIBRARIES kvm)
ELSE()
    list(APPEND AGOSYSTEM_SOURCE_FILES processinfo_linux.cpp)
    list(APPEND SYSTEM_LIBRARIES ${LIBPROCPS_LIBRARIES})

    INCLUDE (CheckLibraryExists)
    CHECK_LIBRARY_EXISTS(procps freeproc "" FREEPROC_EXISTS)
    IF (FREEPROC_EXISTS)
        ADD_DEFINITIONS(-DFREEPROC_EXISTS)
    ENDIF (FREEPROC_EXISTS)
ENDIF()

# add the executable
add_executable (agosystem ${AGOSYSTEM_SOURCE_FILES})
target_link_libraries (agosystem ${SYSTEM_LIBRARIES})

install (TARGETS agosystem RUNTIME DESTINATION ${BINDIR})
AgoService(system DESCRIPTION "ago control system controller")
