/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charger.h"
#include "common.h"
#include "console.h"
#include "driver/tcpm/ccgxxf.h"
#include "driver/tcpm/tcpci.h"
#include "extpower.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "i2c.h"
#include "keyboard_raw.h"
#include "power/meteorlake.h"
#include "system.h"
#include "task.h"
#include "usb_mux.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ##args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ##args)

/*******************************************************************/
/* USB-C Configuration Start */

/* USB-C ports */
enum usbc_port {
	USBC_PORT_C0 = 0,
	USBC_PORT_C1,
	USBC_PORT_COUNT
};
BUILD_ASSERT(USBC_PORT_COUNT == CONFIG_USB_PD_PORT_MAX_COUNT);

void board_overcurrent_event(int port, int is_overcurrented)
{
	/*
	 * TODO: Meteorlake PCH does not use Physical GPIO for over current
	 * error, hence Send 'Over Current Virtual Wire' eSPI signal.
	 */
}

void board_reset_pd_mcu(void)
{
	/* Reset the ccgxxf ports only resetting 1 is required */
	ccgxxf_reset(USBC_PORT_C0);
}

/* PWROK signal configuration */
/*
 * On MTLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
 * as input.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_PCH_SYS_PWROK,
		.delay_ms = 3,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	{
		.gpio = GPIO_PCH_SYS_PWROK,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

static void board_int_init(void)
{
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c2_tcpc));
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c3_tcpc));

}

__override bool pd_check_vbus_level(int port, enum vbus_level level)
{
	if (level == VBUS_PRESENT) {
		return pd_snk_is_vbus_provided(port);
	} else {
		return !pd_snk_is_vbus_provided(port);
	}
}

static int board_pre_task_peripheral_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	/* Only reset tcpc/pd if not sysjump */
	if (!system_jumped_late()) {
		board_reset_pd_mcu();
	}

	/* Initialize all interrupts */
	board_int_init();

	return 0;
}
SYS_INIT(board_pre_task_peripheral_init, APPLICATION,
	 CONFIG_APPLICATION_INIT_PRIORITY);
