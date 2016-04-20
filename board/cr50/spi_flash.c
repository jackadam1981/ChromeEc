/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio.h"
#include "spi_flash_reg.h"
#include "util.h"

static int command_spi_flash_select(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcasecmp("ec", argv[1])) {
			spi_flash_select(SPI_FLASH_GD25Q41B);
			gpio_set_level(GPIO_AP_FLASH_SELECT, 0);
			gpio_set_level(GPIO_EC_FLASH_SELECT, 1);
		} else if (!strcasecmp("ap", argv[1])) {
			spi_flash_select(SPI_FLASH_GD25Q64C);
			gpio_set_level(GPIO_EC_FLASH_SELECT, 0);
			gpio_set_level(GPIO_AP_FLASH_SELECT, 1);
		} else if (!strcasecmp("disable", argv[1])) {
			gpio_set_level(GPIO_AP_FLASH_SELECT, 0);
			gpio_set_level(GPIO_EC_FLASH_SELECT, 0);
		}
	}

	ccprintf("EC: %s\nAP: %s\n", gpio_get_level(GPIO_EC_FLASH_SELECT) ?
		 "enabled" : "disabled", gpio_get_level(GPIO_AP_FLASH_SELECT) ?
		 "enabled" : "disabled");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spi_flash_select, command_spi_flash_select,
	"[ap|ec|disable]",
	"Select spi",
	NULL);
