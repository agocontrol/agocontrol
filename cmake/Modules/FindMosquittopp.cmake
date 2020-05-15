# From https://github.com/romaniucradu/Calculated/blob/master/cmake_modules/FindMosquittopp.cmake

# - Find libmosquitto
# Find the native libmosquitto includes and libraries
#
#  Mosquittopp_INCLUDE_DIR - where to find mosquitto.h, etc.
#  Mosquittopp_LIBRARIES   - List of libraries when using libmosquitto.
#  Mosquittopp_FOUND       - True if libmosquitto found.

if(Mosquittopp_INCLUDE_DIR)
    # Already in cache, be silent
    set(Mosquittopp_FIND_QUIETLY TRUE)
endif(Mosquittopp_INCLUDE_DIR)

find_path(Mosquittopp_INCLUDE_DIR mosquitto.h)

find_library(Mosquittopp_LIBRARY NAMES libmosquittopp mosquittopp)
find_library(Mosquitto_LIBRARY NAMES libmosquitto mosquitto)

# Handle the QUIETLY and REQUIRED arguments and set Mosquittopp_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Mosquittopp DEFAULT_MSG Mosquittopp_LIBRARY Mosquittopp_INCLUDE_DIR)

if(Mosquittopp_FOUND)
    set(Mosquittopp_LIBRARIES ${Mosquittopp_LIBRARY} ${Mosquitto_LIBRARY})
else(Mosquittopp_FOUND)
    set(Mosquittopp_LIBRARIES)
endif(Mosquittopp_FOUND)

mark_as_advanced(Mosquittopp_INCLUDE_DIR Mosquittopp_LIBRARY)
