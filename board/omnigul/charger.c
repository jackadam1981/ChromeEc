/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "charge_manager.h"
#include "charge_state.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/charger/isl9241.h"
#include "driver/ppc/nx20p348x.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

/* SM5360A Software workaround switch vchg*/
#define SM5360A_SWITCH_CONTROL_HIDDEN_REG 0x80
/* SM5360A Software workaround control hidden register enable */
#define SM5360A_VCHG_SWITCH_HIDDEN_REG 0x88
#define DELAY_1_MS 1

#ifndef CONFIG_ZEPHYR
/* Charger Chip Configuration */
const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_CHARGER,
		.i2c_addr_flags = ISL9241_ADDR_FLAGS,
		.drv = &isl9241_drv,
	},
};
BUILD_ASSERT(ARRAY_SIZE(chg_chips) == CHARGER_NUM);
#endif

enum ppc_mode {
	DEAD_BATTERY_MODE,
	SINK_MODE,
	OTG_MODE,
	SOURCE_MODE,
	STANDBY_MODE,
	PPC_MODE_COUNT,
};

int software_workaround_flag;

static int read_reg(uint8_t port, int reg, int *regval)
{
	return i2c_read8(ppc_chips[port].i2c_port,
			 ppc_chips[port].i2c_addr_flags, reg, regval);
}

static int write_reg(uint8_t port, int reg, int regval, int time_ms)
{
	int rv;

	rv = i2c_write8(ppc_chips[port].i2c_port,
			ppc_chips[port].i2c_addr_flags, reg, regval);
	msleep(time_ms);
	return rv;
}

int board_set_ppc_vchg(int port, bool enable)
{
	int rv;
	int mode;

	rv = read_reg(USBC_PORT_C1, NX20P348X_DEVICE_STATUS_REG, &mode);
	if (rv) {
		CPRINTSUSB("read NX20P348X REG fail");
		return rv;
	}
	if (enable) {
		CPRINTSUSB("start set ppc vchg enable, mode = %d", mode);

		switch (mode) {
		case SOURCE_MODE:
			__fallthrough;
		case STANDBY_MODE:
			rv = tcpm_set_src_ctrl(USBC_PORT_C1, 0);
			CPRINTSUSB("set SRC_EN = 'L' in stadby mode");
			msleep(1);
			if (rv) {
				CPRINTSUSB("set SRC_EN = 'L' fail");
				return rv;
			}
			rv = write_reg(USBC_PORT_C1,
				       NX20P348X_SWITCH_CONTROL_REG, 0x00,
				       DELAY_1_MS);
			CPRINTSUSB("set 0x02 to 0x00 in stadby mode");
			if (rv) {
				CPRINTSUSB("set NX20P348X switch fail");
				return rv;
			}
			__fallthrough;
		case OTG_MODE:

			rv = write_reg(USBC_PORT_C1,
				       SM5360A_VCHG_SWITCH_HIDDEN_REG, 0x00,
				       DELAY_1_MS);
			if (rv) {
				CPRINTSUSB("set SM5360A switch fail");
				return rv;
			}
			rv = write_reg(USBC_PORT_C1,
				       SM5360A_SWITCH_CONTROL_HIDDEN_REG, 0x00,
				       DELAY_1_MS);
			if (rv) {
				CPRINTSUSB("set SM5360A switch fail");
				return rv;
			}
			software_workaround_flag = 0;
			break;
		default:
			break;
		}
	} else {
		/*
		 * SM5360A Software workaround solution.
		 * Applied before Main Switch sink mode
		 */
		CPRINTSUSB("start set ppc vchg disable, mode = %d", mode);

		switch (mode) {
		case STANDBY_MODE:
			__fallthrough;
		case SOURCE_MODE:
			rv = write_reg(USBC_PORT_C1,
				       NX20P348X_SWITCH_CONTROL_REG, 0x80,
				       DELAY_1_MS);
			CPRINTSUSB(
				"set 0x02 to 0x80(source mode) in stadby mode");
			if (rv) {
				CPRINTSUSB("set NX20P348X switch fail");
				return rv;
			}
			rv = tcpm_set_src_ctrl(USBC_PORT_C1, 1);
			CPRINTSUSB("set SRC_EN = 'H' in stadby mode");
			msleep(1);
			if (rv) {
				CPRINTSUSB("set SRC_EN = 'H' fail");
				return rv;
			}
			__fallthrough;
		case OTG_MODE:
			rv = write_reg(USBC_PORT_C1,
				       SM5360A_SWITCH_CONTROL_HIDDEN_REG, 0xEA,
				       DELAY_1_MS);
			if (rv) {
				CPRINTSUSB(
					"set SM5360A control hidden reg 0xEA fail");
				return rv;
			}
			rv = write_reg(USBC_PORT_C1,
				       SM5360A_SWITCH_CONTROL_HIDDEN_REG, 0xAF,
				       DELAY_1_MS);
			if (rv) {
				CPRINTSUSB(
					"set SM5360A control hidden reg 0xAF fail");
				return rv;
			}
			rv = write_reg(USBC_PORT_C1,
				       SM5360A_VCHG_SWITCH_HIDDEN_REG, 0x98,
				       DELAY_1_MS);
			if (rv) {
				CPRINTSUSB(
					"set SM5360A VCHG hidden reg 0x98 fail");
				return rv;
			}
			software_workaround_flag = 1;
			break;
		default:
			break;
		}
	}
	return EC_SUCCESS;
}

