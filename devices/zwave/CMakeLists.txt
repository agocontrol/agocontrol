cmake_minimum_required (VERSION 2.6)

file (GLOB DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     ${OpenZWave_LIBRARIES}
     agoclient
     pthread
)

IF (FREEBSD)
	list(APPEND DEVICE_EXTRA_LIBRARIES usb)
	IF(NOT EXISTS "/usr/include/iconv.h")
		# Pre 10.0 FreeBSD without globla iconv
		list(APPEND DEVICE_EXTRA_LIBRARIES iconv)
	ENDIF ()
ENDIF (FREEBSD)

include_directories(${OpenZWave_INCLUDE_DIRS})

set (DEVICE "agozwave")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

set_property(
		TARGET ${DEVICE}
		PROPERTY COMPILE_DEFINITIONS
		OPENZWAVE_VERSION_MAJOR=${OpenZWave_VERSION_MAJOR}
		OPENZWAVE_VERSION_MINOR=${OpenZWave_VERSION_MINOR}
		OPENZWAVE_VERSION_REVISION=${OpenZWave_VERSION_REVISION}
)

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})
install (DIRECTORY DESTINATION ${CONFDIR}/ozw)
