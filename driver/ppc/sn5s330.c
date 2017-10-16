/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TI SN5S330 USB-C Power Path Controller */

/*
 * PP1 : Sourcing power path.
 * PP2 : Sinking power path.
 */

#include "common.h"
#include "console.h"
#include "driver/sn5s330.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define SN5S330_DEBUG 0

#if SN5S330_DEBUG
static int command_sn5s330_dump(int argc, char **argv)
{
	int i;
	int data;

	for (i = SN5S330_FUNC_SET1; i <= SN5S330_FUNC_SET12; i++) {
		i2c_read8(I2C_PORT_TCPC0, SN5S330_ADDR0, i, &data);
		ccprintf("FUNC_SET%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_FUNC_SET1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_STATUS_REG1; i <= SN5S330_INT_STATUS_REG4; i++) {
		i2c_read8(I2C_PORT_TCPC0, SN5S330_ADDR0, i, &data);
		ccprintf("INT_STATUS_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_STATUS_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_TRIP_RISE_REG1; i <= SN5S330_INT_TRIP_RISE_REG3;
	     i++) {
		i2c_read8(I2C_PORT_TCPC0, SN5S330_ADDR0, i, &data);
		ccprintf("INT_TRIP_RISE_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_TRIP_RISE_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_TRIP_FALL_REG1; i <= SN5S330_INT_TRIP_FALL_REG3;
	     i++) {
		i2c_read8(I2C_PORT_TCPC0, SN5S330_ADDR0, i, &data);
		ccprintf("INT_TRIP_FALL_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_TRIP_FALL_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_MASK_RISE_REG1; i <= SN5S330_INT_MASK_RISE_REG3;
	     i++) {
		i2c_read8(I2C_PORT_TCPC0, SN5S330_ADDR0, i, &data);
		ccprintf("INT_MASK_RISE_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_MASK_RISE_REG1 + 1,
			 i,
			 data);
	}

	for (i = SN5S330_INT_MASK_FALL_REG1; i <= SN5S330_INT_MASK_FALL_REG3;
	     i++) {
		i2c_read8(I2C_PORT_TCPC0, SN5S330_ADDR0, i, &data);
		ccprintf("INT_MASK_FALL_REG%d [%02Xh] = 0x%02x\n",
			 i - SN5S330_INT_MASK_FALL_REG1 + 1,
			 i,
			 data);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sn5s330_dump, command_sn5s330_dump,
			"", "dump the regs");
#endif /* SN5S330_DEBUG */

int sn5s330_pp2_fet_enable(int port, int addr)
{
	int regval;
	int status;

	status = i2c_read8(port, addr, SN5S330_FUNC_SET3, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET3!");
		return status;
	}
	regval |= SN5S330_PP2_EN;

	status = i2c_write8(port, addr, SN5S330_FUNC_SET3, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET3!");
		return status;
	}

	return EC_SUCCESS;
}

static int init_sn5s330(int i2c_port, int i2c_addr)
{
	int regval;
	int status;
	int retries;

	/* Set the sourcing current limit value. */
#if defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) &&			\
	(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT == TYPEC_RP_3A0)
	/* Set current limit to ~3A. */
	regval = SN5S330_ILIM_3_06;
#else
	/* Set current limit to ~1.5A. */
	regval = SN5S330_ILIM_1_62;
#endif

	/*
	 * It seems that sometimes setting the FUNC_SET1 register fails
	 * initially.  Therefore, we'll retry a couple of times.
	 */
	retries = 0;
	do {
		status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET1,
				    regval);
		if (status) {
			CPRINTS("Failed to set FUNC_SET1! Retrying...");
			retries++;
			msleep(1);
		} else {
			break;
		}
	} while (retries < 10);

	/* Set Vbus OVP threshold to ~22.325V. */
	regval = 0x37;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET5, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET5!");
		return status;
	}

	/* Set Vbus UVP threshold to ~2.75V. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_FUNC_SET6, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET6!");
		return status;
	}
	regval &= ~0x3F;
	regval |= 1;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET6, regval);
	if (status) {
		CPRINTS("Failed to write FUNC_SET6!");
		return status;
	}

	/* Enable SBU Fets and set PP2 current limit to ~3A. */
	regval = SN5S330_SBU_EN | 0xf;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET2, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET2!");
		return status;
	}

	/* TODO(aaboagye): What about Vconn */

	/*
	 * Indicate we are using PP2 configuration 2 and enable OVP comparator
	 * for CC lines.
	 */
	regval = SN5S330_OVP_EN_CC | SN5S330_PP2_CONFIG;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET9, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET9!");
		return status;
	}

	/* Set analog current limit delay to 200 us for both PP1 & PP2. */
	regval = (PPX_ILIM_DEGLITCH_0_US_200 << 3) | PPX_ILIM_DEGLITCH_0_US_200;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET11,
			    regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET11");
		return status;
	}

	/* Turn off dead battery resistors and turn on CC FETs. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_FUNC_SET4, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET4!");
		return status;
	}
	regval |= SN5S330_CC_EN;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET4, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET4!");
		return status;
	}

	/* Turn on PP1/2 FETs and set ideal diode mode for both PP1 and PP2. */
	status = i2c_read8(i2c_port, i2c_addr, SN5S330_FUNC_SET3, &regval);
	if (status) {
		CPRINTS("Failed to read FUNC_SET3!");
		return status;
	}
	regval |= SN5S330_SET_RCP_MODE_PP1 | SN5S330_SET_RCP_MODE_PP2 |
		SN5S330_PP2_EN | SN5S330_PP1_EN;
	status = i2c_write8(i2c_port, i2c_addr, SN5S330_FUNC_SET3, regval);
	if (status) {
		CPRINTS("Failed to set FUNC_SET3!");
		return status;
	}

	return EC_SUCCESS;
}

static void sn5s330_init(void)
{
	int i;
	int rv;

	for (i = 0; i < sn5s330_cnt; i++) {
		rv = init_sn5s330(sn5s330_tbl[i].i2c_port,
				  sn5s330_tbl[i].i2c_addr);
		if (!rv)
			CPRINTS("C%d: SN5S330 initialized.", i);
		else
			CPRINTS("C%d: SN5S330 init failed! (%d)", i, rv);
	}
}
DECLARE_HOOK(HOOK_INIT, sn5s330_init, HOOK_PRIO_LAST);
