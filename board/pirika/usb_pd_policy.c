/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/charger/isl923x_public.h"
#include "driver/tcpm/tcpci.h"
#include "usb_pd.h"
#include "battery_smart.h"
#include "charger.h"
#include "driver/charger/sm5803.h"
#include "cbi_ssfc.h"
#include "usb_pd_flags.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

int pd_check_vconn_swap(int port)
{
	/* Allow VCONN swaps if the AP is on. */
	return chipset_in_state(CHIPSET_STATE_ANY_SUSPEND | CHIPSET_STATE_ON);
}

void pd_power_supply_reset(int port)
{
	if (get_cbi_ssfc_charger_type() == SSFC_CHARGER_SM5803) {
		int prev_en;

		if (port < 0 || port >= board_get_usb_pd_port_count())
			return;

		prev_en = charger_is_sourcing_otg_power(port);

		/* Disable Vbus */
		charger_enable_otg_power(port, 0);

		/* Discharge Vbus if previously enabled */
		if (prev_en)
			sm5803_set_vbus_disch(port, 1);
	} else {
		/* Disable VBUS */
		tcpc_write(port, TCPC_REG_COMMAND,
		TCPC_REG_COMMAND_SRC_CTRL_LOW);
	}

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	if (get_cbi_ssfc_charger_type() == SSFC_CHARGER_SM5803) {
		enum ec_error_list rv;

		/* Disable sinking */
		rv = sm5803_vbus_sink_enable(port, 0);
		if (rv)
			return rv;

		/* Disable Vbus discharge */
		sm5803_set_vbus_disch(port, 0);

		/* Provide Vbus */
		charger_enable_otg_power(port, 1);
	} else {
		int rv;

		if (port >= board_get_usb_pd_port_count())
			return EC_ERROR_INVAL;

		/* Disable charging. */
		rv = tcpc_write(port, TCPC_REG_COMMAND,
		TCPC_REG_COMMAND_SNK_CTRL_LOW);

		if (rv)
			return rv;

		/* Our policy is not to source VBUS when the AP is off. */
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			return EC_ERROR_NOT_POWERED;

		/* Provide Vbus. */
		rv = tcpc_write(port, TCPC_REG_COMMAND,
		TCPC_REG_COMMAND_SRC_CTRL_HIGH);

		if (rv)
			return rv;

		rv = raa489000_enable_asgate(port, true);
		if (rv)
			return rv;
	}

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}

__override bool pd_check_vbus_level(int port, enum vbus_level level)
{
	if (get_cbi_ssfc_charger_type() == SSFC_CHARGER_SM5803) {
		int vbus_voltage;

		/*
		 * If we're unable to speak to the charger,
		 * best to guess false
		 */
		if (charger_get_vbus_voltage(port, &vbus_voltage))
			return false;

		if (level == VBUS_SAFE0V)
			return vbus_voltage < PD_V_SAFE0V_MAX;
		else if (level == VBUS_PRESENT)
			return vbus_voltage > PD_V_SAFE5V_MIN;
		else
			return vbus_voltage < PD_V_SINK_DISCONNECT_MAX;
	} else {
		if (IS_ENABLED(CONFIG_USB_PD_VBUS_DETECT_TCPC) &&
			(get_usb_pd_vbus_detect() == USB_PD_VBUS_DETECT_TCPC)) {
			return tcpm_check_vbus_level(port, level);
		} else if (level == VBUS_PRESENT) {
			return pd_snk_is_vbus_provided(port);
		} else {
			return !pd_snk_is_vbus_provided(port);
		}
	}
}

