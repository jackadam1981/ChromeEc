/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nereid sub-board hardware configuration */

#include <ap_power/ap_power.h>
#include <drivers/gpio.h>
#include <init.h>
#include <kernel.h>
#include <sys/printk.h>

#include "driver/charger/sm5803.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "usb_pd.h"
#include "task.h"

#include "nissa_common.h"

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

static void hdmi_power_handler(struct ap_power_ev_callback *cb,
			       struct ap_power_ev_data data)
{
	const struct gpio_dt_spec *s3_rail = GPIO_DT_FROM_ALIAS(gpio_hdmi_en_odl);
	const struct gpio_dt_spec *s5_rail = GPIO_DT_FROM_ALIAS(gpio_en_rails_odl);

	switch (data.event) {
	case AP_POWER_PRE_INIT:
		LOG_DBG("Enabling HDMI+USB-A PP5000");
		gpio_pin_set_dt(s5_rail, 1);
		break;
	case AP_POWER_STARTUP:
		LOG_DBG("Enabling HDMI VCC");
		gpio_pin_set_dt(s3_rail, 1);
		break;
	case AP_POWER_SHUTDOWN:
		LOG_DBG("Disabling HDMI VCC");
		gpio_pin_set_dt(s3_rail, 0);
		break;
	case AP_POWER_HARD_OFF:
		LOG_DBG("Disabling HDMI+USB-A PP5000");
		gpio_pin_set_dt(s5_rail, 0);
		break;
	default:
		LOG_ERR("Unhandled HDMI power event %d", data.event);
		break;
	}
}

static void nereid_subboard_init(void)
{
	enum nissa_sub_board_type sb = nissa_get_sb_type();

	/*
	 * Need to initialise board specific GPIOs since the
	 * common init code does not know about them.
	 * Remove once common code initialises all GPIOs, not just
	 * the ones with enum-names.
	 *
	 * TODO(b/214858346): Enable power after AP startup.
	 */
	if (sb != NISSA_SB_C_A && sb != NISSA_SB_HDMI_A) {
		/* Turn off unused USB A1 GPIOs */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(gpio_sub_usb_a1_ilimit_sdp),
			GPIO_DISCONNECTED);
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_en_usb_a1_vbus),
			GPIO_DISCONNECTED);
	}
	if (sb == NISSA_SB_C_A || sb == NISSA_SB_C_LTE) {
		/* Enable type-C port 1 */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_usb_c1_int_odl),
			GPIO_INPUT | GPIO_PULL_UP);
		/* Configure type-A port 1 VBUS, initialise it as low */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_en_usb_a1_vbus),
			GPIO_OUTPUT_LOW);
	}
	if (sb == NISSA_SB_HDMI_A) {
		static struct ap_power_ev_callback hdmi_power_cb;

		/* HDMI power enable outputs */
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_en_rails_odl),
				      GPIO_OUTPUT_INACTIVE | GPIO_OPEN_DRAIN |
					      GPIO_PULL_UP | GPIO_ACTIVE_LOW);
		gpio_pin_configure_dt(GPIO_DT_FROM_ALIAS(gpio_hdmi_en_odl),
				      GPIO_OUTPUT_INACTIVE | GPIO_OPEN_DRAIN |
					      GPIO_ACTIVE_LOW);
		/* Control HDMI power in concert with AP */
		ap_power_ev_init_callback(
			&hdmi_power_cb, hdmi_power_handler,
			AP_POWER_PRE_INIT | AP_POWER_HARD_OFF |
				AP_POWER_STARTUP | AP_POWER_SHUTDOWN);
		ap_power_ev_add_callback(&hdmi_power_cb);

		/* Configure the interrupt separately */
		// TODO: implement interrupt handler
		gpio_pin_configure_dt(
			GPIO_DT_FROM_ALIAS(gpio_hpd_odl),
			0);
	}
}
DECLARE_HOOK(HOOK_INIT, nereid_subboard_init, HOOK_PRIO_FIRST+1);

/*
 * Enable interrupts
 */
static void board_init(void)
{
	/*
	 * Enable USB-C interrupts.
	 */
	gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c0));
	if (board_get_usb_pd_port_count() == 2)
		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_usb_c1));

	/*
	 * TODO: Enable HDMI HPD: forward HPD input from sub-board to GPK7.
	 * Both edges.
	 */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

__override void board_hibernate(void)
{
	/* Shut down the chargers */
	if (board_get_usb_pd_port_count() == 2)
		sm5803_hibernate(CHARGER_SECONDARY);
	sm5803_hibernate(CHARGER_PRIMARY);
	LOG_INF("Charger(s) hibernated");
	cflush();
}

/* Trigger shutdown by enabling the Z-sleep circuit */
__override void board_hibernate_late(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_slp_z), 1);
	/*
	 * The system should hibernate, but there may be
	 * a small delay, so return.
	 */
}
