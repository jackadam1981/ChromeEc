/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implement Nuvotion i2c over LPC protocol, as defined in:
 * http://go/nuvoton-ec-i2c
 */

#include "clock.h"
#include "clock_chip.h"
#include "common.h"
#include "host_command.h"
#include "i2c_over_lpc.h"
#include "lpc.h"
#include "registers.h"
#include "task.h"
#include "system.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

int npcx_iol_enable(struct host_cmd_handler_args *args)
{
	task_enable_irq(NPCX_IRQ_SHM);
	npcx_iol_msg_state = IOL_IDLE;
	msg_from_host = (struct npcx_iol_msg *)lpc_get_mem_host_cmd_range();
	/* Reset Semaphore. */
	NPCX_IOL_SEM = 0;

	/*
	 * Enable SHM interrupt on byte 0 of host command range,
	 * used by semaphore.
	 */
	SET_BIT(NPCX_SMC_STS, NPCX_SMC_STS_HSEM1W);
	SET_BIT(NPCX_SMC_CTL, NPCX_SMC_CTL_HSEM1_IE);
	CLEAR_BIT(NPCX_SHCFG, NPCX_SHCFG_SEMW1_DIS);
	CPRINTS("npcx_iol_enable called.");
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CROS_TO_ALTERNATE,
		     npcx_iol_enable, EC_VER_MASK(0));

int npcx_iol_disable(struct host_cmd_handler_args *args)
{
	task_disable_irq(NPCX_IRQ_SHM);
	CLEAR_BIT(NPCX_SMC_CTL, NPCX_SMC_CTL_HSEM1_IE);
	SET_BIT(NPCX_SHCFG, NPCX_SHCFG_SEMW1_DIS);
	CPRINTS("npcx_iol_disable called.");
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_ALTERNATE_TO_CROS,
		     npcx_iol_disable, EC_VER_MASK(0));

void npcx_iol_shm_irq(void)
{
	SET_BIT(NPCX_SMC_STS, NPCX_SMC_STS_HSEM1W);
	task_set_event(TASK_ID_IOLCMD, TASK_EVENT_IOL_PENDING, 0);
}
DECLARE_IRQ(NPCX_IRQ_SHM, npcx_iol_shm_irq, 3);

int npcx_iol_host_int(int semaphore)
{
	/*
	 * Trigger a host interrupt by writing the semaphore (at least the
	 * 4 MSb) into the SHM semaphore register.
	 */
	NPCX_IOL_SEM = semaphore;
	return 0;
}
