/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/i2c.h>

#include "console.h"
#include "i2c.h"
#include "i2c/i2c.h"
#include "i2c_map.h"

/*
 * Long term we will not need these, for now they're needed to get things to
 * build since these extern symbols are usually defined in
 * board/${BOARD}/board.c.
 *
 * Since all the ports will eventually be handled by device tree. This will
 * be removed at that point.
 */
const struct i2c_port_t i2c_ports[] = {};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

int i2c_get_line_levels(int port)
{
	return I2C_LINE_IDLE;
}

/*
 * TODO(b/174951223): Remove once NPCX I2C supports clock-frequency device
 * tree property.
 */
int zephyr_shim_setup_i2c(void)
{
	uint32_t dev_config;
	const struct device *device;
	int i;
	int rv = 0;

	cprints(CC_I2C, "Configure I2C bus speeds\n");

	for (i = 0; i < zephyr_i2c_port_assert_count; i++) {
		dev_config = I2C_MODE_MASTER
			| I2C_SPEED_SET(zephyr_i2c_ports[i].i2c_speed);
		device = i2c_get_device_for_port(zephyr_i2c_ports[i].port);

		rv = i2c_configure(device, dev_config);
		if (rv) {
			cprints(CC_I2C, "I2C configure error on bus %d\n",
				zephyr_i2c_ports[i].port);
			break;
		}
	}

	return rv;
}
