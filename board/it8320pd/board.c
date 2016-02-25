/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* IT8320 development board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/* Send host event up to AP */
void pd_send_host_event(int mask)
{

}

/* Reset PD MCU */
void board_reset_pd_mcu(void)
{

}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	/*
	 * Default no low power idle for EVB,
	 * use console command "sleepmask" to enable it if necessary.
	 */
	disable_sleep(SLEEP_MASK_FORCE_NO_DSLEEP);
	/*
	 * The GPIOH.5/6 may be used for flashing purposes if WP pin
	 * is deasserted. The clock of this module needs to be enabled.
	 * So we disable the clock when WP pin is asserted,
	 * this can help to reduce power consumption.
	 */
#ifdef CONFIG_WP_ACTIVE_HIGH
	if (gpio_get_level(GPIO_WP))
		clock_disable_peripheral(CGC_OFFSET_USB, 0, 0);
	else
		clock_enable_peripheral(CGC_OFFSET_USB, 0, 0);
#else
	if (!gpio_get_level(GPIO_WP_L))
		clock_disable_peripheral(CGC_OFFSET_USB, 0, 0);
	else
		clock_enable_peripheral(CGC_OFFSET_USB, 0, 0);
#endif
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Convert to mV (3000mV/1024). */
		{"ADC_VBUSSA",	3000, 1024, 0, 0}, /*GPI0*/
		{"ADC_VBUSSB", 3000, 1024, 0, 1}, /*GPI1*/
		{"ADC_VBUS",  3000, 1024, 0, 2},
		{"AMON_BMON", 3000, 1024, 0, 3},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
