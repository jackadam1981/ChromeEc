/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "demo.h"
#include "gpio.h"
#include "hooks.h"
#include "lcd.h"
#include "registers.h"
#include "spi.h"
#include "i2c.h"

/* Must come after other header files and GPIO interrupts*/
#include "gpio_list.h"

/******************************************************************************/

/* SPI devices */
const struct spi_device_t spi_devices[] = {
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void tcpc_alert_clear(int port)
{
	/* Do nothing */
}
