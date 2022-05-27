/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "clock_s3.h"
#include "common.h"
#include "console.h"
#include "registers.h"
#include "scp_watchdog.h"
#include "task.h"

#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ##args)

static void irq_group11_handler(void)
{
	extern volatile int ec_int;
	uint32_t sr_st;

	sr_st = SCP_GIPC_IN_SET;
	if (sr_st & GIPC_IN(S3_IPI_SUSPEND)) {
		CPRINTS("AP suspend");
		task_set_event(TASK_ID_SR, TASK_EVENT_SUSPEND);
		SCP_GIPC_IN_CLR = GIPC_IN(S3_IPI_SUSPEND);
	} else if (sr_st & GIPC_IN(S3_IPI_RESUME)) {
		CPRINTS("AP resume");
		SCP_GIPC_IN_CLR = GIPC_IN(S3_IPI_RESUME);
	}
	asm volatile ("fence.i" ::: "memory");
	task_clear_pending_irq(ec_int);
}
DECLARE_IRQ(11, irq_group11_handler, 0);

void sr_task(void *u)
{
	uint32_t event;

	task_enable_irq(SCP_IRQ_GIPC_IN3);

	while(1) {
		event = task_wait_event(-1);
		if (event & TASK_EVENT_SUSPEND) {
			interrupt_disable();
			watchdog_disable();

			/* alert core1 that core2 is ready */
			SCP_GIPC_IN_SET = GIPC_IN(S3_IPI_READY);

			/* wait core 1 resume */
			while ((SCP_GIPC_IN_SET & GIPC_IN(S3_IPI_RESUME)) == 0);

			watchdog_enable();
			interrupt_enable();
		}
	}
}

void timer_test(void *u)
{
	int d = 0;

	while(1) {
		task_wait_event(10000);

		/* GPIO95 */
		AP_GPIO_MODE11_CLR = 0x70000000;
		AP_GPIO_DIR2_SET = BIT(31);

		if (d % 2 == 0)
			AP_GPIO_DOUT2_SET = BIT(31);
		if (d % 2 == 1)
			AP_GPIO_DOUT2_CLR = BIT(31);
		d++;

	}
}
