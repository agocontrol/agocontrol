cmake_minimum_required (VERSION 2.6)

set(LIBAGOHTTP_SOURCE_FILES
	agohttp.cpp
	mongoose.c
)

add_library (agohttp SHARED ${LIBAGOHTTP_SOURCE_FILES})
set_target_properties (agohttp PROPERTIES VERSION 1 SOVERSION 1)

set (AGOHTTP_LIBRARIES
	agoclient # Mainly for logging
	${Boost_SYSTEM_LIBRARY}
	${Boost_FILESYSTEM_LIBRARY}
	${Boost_THREAD_LIBRARY}
	qpidtypes
	ssl crypto
	${JSONCPP_LIBRARIES}
)

#IF (Boost_LOG_FOUND)
#	LIST(APPEND AGOHTTP_LIBRARIES ${Boost_LOG_LIBRARY})
#ENDIF()

target_link_libraries (agohttp ${AGOHTTP_LIBRARIES})

set_property(
	TARGET agohttp
	PROPERTY COMPILE_DEFINITIONS
	MG_ENABLE_SSL=1
)

set_property(
	TARGET agohttp
	APPEND PROPERTY
	COMPILE_OPTIONS
	-Werror)

install (TARGETS agohttp LIBRARY DESTINATION ${LIBDIR})