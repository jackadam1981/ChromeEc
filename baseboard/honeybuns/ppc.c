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
#include "sn5s330.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd_tcpm.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"
#include "registers.h"
#include "ucpd-stm32gx.h"


static int read_reg(uint8_t port, int reg, int *regval)
{
	return i2c_read8(ppc_chips[port].i2c_port,
			 ppc_chips[port].i2c_addr_flags,
			 reg,
			 regval);
}

static int write_reg(uint8_t port, int reg, int regval)
{
	return i2c_write8(ppc_chips[port].i2c_port,
			  ppc_chips[port].i2c_addr_flags,
			  reg,
			  regval);
}

int baseboard_ppc_init(int port)
{
	int regval;
	int status;
	int retries;

	/*
	 * It seems that sometimes setting the FUNC_SET1 register fails
	 * initially.  Therefore, we'll retry a couple of times.
	 */
	retries = 0;
	do {
		status = write_reg(port, SN5S330_FUNC_SET1, SN5S330_ILIM_3_06);
		if (status) {
			retries++;
			msleep(1);
		} else {
			break;
		}
	} while (retries < 10);

	/* Turn off dead battery resistors, turn on CC FETs */
	status = read_reg(port, SN5S330_FUNC_SET4, &regval);
	if (!status) {
		regval |= SN5S330_CC_EN;
		status = write_reg(port, SN5S330_FUNC_SET4, regval);
	}
	if (status) {
		return status;
	}

	/* Enable sink path via PP2 */
	status = read_reg(port, SN5S330_FUNC_SET3, &regval);
	if (!status) {
		regval &= ~SN5S330_PP1_EN;
		regval |= SN5S330_PP2_EN;
		status = write_reg(port, SN5S330_FUNC_SET3, regval);
	}
	if (status) {
		return status;
	}

	return EC_SUCCESS;
}

#if 1
#define UCPD_ANASUB_TO_RP(r) ((r - 1) & 0x3)
#define UCPD_RP_TO_ANASUB(r) ((r + 1) & 0x3)

static int baseboard_set_cc(int cc_pull, int rp)
{
	uint32_t cr = STM32_UCPD_CR(0);

	/*
	 * Always set ANASUBMODE to match desired Rp. TCPM layer has a valid
	 * range of 0, 1, or 2. This range maps to 1, 2, or 3 in ucpd for
	 * ANASUBMODE.
	 */
	cr &= ~STM32_UCPD_CR_ANASUBMODE_MASK;
	cr |= STM32_UCPD_CR_ANASUBMODE_VAL(UCPD_RP_TO_ANASUB(rp));

	/* Disconnect both pull from both CC lines by default */
	cr &= ~STM32_UCPD_CR_CCENABLE_MASK;
	/* Set ANAMODE if cc_pull is Rd */
	if (cc_pull == TYPEC_CC_RD) {
		cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
	/* Clear ANAMODE if cc_pull is Rp */
	} else if (cc_pull == TYPEC_CC_RP) {
		cr &= ~(STM32_UCPD_CR_ANAMODE);
		cr |= STM32_UCPD_CR_CCENABLE_MASK;
	}

	/* Update pull values */
	STM32_UCPD_CR(0) = cr;

	ccprintf("ucpd: cc_pull = %d, rp = %d, CR = 0x%x\n",
		 cc_pull, rp, STM32_UCPD_CR(0));

	return EC_SUCCESS;
}

static void ppc_dump(void)
{
	int offset;
	int rv;
	int regval;

	read_reg(0, SN5S330_FUNC_SET1, &regval);
	for (offset = SN5S330_FUNC_SET1; offset <= SN5S330_FUNC_SET12;
	     offset++) {
		rv = read_reg(0, offset, &regval);
		if (!rv)
			ccprintf("[0x%02x]: 0x%02x\n", offset, regval);
	}

	for (offset = SN5S330_INT_STATUS_REG1; offset <= SN5S330_INT_STATUS_REG4;
	     offset++) {
		rv = read_reg(0, offset, &regval);
		if (!rv)
			ccprintf("[%02x]: %02x\n", offset, regval);
	}
}

static int command_usbc(int argc, char **argv)
{
	char *e;
	int rp = 0;
	int pull = -1;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "rd")) {
		pull = TYPEC_CC_RD;
	} else if (!strcasecmp(argv[1], "rp")) {
		pull = TYPEC_CC_RP;
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		rp = strtoi(argv[2], &e, 10);
		ccprintf("rp = %d\n", rp);
	} else if (!strcasecmp(argv[1], "open")) {
		pull = TYPEC_CC_OPEN;
	} else if (!strcasecmp(argv[1], "ppc")) {
		ppc_dump();
		return EC_SUCCESS;
	}

	baseboard_set_cc(pull, rp);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(usbc, command_usbc,
			"<rd|rp|off|ppc >",
			"MST lane control.");
#endif
