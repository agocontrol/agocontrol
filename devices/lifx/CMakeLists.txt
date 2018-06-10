cmake_minimum_required (VERSION 3.0)


# Files needed for lifxlan
# Installing local copy of lifxlan for now. CDhnages accepted by maintainer but still not available in standard package
# https://github.com/mclarkk/lifxlan
FILE(GLOB LIFXLAN_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} lifxlan/*.py)

set(EXTRA_FILES lifxbase.py lifxnet.py lifxlan2.py prodinfo.json ${LIFXLAN_FILES})

AgoPythonProgram(lifx
    DESCRIPTION "ago control LIFX handler"
    EXTRA_FILES "${EXTRA_FILES}")