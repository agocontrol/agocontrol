cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     agoclient ${I2C_LIBRARIES}
)

set (DEVICE "agoblinkm")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})

AgoService(blinkm DESCRIPTION "ago control blinkm i2c device")
