/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_I2C_I2C_H
#define ZEPHYR_CHROME_I2C_I2C_H

#include <device.h>
#include <devicetree.h>

/**
 * @brief Adaptation of platform/ec's port IDs which map a port/bus to a device.
 *
 * This function should be implemented per chip and should map the enum value
 * defined for the chip for encoding each valid port/bus combination. For
 * example, the npcx chip defines the following port/bus combinations:
 * https://source.chromium.org/chromiumos/chromiumos/codesearch/+/master:src/platform/ec/chip/npcx/registers-npcx7.h;l=260;drc=1ba3b3d64b0cc58c107a5d24079999deea0bb03b
 *
 * Thus, the npcx shim should implement this function to map the enum values
 * to the correct devicetree device.
 *
 * @param port The port to get the device for.
 * @return Pointer to the device struct or {@code null} if none are available.
 */
const struct device *i2c_get_device_for_port(const int port);

#endif /* ZEPHYR_CHROME_I2C_I2C_H */
