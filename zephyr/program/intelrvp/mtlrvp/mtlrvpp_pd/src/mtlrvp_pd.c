/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charger.h"
#include "common.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "i2c.h"
#include "intel_rvp_board_id.h"
#include "intelrvp.h"
#include "ioexpander.h"
#include "isl9241.h"
#include "keyboard_raw.h"
#include "power/meteorlake.h"
#include "sn5s330.h"
#include "system.h"
#include "task.h"
#include "tusb1064.h"
#include "usb_mux.h"
#include "usbc/usb_muxes.h"
#include "util.h"
#include "usb_config.h"
#include <zephyr/drivers/espi.h>

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ##args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ##args)

static void board_int_init(void)
{
	/* Enable CCD Mode interrupt */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_ccd_mode));

	/* Enable DC jack interrupt */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_dc_jack_present));
}
static int board_pre_task_peripheral_init(void)
{
	/* Initialize all interrupts */
	board_int_init();
	return 0;
}
SYS_INIT(board_pre_task_peripheral_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
