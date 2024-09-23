/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Rauru-PDC DP functions
 */

#include "chipset.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "rauru_dp.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dp_hpd_gpio.h"
#include "usbc/pdc_power_mgmt.h"

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rauru_pdc, LOG_LEVEL_DBG);

static int active_dp_port = DP_PORT_NONE;
uint64_t svdm_hpd_deadline[CONFIG_USB_PD_PORT_MAX_COUNT];

void hdmi_hpd_interrupt(enum gpio_signal signal)
{
	/* stub */
}

int rauru_is_dp_muxable(enum rauru_dp_port port)
{
	return port == active_dp_port || active_dp_port == DP_PORT_NONE;
}

bool rauru_is_hpd_high(enum rauru_dp_port port)
{
#if CONFIG_RAURU_BOARD_HAS_HDMI_SUPPORT
	if (port == DP_PORT_HDMI) {
		return gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_hdmi_ec_hpd));
	}
#endif

	return PD_VDO_DPSTS_HPD_LVL(pdc_power_mgmt_get_dp_status(port));
}

void rauru_set_dp_path(enum rauru_dp_port port)
{
	const struct gpio_dt_spec *c1_en =
		GPIO_DT_FROM_NODELABEL(gpio_dp_path_usb_c1_en);
	const struct gpio_dt_spec *hdmi_en =
		GPIO_DT_FROM_NODELABEL(gpio_dp_path_hdmi_en);
	const struct gpio_dt_spec *dp_in_hpd[] = {
		GPIO_DT_FROM_NODELABEL(gpio_usb_c0_pdc_ec_hpd),
		GPIO_DT_FROM_NODELABEL(gpio_usb_c1_pdc_ec_hpd),
	};

	if (port == active_dp_port) {
		return;
	}

	/* Enable retimer/redriver transmitting */
	for (int i = 0; i < DP_PORT_HDMI; i++) {
		gpio_pin_set_dt(dp_in_hpd[i], i == port);
	}

	/*
	 * DP Pipe -> DPMux -> PDC-C0 (C1_EN)
	 *              |----> DP Mux -> HDMI
	 *                       |-----> C0 (Unable to mux to)
	 * Also, set EN pin to LOW for power saving when unused.
	 */
	if (port == DP_PORT_C0) {
		gpio_pin_set_dt(c1_en, 1);
		gpio_pin_set_dt(hdmi_en, 0);
	} else if (port == DP_PORT_HDMI) {
		gpio_pin_set_dt(c1_en, 0);
		gpio_pin_set_dt(hdmi_en, 1);
	}

	if (port == DP_PORT_NONE) {
		svdm_set_hpd_gpio(active_dp_port, 0);
	}

	active_dp_port = port;
	LOG_INF("DP p%d", port);
}

/* stub out */
bool rauru_has_hdmi_port(void)
{
	return true;
}

void rauru_detach_dp_path(enum rauru_dp_port port)
{
	if (port != active_dp_port) {
		return;
	}

	/* Detach and then rotate. Priority: HDMI -> C0 -> C1 */
#if CONFIG_RAURU_BOARD_HAS_HDMI_SUPPORT
	if (rauru_has_hdmi_port() && port != DP_PORT_HDMI &&
	    rauru_is_hpd_high(DP_PORT_HDMI)) {
		rauru_set_dp_path(DP_PORT_HDMI);
		return;
	}
#endif

	for (int i = 0; i < board_get_usb_pd_port_count(); i++) {
		if (i != port && rauru_is_hpd_high(i)) {
			/* TODO(yllin): should set IRQ_HPD as well? */
			rauru_set_dp_path(i);
			return;
		}
	}

	/* no other active port,  */
	rauru_set_dp_path(DP_PORT_NONE);
}

int svdm_get_hpd_gpio(int port)
{
	/* HPD is low active, inverse the result */
	return !gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l));
}

void svdm_set_hpd_gpio(int port, int en)
{
	if (port != active_dp_port)
		return;

	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l), !en);
}

void svdm_set_hpd_gpio_irq(int port)
{
	svdm_set_hpd_gpio(port, 0);

	if (IS_ENABLED(CONFIG_USB_PD_DP_HPD_GPIO_IRQ_ACCURATE)) {
		udelay(HPD_DSTREAM_DEBOUNCE_IRQ);
	} else {
		crec_usleep(HPD_DSTREAM_DEBOUNCE_IRQ);
	}

	svdm_set_hpd_gpio(port, 1);
}

enum ec_error_list rauru_dp_hpd_gpio_set(int port, bool level, bool irq)
{
	int cur_level = svdm_get_hpd_gpio(port);

	if (irq && !level) {
		/*
		 * IRQ can only be generated when the level is high, because
		 * the IRQ is signaled by a short low pulse from the high level.
		 */
		LOG_ERR("ERR:HPD:IRQ&LOW\n");
		return EC_ERROR_INVAL;
	}

	if (irq && cur_level) {
		uint64_t now = get_time().val;
		/* wait for the minimum spacing between IRQ_HPD if needed */
		if (now < svdm_hpd_deadline[port])
			crec_usleep(svdm_hpd_deadline[port] - now);

		svdm_set_hpd_gpio_irq(port);
	} else {
		svdm_set_hpd_gpio(port, level);
	}

	/* set the minimum time delay (2ms) for the next HPD IRQ */
	svdm_hpd_deadline[port] = get_time().val + HPD_USTREAM_DEBOUNCE_LVL;

	return EC_SUCCESS;
}

__override void rauru_pdc_dp_attention(int port, uint32_t vdo_dp_status)
{
	int lvl = PD_VDO_DPSTS_HPD_LVL(vdo_dp_status);
	int irq = PD_VDO_DPSTS_HPD_IRQ(vdo_dp_status);
	mux_state_t mux_state, mux_mode;

	mux_mode = pdc_power_mgmt_get_dp_mux_mode(port);
	if (!rauru_is_dp_muxable(port)) {
		/* TODO(waihong): Info user? */
		LOG_DBG("p%d: The other port is already muxed.", port);
		return;
	}

	if (lvl) {
		/* connect the DP pipeline before setting HPD if HPG high */
		rauru_set_dp_path(port);
		usb_mux_set(port, mux_mode, USB_SWITCH_CONNECT,
			    polarity_rm_dts(pd_get_polarity(port)));
	} else {
		usb_mux_set(port, mux_mode & (~USB_PD_MUX_DP_ENABLED),
			    USB_SWITCH_CONNECT,
			    polarity_rm_dts(pd_get_polarity(port)));
	}

	if (rauru_dp_hpd_gpio_set(port, lvl, irq)) {
		return;
	}

	if (!lvl) {
		/* detach DP pipeline after setting HPD */
		rauru_detach_dp_path(port);
	}

	/*
	 * Populate MUX state before dp path mux, so we can keep the HPD status.
	 */
	mux_state = (lvl ? USB_PD_MUX_HPD_LVL : USB_PD_MUX_HPD_LVL_DEASSERTED) |
		    (irq ? USB_PD_MUX_HPD_IRQ : USB_PD_MUX_HPD_IRQ_DEASSERTED);
	usb_mux_hpd_update(port, mux_state);

	return;
}

void rauru_pdc_set_unattached(int port)
{
	rauru_detach_dp_path(port);
}
