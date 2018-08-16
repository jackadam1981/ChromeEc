/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implement Nuvotion i2c over LPC protocol, as defined in:
 * https://drive.google.com/file/d/0B0DO3Pn_jl5cc2xvbkZaVkpjTDNURmwwZV9XU1BxaVVTdDFB/view
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



int npcx_iol_enable(struct host_cmd_handler_args *args)
{
	task_enable_irq(NPCX_IRQ_SHM);
	/*
	 * Enable SHM interrupt on byte 0 of host command range,
	 * used by semaphore.
	 */
	NPCX_HOFS_CTL |= 1 << NPCX_HOFS1W_IE;
	npcx_iol_msg_state = IOL_IDLE;
	msg_from_host = (struct npcx_iol_msg *)lpc_get_mem_host_cmd_range();
	return 0;
}
DECLARE_HOST_COMMAND(EC_CMD_CROS_TO_NPCX_I2C,
		     npcx_iol_enable, EC_VER_MASK(0));

int npcx_iol_disable(struct host_cmd_handler_args *args)
{
	task_disable_irq(NPCX_IRQ_SHM);
	NPCX_HOFS_CTL &= ~(1 << NPCX_HOFS1W_IE);
	return 0;
}
DECLARE_HOST_COMMAND(EC_CMD_NPCX_I2C_TO_CROS,
		     npcx_iol_disable, EC_VER_MASK(0));

void npcx_iol_shm_irq(void)
{
	task_set_event(TASK_ID_IOLCMD, TASK_EVENT_IOL_PENDING, 0);
}
DECLARE_IRQ(NPCX_IRQ_SHM, npcx_iol_shm_irq, 3);

/* place holder */
void i2c_hid_process(int read, int len, uint8_t *buffer,
		void (*send_response)(int len))
{
	send_response(0);
}



