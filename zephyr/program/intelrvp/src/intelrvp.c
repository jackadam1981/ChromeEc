/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TODO: b/218904113: Convert to using Zephyr GPIOs */
#include "gpio.h"
#include "hooks.h"

#define DT_DRV_COMPAT cros_ec_shared_spi_flash

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1,
	     "Unsupported External SPI GPIO");

#define SHARED_SPI_NODE DT_PATH(shared_spi)

const struct gpio_dt_spec ec_spi_oe_mecc =
			GPIO_DT_SPEC_GET(SHARED_SPI_NODE, spi_oe_gpios);

#ifdef CONFIG_PLATFORM_EC_SHARED_SPI_FLASH
static void ec_get_external_spi_access(void)
{
	/* EC to get access to SPI flash  */
	gpio_pin_set_dt(&ec_spi_oe_mecc, 0);
	/* delay before EC access the external SPI flash */
	k_msleep(10);
}
DECLARE_HOOK(HOOK_SYSJUMP, ec_get_external_spi_access, HOOK_PRIO_FIRST);
#endif

static void board_init(void)
{
	/* Enable SOC SPI */
	gpio_pin_set_dt(&ec_spi_oe_mecc, 1);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_FIRST);

__override void intel_x86_sys_reset_delay(void)
{
	/*
	 * From MAX6818 Data sheet, Range of 'Debounce Duaration' is
	 * Minimum - 20 ms, Typical - 40 ms, Maximum - 80 ms.
	 * See b/153128296.
	 */
	udelay(60 * MSEC);
}
