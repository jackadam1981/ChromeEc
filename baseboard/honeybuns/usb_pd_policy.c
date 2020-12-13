/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "chip/stm32/ucpd-stm32gx.h"
#include "driver/tcpm/tcpci.h"
#include "driver/mp4245.h"
#include "task.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"
#include "usbc_ppc.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define PDO_FIXED_FLAGS_EXT (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			     PDO_FIXED_COMM_CAP | PDO_FIXED_UNCONSTRAINED)

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP | PDO_FIXED_UNCONSTRAINED)

/* Voltage indexes for the PDOs */
enum volt_idx {
	PDO_IDX_5V   = 0,
	PDO_IDX_9V   = 1,
	PDO_IDX_15V  = 2,
	PDO_IDX_20V  = 3,
	/* TODO: add PPS support */
	PDO_IDX_COUNT
};

const uint32_t pd_src_host_pdo[] = {
	[PDO_IDX_5V]  = PDO_FIXED(5000,   3000, PDO_FIXED_FLAGS),
	[PDO_IDX_9V]  = PDO_FIXED(9000,   3000, PDO_FIXED_FLAGS),
	[PDO_IDX_15V]  = PDO_FIXED(15000, 3000, PDO_FIXED_FLAGS),
	[PDO_IDX_20V]  = PDO_FIXED(20000, 3000, PDO_FIXED_FLAGS),
};
BUILD_ASSERT(ARRAY_SIZE(pd_src_host_pdo) == PDO_IDX_COUNT);

const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

int charge_manager_get_source_pdo(const uint32_t **src_pdo, const int port)
{
	int pdo_cnt = 0;

	*src_pdo =  pd_src_host_pdo;
	pdo_cnt = ARRAY_SIZE(pd_src_host_pdo);

	return pdo_cnt;
}

int pd_check_vconn_swap(int port)
{
	/*TODO: Dock is the Vconn source */
	return 1;
}

void pd_power_supply_reset(int port)
{
	int prev_en;

	if (port < 0 || port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

	if (port == USB_PD_PORT_HOST) {
		/* Turn off voltage output from buck-boost */
		mp4245_votlage_out_enable(0);
		/* Reset VBUS voltage to default value (fixed 5V SRC_CAP) */
		pd_transition_voltage(1);
	}
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	if (port == USB_PD_PORT_HOST) {
		/* Ensure buck-boost is enabled and Vout is on */
		mp4245_votlage_out_enable(1);
		msleep(MP4245_VOUT_5V_DELAY_MS);
	}

	/*
	 * Default operation of buck-boost is 5v/3.6A.
	 * Turn on the PPC Provide Vbus.
	 */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

	return EC_SUCCESS;
}

void pd_transition_voltage(int idx)
{
	int port = TASK_ID_TO_PD_PORT(task_get_current());

	if (port == USB_PD_PORT_HOST) {
		int mv;
		int ma;
		int vbus_thresh;
		int i;

	/*
	 * Set the VBUS output voltage and current limit to the values specified
	 * by the PDO requested by sink. Note that USB PD uses idx = 1 for 1st
	 * PDO of SRC_CAP which must aways be 5V fixed supply.
	 */
		pd_extract_pdo_power(pd_src_host_pdo[idx - 1], &ma, &mv);

		/* Set VBUS level to value specified in the requested PDO */
		mp4245_set_voltage_out(mv);
		/* Wait for vbus to be with 95% of its target value */
		vbus_thresh = mv - (mv >> 4);

		for (i = 0; i < 20; i++) {
			int rv;

			rv =  mp3245_get_vbus(&mv, &ma);
			if ((rv == EC_SUCCESS) && (mv >= vbus_thresh))
				return;
			msleep(2);
		}
	}
}

int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{

}
