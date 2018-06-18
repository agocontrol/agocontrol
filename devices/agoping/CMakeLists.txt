cmake_minimum_required (VERSION 3.0)

AgoPythonProgram(ping DESCRIPTION "ago control Host pinger")

install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/agoping.conf DESTINATION ${CONFDIR}/conf.d)
