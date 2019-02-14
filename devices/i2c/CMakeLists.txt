cmake_minimum_required (VERSION 3.0)

file (GLOB_RECURSE DEVICE_SOURCE_FILES *.cpp)

set (DEVICE_EXTRA_LIBRARIES
     agoclient ${I2C_LIBRARIES}
)

set (DEVICE "agoi2c")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})

AgoService(i2c DESCRIPTION "ago control i2c device")
