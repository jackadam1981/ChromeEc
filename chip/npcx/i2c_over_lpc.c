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
