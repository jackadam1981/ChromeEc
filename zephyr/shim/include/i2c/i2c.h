/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_I2C_I2C_H
#define ZEPHYR_CHROME_I2C_I2C_H

#include <device.h>
#include <devicetree.h>

#ifdef CONFIG_PLATFORM_EC_I2C
#if DT_NODE_EXISTS(DT_PATH(named_i2c_ports))
#define I2C_PORT(id) DT_CAT(I2C_, id)
#define I2C_PORT_WITH_COMMA(id) I2C_PORT(id),
/**
 * Add an enum value using `name` if there exists a node with the given `compat`
 * string along with a status 'okay'.
 *
 * Usage:
 *   I2C_PORT_FROM_COMPAT(test_compat_name, ENUM_NAME)
 *
 * This will first check to see if any nodes contain `test_compat_name` as a
 * compatible string AND that the node has the status set to 'okay'. If that
 * condition is met, it will resolve to `ENUM_NAME,` otherwise it will be a
 * no-op.
 */
#define I2C_PORT_FROM_COMPAT(compat, name) \
	COND_CODE_1(DT_HAS_COMPAT_STATUS_OKAY(compat), (name,), ())

/**
 * Conditionally initialize `dev_ptr` to the first device having the provided
 * `compat` string and having the status set to 'okay'. If the condition is met,
 * this statement will resolve to:
 *   dev_ptr = device_get_binding(DT_LABEL(DT_INST(0, compat)))
 *
 * Note that the call site still need to follow with `;` to make this a valid
 * statement.
 */
#define I2C_DEV_INIT_FROM_COMPAT(dev_ptr, compat)                             \
	COND_CODE_1(                                                          \
		DT_HAS_COMPAT_STATUS_OKAY(compat),                            \
		(dev_ptr = device_get_binding(DT_LABEL(DT_INST(0, compat)))), \
		())
enum i2c_ports {
DT_FOREACH_CHILD(DT_PATH(named_i2c_ports), I2C_PORT_WITH_COMMA)
I2C_PORT_FROM_COMPAT(cros_ec_i2c_port_power, I2C_PORT_POWER)
I2C_PORT_COUNT
};
#define NAMED_I2C(name) I2C_PORT(DT_PATH(named_i2c_ports, name))
#endif /* named_i2c_ports */
#endif /* CONFIG_PLATFORM_EC_I2C */

/**
 * @brief Adaptation of platform/ec's port IDs which map a port/bus to a device.
 *
 * This function should be implemented per chip and should map the enum value
 * defined for the chip for encoding each valid port/bus combination. For
 * example, the npcx chip defines the port/bus combinations NPCX_I2C_PORT* under
 * chip/npcx/registers-npcx7.h.
 *
 * Thus, the npcx shim should implement this function to map the enum values
 * to the correct devicetree device.
 *
 * @param port The port to get the device for.
 * @return Pointer to the device struct or {@code NULL} if none are available.
 */
const struct device *i2c_get_device_for_port(const int port);

#endif /* ZEPHYR_CHROME_I2C_I2C_H */
