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

static int set_flags(const int port, const int addr, const int flags_to_set)
{
	int val, rv;

	rv = read_reg(port, addr, &val);
	if (rv)
		return rv;

	val |= flags_to_set;

	return write_reg(port, addr, val);
}


static int clr_flags(const int port, const int addr, const int flags_to_clear)
{
	int val, rv;

	rv = read_reg(port, addr, &val);
	if (rv)
		return rv;

	val &= ~flags_to_clear;

	return write_reg(port, addr, val);
}


static int baseboard_ppc_pp_fet_enable(uint8_t port, enum sn5s330_pp_idx pp,
				 int enable)
{
	int status;
	int pp_bit;

	if (pp == SN5S330_PP1)
		pp_bit = SN5S330_PP1_EN;
	else if (pp == SN5S330_PP2)
		pp_bit = SN5S330_PP2_EN;
	else
		return EC_ERROR_INVAL;

	status = enable ? set_flags(port, SN5S330_FUNC_SET3, pp_bit)
			: clr_flags(port, SN5S330_FUNC_SET3, pp_bit);

	if (status) {
		return status;
	}

	return EC_SUCCESS;
}

int baseboard_ppc_init(int port)
{
	int regval;
	int status;
	int retries;
	const int i2c_port  = ppc_chips[port].i2c_port;
	const uint16_t i2c_addr_flags = ppc_chips[port].i2c_addr_flags;


	/* Default SRC current limit to ~3.0A. */
	regval = SN5S330_ILIM_3_06;

	/*
	 * It seems that sometimes setting the FUNC_SET1 register fails
	 * initially.  Therefore, we'll retry a couple of times.
	 */
	retries = 0;
	do {
		status = i2c_write8(i2c_port, i2c_addr_flags,
				    SN5S330_FUNC_SET1, regval);
		if (status) {
			retries++;
			msleep(1);
		} else {
			break;
		}
	} while (retries < 10);

	/* Set Vbus OVP threshold to ~22.325V. */
	regval = 0x37;
	status = i2c_write8(i2c_port, i2c_addr_flags,
			    SN5S330_FUNC_SET5, regval);
	if (status) {
		ccprintf("ppc: failed to write func5\n");
		return status;
	}

	/* Set Vbus UVP threshold to ~2.75V. */
	status = i2c_read8(i2c_port, i2c_addr_flags,
			   SN5S330_FUNC_SET6, &regval);
	if (status) {
		ccprintf("ppc: failed to read func6\n");
		return status;
	}
	regval &= ~0x3F;
	regval |= 1;
	status = i2c_write8(i2c_port, i2c_addr_flags,
			    SN5S330_FUNC_SET6, regval);
	if (status) {
		ccprintf("ppc: failed to write func6\n");
		return status;
	}

	/* Enable SBU Fets and set PP2 current limit to ~3A. */
	regval = SN5S330_SBU_EN | 0x8;
	status = i2c_write8(i2c_port, i2c_addr_flags,
			    SN5S330_FUNC_SET2, regval);
	if (status) {
		ccprintf("ppc: failed to write func_2\n");
		return status;
	}

	/*
	 * Indicate we are using PP2 configuration 2 and enable OVP comparator
	 * for CC lines.
	 *
	 * Also, turn off under-voltage protection for incoming Vbus as it would
	 * prevent us from enabling SNK path before we hibernate the ec. We
	 * need to enable the SNK path so USB power will assert ACOK and wake
	 * the EC up went inserting USB power. We always turn off under-voltage
	 * protection because the battery charger will boost the voltage up
	 * to the needed battery voltage either way (and it will have its own
	 * low voltage protection).
	 */
	regval = SN5S330_OVP_EN_CC | SN5S330_PP2_CONFIG | SN5S330_CONFIG_UVP;
	status = i2c_write8(i2c_port, i2c_addr_flags,
			    SN5S330_FUNC_SET9, regval);

	/*
	 * Turn off dead battery resistors, turn on CC FETs, and set the higher
	 * of the two VCONN current limits (min 0.6A).  Many VCONN accessories
	 * trip the default current limit of min 0.35A.
	 */
	status = set_flags(port, SN5S330_FUNC_SET4,
			   SN5S330_CC_EN | SN5S330_VCONN_ILIM_SEL);
	if (status) {
		return status;
	}

	/* Set ideal diode mode for both PP1 and PP2. */
	status = set_flags(port, SN5S330_FUNC_SET3,
			   SN5S330_SET_RCP_MODE_PP1 | SN5S330_SET_RCP_MODE_PP2);
	if (status) {
		ccprintf("ppc: failed to write func_3\n");
		return status;
	}

	/* Turn off PP1 FET. */
	status = baseboard_ppc_pp_fet_enable(port, SN5S330_PP1, 0);

	/*
	 * Clear the digital reset bit, and mask off and clear vSafe0V
	 * interrupts. Leave the dead battery mode bit unchanged since it
	 * is checked below.
	 */
	regval = SN5S330_DIG_RES | SN5S330_VSAFE0V_MASK;
	status |= i2c_write8(i2c_port, i2c_addr_flags,
			    SN5S330_INT_STATUS_REG4, regval);

	/* Turn on PP2 FET. */
	status = baseboard_ppc_pp_fet_enable(port, SN5S330_PP2, 1);
	ccprintf("ppc: initialization complete\n");
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
		rp = strtoi(argv[1], &e, 10);
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
