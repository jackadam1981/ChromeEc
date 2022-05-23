/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#include <assert.h>
#include <string.h>

#include "clock.h"
#include "console.h"
#include "ec_commands.h"
#include "power.h"
#include "registers.h"
#include "scp_timer.h"
#include "scp_watchdog.h"
#include "task.h"

#define CPRINTS(format, args...) cprints(CC_CLOCK, format, ##args)

#define TASK_EVENT_SUSPEND TASK_EVENT_CUSTOM_BIT(4)
#define TASK_EVENT_RESUME TASK_EVENT_CUSTOM_BIT(5)
#define CHECK_26M_PERIOD_US 50000

enum scp_sr_state {
	SR_S0,
	SR_S02S3,
	SR_S3,
};

void clock_init(void) {}

__override void
power_chipset_handle_host_sleep_event(enum host_sleep_event state,
				      struct host_sleep_event_context *ctx)
{
	if (state == HOST_SLEEP_EVENT_S3_SUSPEND) {
		CPRINTS("AP suspend");
		task_set_event(TASK_ID_SR, TASK_EVENT_SUSPEND);
	} else if (state == HOST_SLEEP_EVENT_S3_RESUME) {
		task_set_event(TASK_ID_SR, TASK_EVENT_RESUME);
		CPRINTS("AP resume");
	}
}

void sr_task(void *u)
{
	enum scp_sr_state state = SR_S0;
	uint32_t event;
	uint32_t prev, now;

	while(1) {
		switch (state) {
		case SR_S0:
			event = task_wait_event(-1);
			if (event & TASK_EVENT_SUSPEND) {
				timer_enable(TIMER_SR);
				prev = timer_read_raw_sr();
				state = SR_S02S3;
			}
			break;
		case SR_S02S3:
			event = task_wait_event(CHECK_26M_PERIOD_US);
			if (event & TASK_EVENT_RESUME) {
				/* suspend is aborted */
				timer_disable(TIMER_SR);
				state = SR_S0;
			} else if (event & TASK_EVENT_TIMER) {
				now = timer_read_raw_sr();
				if (now != prev) {
					/* 26M is still on */
					prev = now;
				} else {
					/* 26M is off */
					state = SR_S3;
				}
			}
			break;
		case SR_S3:
			interrupt_disable();
			/* alert core1 that core2 is ready by disabling watchdog */
			watchdog_disable();

			/* wait core 1 resume */
			while (SCP_CORE0_TIMER_EN(TIMER_SR) & TIMER_EN);

			watchdog_enable();
			interrupt_enable();
			timer_disable(TIMER_SR);
			state = SR_S0;
			break;
		}
	}
}
