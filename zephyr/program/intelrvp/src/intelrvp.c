/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TODO: b/218904113: Convert to using Zephyr GPIOs */
#include "gpio.h"
#include "hooks.h"

#ifdef CONFIG_AP_PWRSEQ
#include "x86_non_dsx_common_pwrseq_sm_handler.h"
#else
#include "power.h"
#endif

static void ec_release_external_spi_to_ap(void)
{
	/* covers the usecase of shutdown and followed by powerbtn */
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_spi_oe_mecc), 1);
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, ec_release_external_spi_to_ap, HOOK_PRIO_LAST);

static void ec_get_external_spi_access(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(ec_spi_oe_mecc), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_HARD_OFF, ec_get_external_spi_access, HOOK_PRIO_LAST);

static void board_init(void)
{
#ifdef CONFIG_AP_PWRSEQ
	if (chipset_pwr_seq_get_state() == SYS_POWER_STATE_G3)
#else
	if (power_get_state() == POWER_G3)
#endif
		ec_get_external_spi_access();
	else  /* after sysjump to RO/RW set the SPI access  */
		ec_release_external_spi_to_ap();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_PRE_I2C);

__override void intel_x86_sys_reset_delay(void)
{
	/*
	 * From MAX6818 Data sheet, Range of 'Debounce Duaration' is
	 * Minimum - 20 ms, Typical - 40 ms, Maximum - 80 ms.
	 * See b/153128296.
	 */
	udelay(60 * MSEC);
}
