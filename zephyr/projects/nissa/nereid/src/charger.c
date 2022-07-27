/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>

#include "battery.h"
#include "charger.h"
#include "console.h"
#include "driver/charger/sm5803.h"
#include "extpower.h"
#include "usb_pd.h"
#include "nissa_common.h"

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

int extpower_is_present(void)
{
	int port;
	int rv;
	bool acok;

	for (port = 0; port < board_get_usb_pd_port_count(); port++) {
		rv = sm5803_is_acok(port, &acok);
		if ((rv == EC_SUCCESS) && acok)
			return 1;
	}

	return 0;
}

/*
 * Nereid does not have a GPIO indicating whether extpower is present,
 * so detect using the charger(s).
 */
__override void board_check_extpower(void)
{
	static int last_extpower_present;
	int extpower_present = extpower_is_present();

	if (last_extpower_present ^ extpower_present)
		extpower_handle_update(extpower_present);

	last_extpower_present = extpower_present;
}

__override void board_hibernate(void)
{
	/* Shut down the chargers */
	if (board_get_usb_pd_port_count() == 2)
		sm5803_hibernate(CHARGER_SECONDARY);
	sm5803_hibernate(CHARGER_PRIMARY);
	LOG_INF("Charger(s) hibernated");
	cflush();
}


#define CC_OVP_GPIO GPIO_DT_FROM_NODELABEL(gpio_usb_c0_prot_fault_odl)

static void cc_ovp_interrupt(const struct device *device, struct gpio_callback *callback, gpio_port_pins_t pins)
{
	int state = gpio_pin_get_dt(CC_OVP_GPIO);
	LOG_ERR("CC OVP fired!!!1!1! (= %d)", state);
}

static int init_cc_ovp_interrupt(const struct device *unused)
{
	static struct gpio_callback cc_ovp_cb;
	int rv;

	gpio_pin_configure_dt(CC_OVP_GPIO, GPIO_INPUT | GPIO_PULL_UP);
	gpio_init_callback(&cc_ovp_cb, cc_ovp_interrupt, BIT(CC_OVP_GPIO->pin));
	rv = gpio_pin_interrupt_configure_dt(CC_OVP_GPIO, GPIO_INT_EDGE_BOTH);

	if (rv != 0) {
		LOG_ERR("failed to configure CC OVP interrupt");
		k_oops();
	}
	return 0;
}
SYS_INIT(init_cc_ovp_interrupt, APPLICATION, 0);
