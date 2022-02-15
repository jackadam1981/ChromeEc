/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "pca9555.h"

/* Board ID */
#define I2C_PORT_PCA9555_BOARD_ID_GPIO  I2C_PORT_BATTERY
#define I2C_ADDR_PCA9555_BOARD_ID_GPIO  0x22

static void board_init(void)
{
	/* Enable SOC SPI */
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_spi_oe_mecc), 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_LAST);

int ioexpander_read_intelrvp_version(int *port0, int *port1)
{
	if (pca9555_read(I2C_PORT_PCA9555_BOARD_ID_GPIO,
				I2C_ADDR_PCA9555_BOARD_ID_GPIO,
				PCA9555_CMD_INPUT_PORT_0, port0))
		return -1;

	return pca9555_read(I2C_PORT_PCA9555_BOARD_ID_GPIO,
			I2C_ADDR_PCA9555_BOARD_ID_GPIO,
			PCA9555_CMD_INPUT_PORT_1, port1);
}
