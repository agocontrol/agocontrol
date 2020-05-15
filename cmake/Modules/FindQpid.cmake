# Find qpid libraries and headers
#
#  Qpid_INCLUDE_DIR - where to find qpid/messaging/*.h, etc.
#  Qpid_LIBRARIES   - List of libraries when using qpid.
#  Qpid_FOUND       - True if qpid found.

if(Qpid_INCLUDE_DIR)
    # Already in cache, be silent
    set(Qpid_FIND_QUIETLY TRUE)
endif(Qpid_INCLUDE_DIR)

find_path(Qpid_INCLUDE_DIR qpid/messaging/Connection.h)

find_library(Qpid_MESSAGING_LIBRARY NAMES qpidmessaging)
find_library(Qpid_TYPES_LIBRARY NAMES qpidtypes)

# Handle the QUIETLY and REQUIRED arguments and set Qpid_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Qpid DEFAULT_MSG Qpid_MESSAGING_LIBRARY Qpid_INCLUDE_DIR)

# Modern CMake suggests that variable name is named after FindXxx i.e. same case.
# Older cmake (3.0) ignores case and always registers UPPERCASE_FOUND
if(NOT DEFINED Qpid_FOUND)
    set(Qpid_FOUND ${QPID_FOUND})
endif()

if(Qpid_FOUND)
    set(Qpid_LIBRARIES ${Qpid_MESSAGING_LIBRARY} ${Qpid_TYPES_LIBRARY})
else(Qpid_FOUND)
    set(Qpid_LIBRARIES)
endif(Qpid_FOUND)

mark_as_advanced(Qpid_INCLUDE_DIR Qpid_LIBRARY)
