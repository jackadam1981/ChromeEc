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
	/* Enable TCPC interrupts. */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0_c1_tcpc));
#if defined(HAS_TASK_PD_C2)
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c2_tcpc));
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c3_tcpc));
#endif

	/* Enable CCD Mode interrupt */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_ccd_mode));

	/* Enable DC jack interrupt */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_dc_jack_present));
}
static int board_pre_task_peripheral_init(void)
{
	/* Only reset tcpc/pd if not sysjump */
	if (!system_jumped_late()) {
		/* Initialize tcpc and all ioex */
		board_reset_pd_mcu();
	}

	/* Initialize all interrupts */
	board_int_init();

	/* Make sure SBU are routed to CCD or AUX based on CCD status at init */
	board_connect_c0_sbu_deferred();

	/* Configure board specific retimer & mux */
	configure_retimer_usbmux();

	return 0;
}
SYS_INIT(board_pre_task_peripheral_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
