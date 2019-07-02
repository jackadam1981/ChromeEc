/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "pmic.h"
#include "registers.h"

static int16_t pwrap_scp(uint16_t write, uint16_t adr, uint16_t wdata, uint16_t *rdata)
{
	unsigned int wacs_write = 0;
	unsigned int wacs_adr = 0;
	unsigned int timeout = 0;

	timeout = 0xff;
	do {
		switch (GET_WACS_FSM(PMICW_WACS_RDATA)) {
		case WACS_FSM_WFVLDCLR:
			PMICW_WACS_VLDCLR = 1;
			ccprintf("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			ccprintf("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			ccprintf("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}

		if (GET_WACS_FSM(PMICW_WACS_RDATA) != WACS_FSM_IDLE)
			timeout--;
		else
			break;
	} while (timeout);  /* IDLE State */
	if (timeout == 0)
		return E_PWR_WAIT_IDLE_TIMEOUT;

	/* Argument process */
	wacs_write  = write << 31;
	wacs_adr    = (adr >> 1) << 16;
	PMICW_WACS_CMD = wacs_write | wacs_adr | wdata;

	if(write == 0){
		timeout = 0xff;
		do {
			if (GET_WACS_SYNC_IDLE(PMICW_WACS_RDATA) != WACS_SYNC_IDLE)
				timeout--;
			else
				break;
		} while (timeout); /* IDLE State */
		if (timeout == 0)
			return E_PWR_WAIT_IDLE_TIMEOUT_READ;

		*rdata = PMICW_WACS_RDATA & RDATA_WACS_RDATA_MASK;
		PMICW_WACS_VLDCLR = 1;
	}
	return 0;
}

static int16_t pmic_scp_read(uint16_t addr, uint16_t *rdata)
{
	return pwrap_scp(0, addr, 0, rdata);
}

static int16_t pmic_scp_write(uint16_t addr, uint16_t wdata)
{
	return pwrap_scp(1, addr, wdata, 0);
}

static uint16_t pmic_read_field(uint16_t reg, uint16_t mask, uint16_t shift)
{
	uint16_t rdata;
	pmic_scp_read(reg, &rdata);
	rdata &= (mask << shift);
	rdata = (rdata >> shift);
	return rdata;
}

static void pmic_write_field(uint16_t reg, uint16_t val, uint16_t mask, uint16_t shift)
{
	uint16_t old, new;
	pmic_scp_read(reg, &old);
	new = old & ~(mask << shift);
	new |= (val << shift);
	pmic_scp_write(reg, new);
}

void scp_pmic_init(void)
{
	ccprintf("[%s] CHIP(0x%x)\n", __func__, pmic_read_field(PMIC_CHIP_ID, 0xffff, 0));

	/* Set GPIO 154 to function mode 1, SCP_VREQ */
	AP_GPIO_MODE(19) = (AP_GPIO_MODE(19) & ~GPIO_154_MODESET_MASK) | GPIO_154_SCP_VREQ;

	/* scp voltage initialization */
	/* 0x14A6[6:0], RG_BUCK_VCORE_SSHUB_VOSEL */
	pmic_write_field(0x14A6, 0x20, 0x7f, 0);
	/* 0x14A6[14:8], RG_BUCK_VCORE_SSHUB_VOSEL_SLEEP */
	pmic_write_field(0x14A6, 0x20, 0x7f, 8);
	/* 0x14a4[0], RG_BUCK_VCORE_SSHUB_EN */
	pmic_write_field(0x14a4, 0x1, 0x1, 0);
	/* 0x14a4[1], RG_BUCK_VCORE_SSHUB_SLEEP_VOSEL_EN */
	pmic_write_field(0x14a4, 0, 0x1, 1);

	/* 0x1bc6[6:0], RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL */
	pmic_write_field(0x1bc6, 0x40, 0x7f, 0);
	/* 0x1bc6[14:8], RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_SLEEP */
	pmic_write_field(0x1bc6, 0x40, 0x7f, 8);
	/* 0x1bc4[0], RG_LDO_VSRAM_OTHERS_SSHUB_EN */
	pmic_write_field(0x1bc4, 0x1, 0x1, 0);
	/* 0x1bc4[1], RG_LDO_VSRAM_OTHERS_SSHUB_SLEEP_VOSEL_EN */
	pmic_write_field(0x1bc4, 0, 0x1, 1);

	/* 0x134[4], RG_SRCVOLTEN_LP_EN */
	pmic_write_field(0x134, 0x1, 0x1, 4);

}
