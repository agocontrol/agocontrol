cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE AGODATALOGGER_SOURCE_FILES *.cpp)
set (DATALOGGER_LIBRARIES
    agoclient
    sqlite3
    ${Boost_PROGRAM_DATE_TIME_LIBRARY}
    ${LIBRRD_LIBRARIES}
    ${CppDb_LIBRARIES}
)

include_directories(${CppDb_INCLUDE_DIRS})

# add the executable
add_executable (agodatalogger ${AGODATALOGGER_SOURCE_FILES})
target_link_libraries (agodatalogger ${DATALOGGER_LIBRARIES})

install (TARGETS agodatalogger RUNTIME DESTINATION ${BINDIR})

AgoService(datalogger DESCRIPTION "ago control Data Logger")
