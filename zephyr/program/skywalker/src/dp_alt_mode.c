/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"
#include "hooks.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_dp_hpd_gpio.h"
#include "usbc/pdc_power_mgmt.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(skywalker_usbc, LOG_LEVEL_INF);

uint64_t svdm_hpd_deadline[CONFIG_USB_PD_PORT_MAX_COUNT];
static bool in_cooldown;
static bool dp_attached[2];

int svdm_get_hpd_gpio(int port)
{
	return gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l));
}

void svdm_set_hpd_gpio(int port, int en)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l), en);
}

static void detach_cooldown(void)
{
	in_cooldown = false;

	if (dp_attached[0]) {
		svdm_set_hpd_gpio(0, 1);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_dp_aux_path_sel), 0);
	} else if (dp_attached[1]) {
		svdm_set_hpd_gpio(1, 1);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_dp_aux_path_sel), 1);
	}

	LOG_INF("\x1b[1;31mcooldown expired\x1b[m");
}
DECLARE_DEFERRED(detach_cooldown);

static bool is_active(int port)
{
	if (port == 0)
		return dp_attached[0];
	
	return !dp_attached[0] && dp_attached[1];
}

static void attach_dp(int port)
{
	LOG_INF("\x1b[1;31mport %d attach\x1b[m", port);

	if (in_cooldown) {
		// do nothing
	} else if (port == 0) {
		if (is_active(1)) {
			LOG_INF("\x1b[1;31mdetach c1, start 1s cd\x1b[m");
			svdm_set_hpd_gpio(port, 0);
			in_cooldown = true;
			hook_call_deferred(&detach_cooldown_data, USEC_PER_SEC);
		} else {
			svdm_set_hpd_gpio(port, 1);
			gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_dp_aux_path_sel), port);
		}
	} else if (!is_active(0) && port == 1) {
		svdm_set_hpd_gpio(port, 1);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_dp_aux_path_sel), port);
	}


	dp_attached[port] = true;
}

static void detach_dp(int port)
{
	LOG_INF("\x1b[1;31mport %d detach\x1b[m", port);
	if (is_active(port)) {
		LOG_INF("\x1b[1;31mstart 1s cd\x1b[m");
		svdm_set_hpd_gpio(port, 0);
		if (!in_cooldown) {
			in_cooldown = true;
			hook_call_deferred(&detach_cooldown_data, USEC_PER_SEC);
		}
	}

	dp_attached[port] = false;
}

static void skywalker_dp_attention(int port, uint32_t vdo_dp_status)
{
	int lvl = PD_VDO_DPSTS_HPD_LVL(vdo_dp_status);
	int irq = PD_VDO_DPSTS_HPD_IRQ(vdo_dp_status);

	if (lvl) {
		attach_dp(port);
	} else {
		detach_dp(port);
	}

	if (in_cooldown) {
		return;
	}

	int cur_lvl = svdm_get_hpd_gpio(port);

	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND) && (irq || lvl)) {
		/*
		 * Wake up the AP.  IRQ or level high indicates a DP sink is now
		 * present.
		 */
		pd_notify_dp_alt_mode_entry(port);
	}

	if (irq && !lvl) {
		/*
		 * IRQ can only be generated when the level is high, because
		 * the IRQ is signaled by a short low pulse from the high level.
		 */
		LOG_ERR("ERR:HPD:IRQ&LOW\n");
		return;
	}

	if (irq && cur_lvl) {
		uint64_t now = get_time().val;

		/* wait for the minimum spacing between IRQ_HPD if needed */
		if (now < svdm_hpd_deadline[port]) {
			k_usleep(svdm_hpd_deadline[port] - now);
		}

		/* generate IRQ_HPD pulse */
		svdm_set_hpd_gpio(port, 0);
		/*
		 * b/171172053#comment14: since the HPD_DSTREAM_DEBOUNCE_IRQ is
		 * very short (500us), we can use k_busy_wait for more stable
		 * pulse period.
		 */
		k_busy_wait(HPD_DSTREAM_DEBOUNCE_IRQ);
		svdm_set_hpd_gpio(port, 1);
	} else {
		svdm_set_hpd_gpio(port, lvl);
	}

	/* set the minimum time delay (2ms) for the next HPD IRQ */
	svdm_hpd_deadline[port] = get_time().val + HPD_USTREAM_DEBOUNCE_LVL;

	host_set_single_event(EC_HOST_EVENT_USB_MUX);

	return;
}

static void skywalker_set_unattached(int port)
{
	detach_dp(port);
}

static int skywalker_pdc_cb_init(void)
{
	pdc_power_mgmt_register_board_callback(PDC_BOARD_CB_UNATTACH,
					       skywalker_set_unattached);
	pdc_power_mgmt_register_board_callback(PDC_BOARD_CB_DP_ATTENTION,
					       skywalker_dp_attention);
	return 0;
}
SYS_INIT(skywalker_pdc_cb_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
