/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared USB-C policy for IT83xx and IT8xxx2 development boards */

#include "adc.h"
#include "config.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "it83xx_pd.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_mux.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* ---------------- Power Data Objects (PDOs) ----------------- */
#ifdef CONFIG_USB_PD_CUSTOM_PDO
#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_UNCONSTRAINED  | PDO_FIXED_COMM_CAP)

/* Threshold voltage of VBUS provided (mV) */
#define PD_VBUS_PROVIDED_THRESHOLD 3900

const uint32_t pd_src_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
	PDO_BATT(4500, 14000, 10000),
	PDO_VAR(4500, 14000, 3000),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);
#endif

int pd_is_max_request_allowed(void)
{
	/* max voltage request allowed */
	return 1;
}

int pd_snk_is_vbus_provided(int port)
{
	int mv = ADC_READ_ERROR;

	switch (port) {
	case USBPD_PORT_A:
		mv = adc_read_channel(ADC_VBUSSA);
		break;
	case USBPD_PORT_B:
		mv = adc_read_channel(ADC_VBUSSB);
		break;
	case USBPD_PORT_C:
		mv = adc_read_channel(ADC_VBUSSC);
		break;
	}

	return mv > PD_VBUS_PROVIDED_THRESHOLD;
}

int pd_set_power_supply_ready(int port)
{
	/* provide VBUS */
	board_pd_vbus_ctrl(port, 1);
	/* vbus provided or not */
	return !pd_snk_is_vbus_provided(port);
}

void pd_power_supply_reset(int port)
{
	/* Kill VBUS */
	board_pd_vbus_ctrl(port, 0);
}


__override int pd_check_data_swap(int port, enum pd_data_role data_role)
{
	/* Always allow data swap: we can be DFP or UFP for USB */
	return 1;
}

int pd_check_vconn_swap(int port)
{
	/*
	 * VCONN is provided directly by the battery(PPVAR_SYS)
	 * but use the same rules as power swap
	 */
	return pd_get_dual_role(port) == PD_DRP_TOGGLE_ON ? 1 : 0;
}

/* ----------------- Vendor Defined Messages ------------------ */
__override int svdm_enter_dp_mode(int port, uint32_t mode_caps)
{
	/*
	 * Do not enter dp mode, we let VDM enumeration stop after discover
	 * modes have done.
	 */
	return -1;
}

__override void svdm_dp_post_config(int port)
{
}

__override int svdm_dp_attention(int port, uint32_t *payload)
{
	/* ack */
	return 1;
}

__override void svdm_exit_dp_mode(int port)
{
}

__override int pd_custom_vdm(int port, int cnt, uint32_t *payload,
				uint32_t **rpayload)
{
	return 0;
}

__override int svdm_dp_config(int port, uint32_t *payload)
{
	int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);
	uint8_t pin_mode = get_dp_pin_mode(port);

	if (!pin_mode)
		return 0;

	payload[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_CONFIG | VDO_OPOS(opos));
	payload[1] = VDO_DP_CFG(pin_mode,      /* pin mode */
				1,	       /* DPv1.3 signaling */
				2);	       /* UFP connected */
	return 2;
};
