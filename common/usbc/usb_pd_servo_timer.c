/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "assert.h"
#include "atomic.h"
#include "common.h"
#include "console.h"
#include "limits.h"
#include "math_util.h"
#include "system.h"
#include "usb_pd_timer.h"
#include "usb_tc_sm.h"

#define MAX_PD_PORTS			CONFIG_USB_PD_PORT_MAX_COUNT
#define MAX_PD_TIMERS			PD_TIMER_COUNT
#define TIMER_ENABLE			0x8000
#define T_LOOP_5MS			(5 * MSEC)

static uint16_t timers[MAX_PD_PORTS][MAX_PD_TIMERS];

void pd_timer_init(int port)
{
	for (int i = 0; i < MAX_PD_TIMERS; i++)
		timers[port][i] = 0;
}

void tc_timer_init(int port)
{
	for (int i = TC_TIMER_CC_DEBOUNCE; i <= TC_TIMER_VBUS_DEBOUNCE; i++)
		timers[port][i] = 0;
}

void pe_timer_init(int port)
{
	for (int i = 0;
			i <= PE_TIMER_WAIT_AND_ADD_JITTER; i++)
		timers[port][i] = 0;
}

void pr_timer_init(int port)
{
	for (int i = PR_TIMER_CHUNK_SENDER_RESPONSE;
			i <= PR_TIMER_TCPC_TX_TIMEOUT; i++)
		timers[port][i] = 0;
}

void pd_timer_enable(int port, enum pd_task_timer timer, uint32_t expires_us)
{
	timers[port][timer] = (expires_us / T_LOOP_5MS) | TIMER_ENABLE;
}

void pd_timer_disable(int port, enum pd_task_timer timer)
{
	timers[port][timer] = 0;
}

void pd_timer_update(int port)
{
	for (int i = 0; i < MAX_PD_TIMERS; i++) {
		if (timers[port][i] > TIMER_ENABLE)
			timers[port][i]--;
	}
}

bool pd_timer_is_disabled(int port, enum pd_task_timer timer)
{
	return !(timers[port][timer] & TIMER_ENABLE);
}

bool pd_timer_is_expired(int port, enum pd_task_timer timer)
{
	return timers[port][timer] == TIMER_ENABLE;
}
