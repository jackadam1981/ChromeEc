/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"
#include "hooks.h"
#include "rauru_dp.h"
#include "timer.h"
#include "typec_control.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dp_hpd_gpio.h"
#include "usbc/pdc_power_mgmt.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)

int svdm_get_hpd_gpio(int port)
{
	if (port == 0) {
		/* HPD is low active, inverse the result */
		return !gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l));

	} else {
		return gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_usb_c1_dp_in_hpd));
	}
}

void svdm_set_hpd_gpio(int port, int en)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l), !en);
	if (port == 0) {
		/* HPD is low active, inverse the result */
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_ap_dp_hpd_l),
				!en);

	} else {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c1_dp_in_hpd),
				en);
	}
}

void hylia_dp_attention(int port, uint32_t vdo_dp_status)
{
	/* TODO(b:380121396): implement me */
	int lvl = PD_VDO_DPSTS_HPD_LVL(vdo_dp_status);
	int irq = PD_VDO_DPSTS_HPD_IRQ(vdo_dp_status);
	mux_state_t mux_state, mux_mode;

	mux_mode = pdc_power_mgmt_get_dp_mux_mode(port);

	if (lvl) {
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
	svdm_set_hpd_gpio(port, 0);
}

void xhci_interrupt(enum gpio_signal signal)
{
	/* TODO: implement me */
}

static void hylia_pdc_cb_init(void)
{
	/* TODO(b:380121396): implement me */
	pdc_power_mgmt_register_board_callback(PDC_BOARD_CB_UNATTACH,
					       hylia_set_unattached);
	pdc_power_mgmt_register_board_callback(PDC_BOARD_CB_DP_ATTENTION,
					       hylia_dp_attention);
}
DECLARE_HOOK(HOOK_INIT, hylia_pdc_cb_init, HOOK_PRIO_LAST);
