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

#define UCPD_PSC_DIV 1
#define UCPD_HBIT_DIV 27
#define UCPD_TRANSWIN_CNT 8
#define UCPD_IFRGAP_CNT 17

static int ucpd_init(int port)
{
	uint32_t cfgr1_reg;
	uint32_t moder_reg;
	uint32_t cr;

	/* Ensure that clock to UCPD is enabled */
	STM32_RCC_APB1ENR2 |= STM32_RCC_APB1ENR2_UPCD1EN;

	/* Make sure CC1/CC2 pins PB4/PB6 are set for analog mode */
	moder_reg = STM32_GPIO_MODER(GPIO_B);
	moder_reg |= 0x3300;
	STM32_GPIO_MODER(GPIO_B) = moder_reg;
	/*
	 * CFGR1 must be written when UCPD peripheral is disabled. Note that
	 * disabling ucpd causes the peripheral to quit any ongoing activity and
	 * sets all ucpd registers back their default values.
	 */
	STM32_PWR_CR3 &= ~STM32_PWR_CR3_UCPD1_DBDIS;

	cfgr1_reg = STM32_UCPD_CFGR1_PSC_CLK_VAL(UCPD_PSC_DIV - 1) |
		STM32_UCPD_CFGR1_TRANSWIN_VAL(UCPD_TRANSWIN_CNT - 1) |
		STM32_UCPD_CFGR1_IFRGAP_VAL(UCPD_IFRGAP_CNT - 1) |
		STM32_UCPD_CFGR1_HBITCLKD_VAL(UCPD_HBIT_DIV - 1);
	STM32_UCPD_CFGR1(port) = cfgr1_reg;

	/* Enable ucpd  */
	STM32_UCPD_CFGR1(port) |= STM32_UCPD_CFGR1_UCPDEN;

	/* Apply Rd to both CC lines */
	cr = STM32_UCPD_CR(0);
	cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
	STM32_UCPD_CR(0) = cr;

	/*
	* After exiting reset, stm32gx will have dead battery mode enabled by
	* default which connects Rd to CC1/CC2. This should be disabled when EC
	* is powered up.
	*/
	STM32_PWR_CR3 |= STM32_PWR_CR3_UCPD1_DBDIS;

	return EC_SUCCESS;
}

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

	ucpd_init(0);
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
	int cc = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "rd")) {
		pull = TYPEC_CC_RD;
		cc = 1;
	} else if (!strcasecmp(argv[1], "rp")) {
		pull = TYPEC_CC_RP;
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		rp = strtoi(argv[2], &e, 10);
		ccprintf("rp = %d\n", rp);
		cc = 1;
	} else if (!strcasecmp(argv[1], "db")) {
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		rp = strtoi(argv[2], &e, 10);
		if (rp)
			STM32_PWR_CR3 &= ~STM32_PWR_CR3_UCPD1_DBDIS;
		else
			STM32_PWR_CR3 |= STM32_PWR_CR3_UCPD1_DBDIS;
		ccprintf("ucpd DB %s, PWR_CR3 = 0x%x\n",
			 rp ? "enabled" : "disabled",
			STM32_PWR_CR3);
	} else if (!strcasecmp(argv[1], "open")) {
		pull = TYPEC_CC_OPEN;
		cc = 1;
	} else if (!strcasecmp(argv[1], "ppc")) {
		ppc_dump();
		ccprintf("stm32g4: PWR3 = 0x%x, CR = 0x%x\n", STM32_PWR_CR3,
			STM32_UCPD_CR(0));
		   ccprintf("stm32: modera = 0x%08x, moderb = 0x%08x\n",
			    STM32_GPIO_MODER(GPIO_A), STM32_GPIO_MODER(GPIO_B));
		return EC_SUCCESS;
	}

	if (cc)
		baseboard_set_cc(pull, rp);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(usbc, command_usbc,
			"<rd|rp|off|ppc >",
			"MST lane control.");
#endif
