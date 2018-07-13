#ifndef AGOCONTROL_I2C_INCLUDE_H
#define AGOCONTROL_I2C_INCLUDE_H

/* This file interacts with cmake/Modules/FindLinuxI2C.cmake
 * and can be included to pick the proper header to use for accessing i2c_smbus_*
 */

#include "build_config.h"

#if HAVE_I2C_SMBUS_VIA_I2C_TOOLS
#include <i2c-tools/i2c-dev.h>
#elif HAVE_I2C_SMBUS_VIA_LINUX_I2C_DEV
// Fedora 27 etc
#include <linux/i2c-dev.h>
#elif HAVE_I2C_SMBUS_VIA_LIBI2C
extern "C" {
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
};
#else
#error "No i2c smbus support"
#endif

#endif //AGOCONTROL_I2C_INCLUDE_H
