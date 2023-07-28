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
#include "tcpm/tcpm.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

/* SM5360A Software workaround switch vchg*/
#define NX20P348X_SWITCH_CONTROL_HIDDEN_REG 0x80
/*SM5360A Software workaround control hidden register enable*/
#define NX20P348X_VCHG_SWITCH_HIDDEN_REG 0x88

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

static int write_reg(uint8_t port, int reg, int regval)
{
	return i2c_write8(ppc_chips[port].i2c_port,
			  ppc_chips[port].i2c_addr_flags, reg, regval);
}

int board_set_ppc_vchg(int port, bool enable)
{
	int rv;
	int en = enable ? 1 : 0;

	if (enable) {
		rv = tcpm_set_src_ctrl(port, en);
		if (rv)
			return rv;
		msleep(1);
		rv = write_reg(1, NX20P348X_SWITCH_CONTROL_REG, 0x00);
		if (rv)
			return rv;
		msleep(1);
		rv = write_reg(1, NX20P348X_VCHG_SWITCH_HIDDEN_REG, 0x00);
		if (rv)
			return rv;
		msleep(1);
		rv = write_reg(1, NX20P348X_SWITCH_CONTROL_HIDDEN_REG, 0x00);
		if (rv)
			return rv;
	} else {
		/*
		 * SM5360A Software workaround solution.
		 * Applied before Main Switch sink mode
		 */
		rv = write_reg(1, NX20P348X_SWITCH_CONTROL_REG, 0x80);
		if (rv)
			return rv;
		msleep(1);
		rv = tcpm_set_src_ctrl(1, 1);
		if (rv)
			return rv;
		msleep(1);
		rv = write_reg(1, NX20P348X_SWITCH_CONTROL_HIDDEN_REG, 0xEA);
		if (rv)
			return rv;
		msleep(1);
		rv = write_reg(1, NX20P348X_SWITCH_CONTROL_HIDDEN_REG, 0xAF);
		if (rv)
			return rv;
		msleep(1);
		rv = write_reg(1, NX20P348X_VCHG_SWITCH_HIDDEN_REG, 0x98);
		if (rv)
			return rv;
	}
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
			if (port == USBC_PORT_C0) {
				rv = board_set_ppc_vchg(USBC_PORT_C1, 1);
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
	if (ppc_is_sourcing_vbus(port)) {
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
		if (port == USBC_PORT_C0) {
			rv = board_set_ppc_vchg(USBC_PORT_C1, 1);
			if (rv)
				CPRINTSUSB("set C%d ppc vchg failed.", i);
		}
	}
	/*
	 * SM5360A Software workaround solution.
	 * Applied before Main Switch sink mode
	 */
	if (port == USBC_PORT_C0) {
		rv = board_set_ppc_vchg(USBC_PORT_C1, 0);
		if (rv)
			CPRINTSUSB("set C%d ppc vchg failed.", i);
	}
	/* Enable requested charge port. */
	if (ppc_vbus_sink_enable(port, 1)) {
		CPRINTSUSB("C%d: sink path enable failed.", port);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}
