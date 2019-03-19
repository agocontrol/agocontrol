# Find qpid libraries and headers
#
#  QPID_INCLUDE_DIR - where to find qpid/messaging/*.h, etc.
#  QPID_LIBRARIES   - List of libraries when using qpid.
#  QPID_FOUND       - True if qpid found.

if(QPID_INCLUDE_DIR)
    # Already in cache, be silent
    set(QPID_FIND_QUIETLY TRUE)
endif(QPID_INCLUDE_DIR)

find_path(QPID_INCLUDE_DIR qpid/messaging/Connection.h)

find_library(QPID_MESSAGING_LIBRARY NAMES qpidmessaging)
find_library(QPID_TYPES_LIBRARY NAMES qpidtypes)

# Handle the QUIETLY and REQUIRED arguments and set QPID_FOUND to TRUE if
# all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QPID DEFAULT_MSG QPID_MESSAGING_LIBRARY QPID_INCLUDE_DIR)

if(QPID_FOUND)
    set(QPID_LIBRARIES ${QPID_MESSAGING_LIBRARY} ${QPID_TYPES_LIBRARY})
else(QPID_FOUND)
    set(QPID_LIBRARIES)
endif(QPID_FOUND)

mark_as_advanced(QPID_INCLUDE_DIR QPID_LIBRARY)
