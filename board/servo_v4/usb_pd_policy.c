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
			 PDO_FIXED_COMM_CAP | PDO_FIXED_EXTERNAL)

/*
 * Dynamic PDO that reflects capabilities present on the CHG port. Allow for two
 * entries since DUT port could be in the middle of sending a pdo when a CHG
 * port Vbus state change occurs.
 */
static uint32_t pd_src_chg_pdo[2];
static uint8_t chg_pdo_idx;
static const uint32_t pd_src_host_pdo[] = {
		PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
};
static const int pd_src_host_pdo_cnt = ARRAY_SIZE(pd_src_host_pdo);

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
static int vbus_ma_chg;
static int vbus_mv_chg;
/* Voltage thresholds for no connect */
static int pd_src_vnc[TYPEC_RP_RESERVED][2] = {
	{PD_SRC_3_0_VNC_MV, PD_SRC_1_5_VNC_MV},
	{PD_SRC_1_5_VNC_MV, PD_SRC_DEF_VNC_MV},
	{PD_SRC_3_0_VNC_MV, PD_SRC_DEF_VNC_MV},
};
/* Voltage thresholds for Ra attach */
static int pd_src_rd_threshold[TYPEC_RP_RESERVED][2] = {
	{PD_SRC_3_0_RD_THRESH_MV, PD_SRC_DEF_RD_THRESH_MV},
	{PD_SRC_1_5_RD_THRESH_MV, PD_SRC_DEF_RD_THRESH_MV},
	{PD_SRC_3_0_RD_THRESH_MV, PD_SRC_DEF_RD_THRESH_MV},
};

static void board_manage_dut_port(int port)
{
	int rp;

	/*
	 * This function is called by the DUT port only when Vbus is being
	 * enabled and it's called by the CHG port when the CHG port Vbus
	 * changes state.
	 */

	/* Update the Rp value being used. Assume the default value of Rp */
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

	/* Update PD contract to reflect new available CHG voltage/current */
	pd_update_contract(DUT);
}

static void board_update_chg_port(void)
{
	/* Update the voltage/current values for CHG port */
	vbus_volt_mv[CHG] = vbus_mv_chg;
	vbus_curr_ma[CHG] = vbus_ma_chg;
	/* Set VBUS status for CHG port */
	vbus_en[CHG] = vbus_mv_chg >= 5000 ? 1 : 0;
	/*
	 * CHG Vbus has changed states, update PDO that reflects CHG port
	 * state
	 */
	if (!vbus_en[CHG]) {
		/* CHG Vbus has dropped, so always source DUT Vbus from host */
		gpio_set_level(GPIO_HOST_OR_CHG_CTL, 0);
	} else {
		/* CHG port is in steady state, can update its PDO */
		pd_src_chg_pdo[chg_pdo_idx ^ 1] = PDO_FIXED_VOLT(vbus_mv_chg) |
				PDO_FIXED_CURR(vbus_ma_chg) | PDO_FIXED_FLAGS;
		/* Point to current chg pdo entry */
		chg_pdo_idx ^= 1;
	}

	/* Call DUT port manager to update Rp and possible PD contract */
	board_manage_dut_port(CHG);
}
DECLARE_DEFERRED(board_update_chg_port);

static void board_manage_chg_port(int port, int max_ma, int vbus_mv)
{

	/* Only concerned about changes in Vbus on CHG port */
	if (port == DUT)
		return;

	/* Check that VBUS isn't higher than what servo_v4 allows */
	if (vbus_mv > PD_MAX_VOLTAGE_MV)
		return;

	/*
	 * Determine if vbus from CHG port has changed values and if the current
	 * state of CHG vbus is on or off. If the change is on, then schedule a
	 * deffered callback. If the change is off, then act immediately.
	 */
	if (vbus_mv == vbus_volt_mv[port])
		/* No change in CHG VBUS detected, nothing else to do. */
		return;

	/* Save CHG port voltage and current levels */
	vbus_mv_chg = vbus_mv;
	vbus_ma_chg = max_ma;

	/* Cancel any pending deferred call */
	/*
	 * TODO(scollyer): Are there issues with calling the function
	 * immediately vs deferred given that one would be in pd_task and the
	 * other would be in the hook_task.
	 */
	hook_call_deferred(&board_update_chg_port_data, -1);
	if (vbus_mv)
		hook_call_deferred(&board_update_chg_port_data, 250 * MSEC);
	else
		/* Update CHG port status now since vbus is off */
		board_update_chg_port();
}

int pd_tcpc_cc_nc(int port, int cc_volt, int cc_sel)
{
	int rp_index;

	/* Can never be called from CHG port as it's sink only */
	if (port == CHG)
		return 0;

	rp_index = vbus_rp;
	/* Ensure that rp_index doens't exceed the array size */
	if (rp_index >= TYPEC_RP_RESERVED)
		rp_index = 0;

	return cc_volt >= pd_src_vnc[rp_index][cc_sel];
}

int pd_tcpc_cc_ra(int port, int cc_volt, int cc_sel)
{
	int rp_index;

	/* Can never be called from CHG port as it's sink only */
	if (port == CHG)
		return 0;

	rp_index = vbus_rp;
	/* Ensure that rp_index doens't exceed the array size */
	if (rp_index >= TYPEC_RP_RESERVED)
		rp_index = 0;

	return cc_volt < pd_src_rd_threshold[rp_index][cc_sel];
}

int board_select_rp_value(int port, int rp)
{
	return pd_set_rp_rd(port, TYPEC_CC_RP, rp);
}

int charge_manager_get_source_pdo(const uint32_t **src_pdo)
{
	/*
	 * If CHG is providing VBUS, then advertise what's available on the CHG
	 * port, otherwise used the fixed value that matches host capabilities.
	 */
	if (vbus_en[CHG])
		*src_pdo =  &pd_src_chg_pdo[chg_pdo_idx];
	else
		*src_pdo =  pd_src_host_pdo;

	return pd_src_host_pdo_cnt;
}

int pd_is_valid_input_voltage(int mv)
{
	/* Any voltage less than the max is allowed */
	return 1;
}

void pd_transition_voltage(int idx)
{
	/*
	 * Up to this point, VBUS will have been supplied by host. If
	 * vbus_en[CHG] is set, then that means the CHG port is in a steady
	 * state condition and its voltage/current values have been communicated
	 * to the DUT in the SRC_CAP message. Therefore, it's always safe here
	 * to switch from host supplied Vbus to CHG port Vbus.
	 *
	 * TODO(scollyer): Do I need to verify that CHG port Vbus voltage
	 * matches what was in the RDO and if so, how can that RDO be accessed
	 * from here.
	 */
	if (vbus_en[CHG])
		gpio_set_level(GPIO_HOST_OR_CHG_CTL, 1);
}

int pd_set_power_supply_ready(int port)
{
	/* Port 0 can never provide vbus. */
	if (port == CHG)
		return EC_ERROR_INVAL;

	/* Update DUT Rp (if necessary) based on CHG port VBUS status */
	board_manage_dut_port(port);

	/* Only ever allow host vbus at this point */
	gpio_set_level(GPIO_HOST_OR_CHG_CTL, 0);

	/* Enable VBUS */
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
	board_manage_chg_port(port, max_ma, supply_voltage);
}

void typec_set_input_current_limit(int port, uint32_t max_ma,
				   uint32_t supply_voltage)
{
	board_manage_chg_port(port, max_ma, supply_voltage);
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
	 * only. Don't allow DUT port to accept a power role swap request. More
	 * support still needs to be added so that servo_v4 DUT port behaves
	 * properly when acting as a SNK device.
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
