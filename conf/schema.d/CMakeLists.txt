cmake_minimum_required (VERSION 3.0)

file (GLOB SCHEMA_FILES *.yaml)

install (FILES ${SCHEMA_FILES} DESTINATION ${CONFDIR}/schema.d)
