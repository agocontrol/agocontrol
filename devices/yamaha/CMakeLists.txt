cmake_minimum_required (VERSION 3.0)

# select your source files
file (GLOB_RECURSE DEVICE_SOURCE_FILES
    agoyamaha.cpp
    yamaha_device_base.cpp
    yamaha_device.cpp
    ynca_message.cpp
    )

# set extra libraries you need to link to
# your binary
set (DEVICE_EXTRA_LIBRARIES
     agoclient
)

#set a device name. This will be the 'target'
set (DEVICE "agoyamaha")

# add the executable
add_executable (${DEVICE} ${DEVICE_SOURCE_FILES})

# link all needed libraries
target_link_libraries (${DEVICE} ${DEVICE_EXTRA_LIBRARIES})

# create an install target for the device
install (TARGETS ${DEVICE} RUNTIME DESTINATION ${BINDIR})

