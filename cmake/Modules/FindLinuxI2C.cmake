# Different linux distributions places i2c-dev.h at different locations, with different contents.
# For example on debian i2c_smbus_write_i2c_block_data is provided by linux/i2c-dev.h, but Fedora 27 uses i2c-tools/i2c-dev.h
# even if it has linux/i2c-dev.h.
# This tries to figure out which file to include.
include (CheckSymbolExists)

check_symbol_exists(i2c_smbus_write_i2c_block_data i2c-tools/i2c-dev.h HAVE_I2C_SMBUS_VIA_I2C_TOOLS)
check_symbol_exists(i2c_smbus_write_i2c_block_data linux/i2c-dev.h HAVE_I2C_SMBUS_VIA_LINUX_I2C_DEV)

if(HAVE_I2C_SMBUS_VIA_I2C_TOOLS)
    message(STATUS "i2c_smbus_write_i2c_block_data available through i2c-tools/i2c-dev.h")
    set(HAVE_I2C_SMBUS 1)
elseif(HAVE_I2C_SMBUS_VIA_LINUX_I2C_DEV)
    message(STATUS "i2c_smbus_write_i2c_block_data available through linux/i2c-dev.h")
    set(HAVE_I2C_SMBUS 1)
else()
    message(STATUS "i2c_smbus_write_i2c_block_data not available, consider installing package providing i2c-tools")
    set(HAVE_I2C_SMBUS 0)
endif()