int board_set_active_charge_port(int port)
{
	int is_valid_port = board_is_usb_pd_port_present(port);
	int i;
	int rv;

	if (port == CHARGE_PORT_NONE) {
		CPRINTSUSB("Disabling all charger ports");

		/* Disable all ports. */
		for (i = 0; i < ppc_cnt; i++) {
			/*
			 * Do not return early if one fails otherwise we can
			 * get into a boot loop assertion failure.
			 */

			if (ppc_vbus_sink_enable(i, 0))
				CPRINTSUSB("Disabling C%d as sink failed.", i);
			if (i == USBC_PORT_C0) {
				rv = board_set_ppc_vchg(USBC_PORT_C1, true);
				if (rv)
					CPRINTSUSB("set C%d ppc vchg failed.",
						   i);
			}
		}

		return EC_SUCCESS;
	} else if (!is_valid_port) {
		return EC_ERROR_INVAL;
	}

	/* Check if the port is sourcing VBUS. */
	if ((ppc_is_sourcing_vbus(port)) &&
	    !((port == USBC_PORT_C1) && (software_workaround_flag))) {
		CPRINTFUSB("Skip enable C%d", port);
		return EC_ERROR_INVAL;
	}

	CPRINTSUSB("New charge port: C%d", port);

	/*
	 * Turn off the other ports' sink path FETs, before enabling the
	 * requested charge port.
	 */
	for (i = 0; i < ppc_cnt; i++) {
		if (i == port)
			continue;

		if (ppc_vbus_sink_enable(i, 0))
			CPRINTSUSB("C%d: sink path disable failed.", i);
		if (i == USBC_PORT_C0) {
			rv = board_set_ppc_vchg(USBC_PORT_C1, true);
			if (rv) {
				CPRINTSUSB("set C%d ppc vchg failed.", i);
				return rv;
			}
		}
	}
	/*
	 * SM5360A Software workaround solution.
	 * Applied before Main Switch sink mode
	 */
	if (port == USBC_PORT_C0) {
		rv = board_set_ppc_vchg(USBC_PORT_C1, false);
		if (rv) {
			CPRINTSUSB("set C%d ppc vchg failed.", i);
			return rv;
		}
	}
	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTSUSB("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

__override void pd_power_supply_reset(int port)
{
	int prev_en;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS. */
	if ((port == 1) && (software_workaround_flag))
		CPRINTSUSB("PPC vbus didn't sourcing");
	else
		ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

	if (port == USBC_PORT_C1 && software_workaround_flag) {
		write_reg(USBC_PORT_C1, NX20P348X_SWITCH_CONTROL_REG, 0x80,
			  DELAY_1_MS);
	}
	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

__override int pd_set_power_supply_ready(int port)
{
	int rv;

	/* Disable charging. */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv)
		return rv;

	pd_set_vbus_discharge(port, 0);

	/* Provide Vbus. */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;
	if (port == USBC_PORT_C1 && software_workaround_flag) {
		rv = write_reg(USBC_PORT_C1, NX20P348X_SWITCH_CONTROL_REG, 0x00,
			       DELAY_1_MS);
		if (rv) {
			return rv;
		}
	}
	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}
