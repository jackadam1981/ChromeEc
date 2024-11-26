/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Hylia DP functions
 */

#include "gpio_signal.h"
#include "rauru_dp.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dp_hpd_gpio.h"
#include "usbc/pdc_power_mgmt.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hylia_usbc, LOG_LEVEL_DBG);

void hylia_dp_attention(int port, uint32_t vdo_dp_status)
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

	if (dp_hpd_gpio_set(port, lvl, irq)) {
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

void hylia_set_unattached(int port)
{
	rauru_detach_dp_path(port);
}
