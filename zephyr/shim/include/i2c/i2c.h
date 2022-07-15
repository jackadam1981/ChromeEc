/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_I2C_I2C_H
#define ZEPHYR_CHROME_I2C_I2C_H

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#ifdef CONFIG_PLATFORM_EC_I2C
#if DT_NODE_EXISTS(DT_PATH(named_i2c_ports))

#define NPCX_PORT_COMPAT nuvoton_npcx_i2c_port
#define ITE_IT8XXX2_PORT_COMPAT ite_it8xxx2_i2c
#define ITE_ENHANCE_PORT_COMPAT ite_enhance_i2c
#define MICROCHIP_XEC_COMPAT microchip_xec_i2c_v2
#define I2C_EMUL_COMPAT zephyr_i2c_emul_controller
#define I2C_FOREACH_PORT(fn)                                \
	DT_FOREACH_STATUS_OKAY(NPCX_PORT_COMPAT, fn)        \
	DT_FOREACH_STATUS_OKAY(ITE_IT8XXX2_PORT_COMPAT, fn) \
	DT_FOREACH_STATUS_OKAY(ITE_ENHANCE_PORT_COMPAT, fn) \
	DT_FOREACH_STATUS_OKAY(MICROCHIP_XEC_COMPAT, fn)    \
	DT_FOREACH_STATUS_OKAY(I2C_EMUL_COMPAT, fn)

#define I2C_PORT_BUS(i2c_port_id) DT_CAT(I2C_BUS_, i2c_port_id)
#define I2C_PORT_BUS_WITH_COMMA(i2c_port_id) I2C_PORT_BUS(i2c_port_id),

enum i2c_ports_chip {
	I2C_FOREACH_PORT(I2C_PORT_BUS_WITH_COMMA)
	I2C_PORT_COUNT
};

BUILD_ASSERT(I2C_PORT_COUNT != 0, "No I2C devices defined");

#define I2C_PORT(i2c_named_id) DT_STRING_UPPER_TOKEN(i2c_named_id, enum_name)
#define I2C_PORT_ENUM(i2c_named_id) \
	I2C_PORT(i2c_named_id) =    \
		I2C_PORT_BUS(DT_PHANDLE(i2c_named_id, i2c_port))
#define I2C_PORT_WITH_COMMA(i2c_named_id) I2C_PORT_ENUM(i2c_named_id),

enum i2c_ports {
	DT_FOREACH_CHILD(DT_PATH(named_i2c_ports), I2C_PORT_WITH_COMMA)
};
#define NAMED_I2C(name) I2C_PORT(DT_PATH(named_i2c_ports, name))
#endif /* named_i2c_ports */
#endif /* CONFIG_PLATFORM_EC_I2C */

#define I2C_DEVICE_COUNT I2C_PORT_COUNT

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

/**
 * @brief Get a port number for a received remote port number.
 *
 * This function translate a received port number via the I2C_PASSTHRU host
 * command to a port number used in ZephyrEC based on remote_port property in
 * dts. The first port which matches the remote port number is returned.
 *
 * @param port The received remote port.
 * @return Port number used in EC. -1 if the remote port is not defined
 */
int i2c_get_port_from_remote_port(int remote_port);

#endif /* ZEPHYR_CHROME_I2C_I2C_H */
