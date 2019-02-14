# Different linux distributions places i2c-dev.h at different locations, with different contents.
# For example on debian i2c_smbus_write_i2c_block_data is provided by linux/i2c-dev.h, but Fedora 27 uses i2c-tools/i2c-dev.h
# even if it has linux/i2c-dev.h.
# This tries to figure out which file to include.
include (CheckSymbolExists)

check_symbol_exists(i2c_smbus_write_i2c_block_data i2c-tools/i2c-dev.h HAVE_I2C_SMBUS_VIA_I2C_TOOLS)
check_symbol_exists(i2c_smbus_write_i2c_block_data linux/i2c-dev.h HAVE_I2C_SMBUS_VIA_LINUX_I2C_DEV)

# Fedora 28 uses libi2c
# https://lists.fedoraproject.org/archives/list/devel@lists.fedoraproject.org/thread/BRUCA5P4WXL4MZLQ4Q6TP7IBDFR7WFWA/?sort=date
set(CMAKE_REQUIRED_LIBRARIES i2c)
check_symbol_exists(i2c_smbus_write_i2c_block_data i2c/smbus.h HAVE_I2C_SMBUS_VIA_LIBI2C)
set(CMAKE_REQUIRED_LIBRARIES)

if(HAVE_I2C_SMBUS_VIA_I2C_TOOLS)
    message(STATUS "i2c_smbus_write_i2c_block_data available through i2c-tools/i2c-dev.h")
    set(HAVE_I2C_SMBUS 1)
elseif(HAVE_I2C_SMBUS_VIA_LINUX_I2C_DEV)
    message(STATUS "i2c_smbus_write_i2c_block_data available through linux/i2c-dev.h")
    set(HAVE_I2C_SMBUS 1)
elseif(HAVE_I2C_SMBUS_VIA_LIBI2C)
    message(STATUS "i2c_smbus_write_i2c_block_data available through i2c/smbus.h")
    set(HAVE_I2C_SMBUS 1)
    set(I2C_LIBRARIES i2c)
else()
    message(STATUS "i2c_smbus_write_i2c_block_data not available, consider installing package providing i2c-tools/libi2c-devel")
    set(HAVE_I2C_SMBUS 0)
endif()
