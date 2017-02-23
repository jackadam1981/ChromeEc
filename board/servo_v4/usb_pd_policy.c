/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "charge_manager.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "tcpm.h"
#include "timer.h"
#include "util.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "usb_pd_tcpm.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/* Define typical operating power and max power */
/*#define OPERATING_POWER_MW 15000 */
/*#define MAX_POWER_MW       60000 */
/*#define MAX_CURRENT_MA     3000 */

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP)

const uint32_t pd_src_pdo[] = {
		PDO_FIXED(5000,   900, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

const uint32_t pd_snk_pdo[] = {
		PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
		PDO_BATT(4750, 21000, 15000),
		PDO_VAR(4750, 21000, 3000),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

static uint8_t vbus_en[CONFIG_USB_PD_PORT_COUNT] = {0, 0};
static uint8_t vbus_rp = TYPEC_RP_RESERVED;
static int vbus_volt_mv[CONFIG_USB_PD_PORT_COUNT] = {0, 0};
static int vbus_curr_ma[CONFIG_USB_PD_PORT_COUNT] = {0, 0};

static void board_manage_port(int port)
{
	int rp;

	/*
	 * This function is called by CHG port only when there has been a change
	 * in the CHG port VUBS status. It is called by the DUT port in SRC mode
	 * when the VBUS is enabled or there is a detach event.
	 *
	 */

	/* Assume the default value of Rp */
	rp = TYPEC_RP_USB;
	if (vbus_curr_ma[CHG] >= 3000)
		/* CHG port is connected and DUt can advertise 3A */
		rp = TYPEC_RP_3A0;
	else if (vbus_curr_ma[CHG] >= 1500)
		rp = TYPEC_RP_1A5;

	if (vbus_rp == rp) {
		/* No change in DUT Rp value from current setting */
		return;
	}

	/* Save new Rp value for DUT port */
	vbus_rp = rp;
	/* Present new Rp value */
	tcpm_select_rp_value(DUT, rp);
	/* If DUT VBUS is currently enabled and Rp has changed, then the VBUS
	 * source may need to be changed as well.
	 */
	if (vbus_en[DUT]) {
		pd_set_power_supply_ready(DUT);
	}

	/* Update PD contract to reflect new available CHG voltage/current */
	/*
	 * TODO(crbug.com/p/61878): When messaging support has been added, then
	 * will update the PD contract here so changes related to the new Rp can
	 * be taken in to consideration.
	 * pd_update_contract(port);
	 */
}

static void board_update_chg_state(int port, int max_ma, int vbus_mv)
{
	int vbus_state;

	if (port == DUT)
		return;

	/* Check that VBUS isn't higher than what servo_v4 allows */
	if (vbus_mv > PD_MAX_VOLTAGE_MV)
		return;

	/* Save new voltage and current levels for this port */
	vbus_volt_mv[port] = vbus_mv;
	vbus_curr_ma[port] = max_ma;
	vbus_state = vbus_en[port];
	/* Save VBUS status for this port */
	vbus_en[port] = vbus_mv >= 5000 ? 1 : 0;;
	if (vbus_state == vbus_en[port])
		/* No change in VBUS detected, nothing else to do. */
		return;
	/* Call board port manager to possibly trigger disconnect on DUT port */
	board_manage_port(port);
}

int board_select_rp_value(int port, int rp)
{
	return pd_set_rp_rd(port, TYPEC_CC_RP, rp);
}

int pd_is_valid_input_voltage(int mv)
{
	/* Any voltage less than the max is allowed */
	return 1;
}

void pd_transition_voltage(int idx)
{
	/*
	 * TODO(crosbug.com/p/60794): Most likely this function is a don't care
	 * for servo_v4 since VBUS provided to the DUT port has just an on/off
	 * control.  For now leave it as a no-op.
	 */
}

int pd_set_power_supply_ready(int port)
{
	int vbus_chg_or_host;

	/* Port 0 can never provide vbus. */
	if (port == CHG)
		return EC_ERROR_INVAL;

	/* Update DUT Rp (if necessary) based on CHG port VBUS status */
	board_manage_port(port);

	/* Select DUT VUBS source based on presence of CHG port VBUS */
	vbus_chg_or_host = vbus_volt_mv[CHG] > 0 ? 1 : 0;

	/*
	 * TODO(crosbug.com/p/60794): Currently CHG VBUS is always limited to 5V
	 * by the macro PD_MAX_VOLTAGE_MV. Therefore, if VBUS is present on the
	 * CHG port then it's always safe to use that VBUS since it can't be
	 * higher than 5V. However, when PD messaging is supported, then will
	 * need to account for VBUS on CHG port being > 5V and will need to
	 * ensure a PD contract is in place on DUT port prior to
	 * enabling. vbus_volt_mv[] is set to non-zero values only when a
	 * PS_READY message is receieved so don't need to worry about CHG VBUS
	 * being safe 5V but about to transition to a higher value.
	 */

	/*
	 * Select Host as source for VBUS.
	 * To select host, set GPIO_HOST_OR_CHG_CTL low. To select CHG as VBUS
	 * source, then set GPIO_HOST_OR_CHG_CTL high.
	 */
	gpio_set_level(GPIO_HOST_OR_CHG_CTL, vbus_chg_or_host);

	/* Enable VBUS from the source selected above. */
	vbus_en[port] = 1;
	gpio_set_level(GPIO_DUT_CHG_EN, 1);

	return EC_SUCCESS; /* we are ready */
}

void pd_power_supply_reset(int port)
{
	/* Disable VBUS */
	gpio_set_level(GPIO_DUT_CHG_EN, 0);

	/* Set default VBUS source to Host */
	gpio_set_level(GPIO_HOST_OR_CHG_CTL, 0);
	/* Indicate that VBUS is not being supplied by this port */
	vbus_en[port] = 0;
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	board_update_chg_state(port, max_ma, supply_voltage);
}

void typec_set_input_current_limit(int port, uint32_t max_ma,
				   uint32_t supply_voltage)
{
	/*
	 * TODO(crosbug.com/p/60794): Placeholder for now so that can compile
	 * with USB PD support.
	 */
}

int pd_snk_is_vbus_provided(int port)
{
	return gpio_get_level(port ? GPIO_USB_DET_PP_DUT :
				     GPIO_USB_DET_PP_CHG);
}

int pd_board_checks(void)
{
	return EC_SUCCESS;
}

int pd_check_power_swap(int port)
{
	/*
	 * TODO(crosbug.com/p/60792): CHG port can't do a power swap as it's SNK
	 * only. DUT port should be able to support a power role swap, but VBUS
	 * will need to be present. For now, don't allow swaps on either port.
	 */
	return 0;
}

int pd_check_data_swap(int port, int data_role)
{
	/* Servo can allow data role swaps */
	return 1;
}

void pd_execute_data_swap(int port, int data_role)
{
	/* Should we do something here? */
}

void pd_check_pr_role(int port, int pr_role, int flags)
{
	/*
	 * TODO(crosbug.com/p/60792): CHG port can't do a power swap as it's SNK
	 * only. DUT port should be able to support a power role swap, but VBUS
	 * will need to be present. For now, don't allow swaps on either port.
	 */

}

void pd_check_dr_role(int port, int dr_role, int flags)
{
	/*
	 * TODO(crosbug.com/p/60792): CHG port is SNK only and should not need
	 * to change from default UFP role. DUT port behavior needs to be
	 * flushed out. Don't request any data role change for either port for
	 * now.
	 */
}


/* ----------------- Vendor Defined Messages ------------------ */
const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int pd_custom_vdm(int port, int cnt, uint32_t *payload,
		  uint32_t **rpayload)
{
	int cmd = PD_VDO_CMD(payload[0]);

	/* make sure we have some payload */
	if (cnt == 0)
		return 0;

	switch (cmd) {
	case VDO_CMD_VERSION:
		/* guarantee last byte of payload is null character */
		*(payload + cnt - 1) = 0;
		CPRINTF("ver: %s\n", (char *)(payload+1));
		break;
	case VDO_CMD_CURRENT:
		CPRINTF("Current: %dmA\n", payload[1]);
		break;
	}

	return 0;
}



const struct svdm_amode_fx supported_modes[] = {};
const int supported_modes_cnt = ARRAY_SIZE(supported_modes);
