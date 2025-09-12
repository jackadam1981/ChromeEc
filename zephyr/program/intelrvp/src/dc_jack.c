/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DC Jack configuration */

#include "charge_manager.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "intelrvp.h"
#include "tcpm/tcpci.h"

#include <zephyr/init.h>

static struct k_work dc_jack_handle;
static bool dc_jack_is_disabled = false;

bool is_typec_port(int port)
{
	return !(port == DEDICATED_CHARGE_PORT || port == CHARGE_PORT_NONE);
}

int board_is_dc_jack_present(void)
{
	return gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(std_adp_prsnt));
}

static void board_dc_jack_handler(struct k_work *dc_jack_work)
{
	struct charge_port_info charge_dc_jack;

	/* System is booted from DC Jack */
	if (!dc_jack_is_disabled && board_is_dc_jack_present()) {
		charge_dc_jack.current =
			MIN((CONFIG_PLATFORM_EC_USB_PD_MAX_POWER_MW * 1000) /
				    DC_JACK_MAX_VOLTAGE_MV,
			    CONFIG_PLATFORM_EC_USB_PD_MAX_CURRENT_MA);
		charge_dc_jack.voltage = DC_JACK_MAX_VOLTAGE_MV;
	} else {
		/* DC jack is disabled or not present */
		charge_dc_jack.current = 0;
		charge_dc_jack.voltage = 0;
	}

	charge_manager_update_charge(CHARGE_SUPPLIER_DEDICATED,
				     DEDICATED_CHARGE_PORT, &charge_dc_jack);
}

void board_dc_jack_interrupt(enum gpio_signal signal)
{
	k_work_submit(&dc_jack_handle);
}

test_export_static void board_charge_init(void)
{
	k_work_init(&dc_jack_handle, board_dc_jack_handler);

	/* Handler not deferred during Board charge initialization */
	board_dc_jack_handler(NULL);

	if (!dc_jack_is_disabled) {
		gpio_enable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_dc_jack_present));
	}
}

#ifdef CONFIG_USB_PDC_POWER_MGMT
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rvp_dc_jack, LOG_LEVEL_INF);

static int board_charge_sys_init(void)
{
	board_charge_init();
	return 0;
}
SYS_INIT(board_charge_sys_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#define DC_JACK_STARTUP_CHECK_INIT_PRIORITY 88

/* Must come after GPIO drivers are ready, and before charge manager */
BUILD_ASSERT(CONFIG_PLATFORM_EC_GPIO_INIT_PRIORITY <
	     DC_JACK_STARTUP_CHECK_INIT_PRIORITY);
BUILD_ASSERT(DC_JACK_STARTUP_CHECK_INIT_PRIORITY <
	     CONFIG_CHARGE_MANAGER_SYS_INIT_PRIORITY);
/**
 * @brief If the DC barrel jack is absent at startup, disable its load switch.
 *        We cannot currently handle power transitions between PDC and barrel
 *        jack on RVP boards, so don't allow a barrel jack adapter to be
 *        connected later.
 */
static int run_dc_jack_startup_check(void)
{
	if (board_is_dc_jack_present() == false) {
		dc_jack_is_disabled = true;
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(std_adp_cntrl), 1);
		LOG_INF("RVP ext power input: PDCs only (no barrel jack)");
	} else {
		LOG_INF("RVP ext power input: DC barrel jack only");
	}

	return 0;
}
SYS_INIT(run_dc_jack_startup_check, POST_KERNEL,
	 DC_JACK_STARTUP_CHECK_INIT_PRIORITY);

#else
DECLARE_HOOK(HOOK_INIT, board_charge_init, HOOK_PRIO_POST_CHARGE_MANAGER);
#endif
