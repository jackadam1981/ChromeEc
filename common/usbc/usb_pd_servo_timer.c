/* Copyright 2022 The Chromium OS Authors. All rights reserved.
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

#define MAX_PD_PORTS	CONFIG_USB_PD_PORT_MAX_COUNT
#define MAX_PD_TIMERS	PD_TIMER_COUNT
#define TIMER_ENABLE	0x8000
#define T_LOOP_5MS	(5 * MSEC)

#define TC_T_SRC_RECOVER		(PD_T_SRC_RECOVER / T_LOOP_5MS)
#define TC_T_SAFE_0V			(PD_T_SAFE_0V / T_LOOP_5MS)
#define TC_T_SRC_RECOVER_MAX		(PD_T_SRC_RECOVER_MAX / T_LOOP_5MS)
#define TC_T_CC_DEBOUNCE		(PD_T_CC_DEBOUNCE / T_LOOP_5MS)
#define TC_T_PD_DEBOUNCE		(PD_T_PD_DEBOUNCE / T_LOOP_5MS)
#define TC_T_POWER_SUPPLY_TURN_ON_DELAY	33
#define TC_T_ERROR_RECOVERY		(PD_T_ERROR_RECOVERY / T_LOOP_5MS)
#define TC_T_SRC_TURN_ON		(PD_T_PS_SOURCE_ON / T_LOOP_5MS)
#define TC_T_RP_VALUE_CHANGE		(PD_T_RP_VALUE_CHANGE / T_LOOP_5MS)
#define TC_T_DRP_SNK			(PD_T_DRP_SNK / T_LOOP_5MS)
#define TC_T_DRP_SRC			(PD_T_DRP_SRC / T_LOOP_5MS)
#define TC_T_SRC_DISCONNECT		(PD_T_SRC_DISCONNECT / T_LOOP_5MS)
#define TC_T_DEBOUNCE			(PD_T_DEBOUNCE / T_LOOP_5MS)
#define TC_T_TIMEOUT			0

#define PE_T_PS_SOURCE_OFF		(PD_T_PS_SOURCE_OFF / T_LOOP_5MS)
#define PE_T_NO_RESPONSE		(PD_T_NO_RESPONSE / T_LOOP_5MS)
#define PE_T_SENDER_RESPONSE		(PD_T_SENDER_RESPONSE / T_LOOP_5MS)
#define PE_T_PS_TRANSITION		(PD_T_PS_TRANSITION / T_LOOP_5MS)
#define PE_T_PS_SOURCE_ON		(PD_T_PS_SOURCE_ON / T_LOOP_5MS)
#define PE_T_PS_HARD_RESET		(PD_T_PS_HARD_RESET / T_LOOP_5MS)
#define PE_T_VCONN_SOURCE_ON		(PD_T_VCONN_SOURCE_ON / T_LOOP_5MS)
#define PE_T_SINK_REQUEST		(PD_T_SINK_REQUEST / T_LOOP_5MS)
#define PE_T_SWAP_SOURCE_START		(PD_T_SWAP_SOURCE_START / T_LOOP_5MS)
#define PE_T_RP_VALUE_CHANGE		(PD_T_RP_VALUE_CHANGE / T_LOOP_5MS)
#define PE_T_SRC_DISCONNECT		(PD_T_SRC_DISCONNECT / T_LOOP_5MS)
#define PE_T_SRC_TRANSITION		(PD_T_SRC_TRANSITION / T_LOOP_5MS)
#define PE_T_VCONN_STABLE		(PD_T_VCONN_STABLE / T_LOOP_5MS)
#define PE_T_DISCOVER_IDENTITY		(PD_T_DISCOVER_IDENTITY / T_LOOP_5MS)
#define PE_T_PR_SWAP_WAIT		(PD_T_PR_SWAP_WAIT / T_LOOP_5MS)
#define PE_T_SEND_SOURCE_CAP		(PD_T_SEND_SOURCE_CAP / T_LOOP_5MS)
#define PE_T_SINK_WAIT_CAP		(PD_T_SINK_WAIT_CAP / T_LOOP_5MS)
#define PE_T_TIMEOUT			0

/* VDM Timers ( USB PD Spec Rev2.0 Table 6-30 )*/
#define PE_T_VDM_BUSY			(PD_T_VDM_BUSY / T_LOOP_5MS)
#define PE_T_VDM_E_MODE			(PD_T_VDM_E_MODE / T_LOOP_5MS)
#define PE_T_VDM_RCVR_RSP		(PD_T_VDM_RCVR_RSP / T_LOOP_5MS)
#define PE_T_VDM_SNDR_RSP		(PD_T_VDM_SNDR_RSP / T_LOOP_5MS)
#define PE_T_VDM_WAIT_MODE_E		(PD_T_VDM_WAIT_MODE_E / T_LOOP_5MS)

#define PRL_T_TCPC_TX_TIMEOUT		((100*MSEC) / T_LOOP_5MS)
#define PRL_T_SINK_TX			(PD_T_SINK_TX / T_LOOP_5MS)
#define PRL_T_PS_HARD_RESET		(PD_T_PS_HARD_RESET / T_LOOP_5MS)

static const uint16_t timeouts[] = {
	TC_T_SRC_RECOVER,
	TC_T_SAFE_0V,
	TC_T_SRC_RECOVER_MAX,
	TC_T_CC_DEBOUNCE,
	TC_T_PD_DEBOUNCE,
	TC_T_POWER_SUPPLY_TURN_ON_DELAY,
	TC_T_ERROR_RECOVERY,
	TC_T_SRC_TURN_ON,
	TC_T_RP_VALUE_CHANGE,
	TC_T_DRP_SNK,
	TC_T_DRP_SRC,
	TC_T_SRC_DISCONNECT,
	TC_T_DEBOUNCE,
	TC_T_TIMEOUT,

	PE_T_PS_SOURCE_OFF,
	PE_T_NO_RESPONSE,
	PE_T_SENDER_RESPONSE,
	PE_T_PS_TRANSITION,
	PE_T_PS_SOURCE_ON,
	PE_T_PS_HARD_RESET,
	PE_T_VCONN_SOURCE_ON,
	PE_T_SINK_REQUEST,
	PE_T_SWAP_SOURCE_START,
	PE_T_RP_VALUE_CHANGE,
	PE_T_SRC_DISCONNECT,
	PE_T_SRC_TRANSITION,
	PE_T_VCONN_STABLE,
	PE_T_DISCOVER_IDENTITY,
	PE_T_PR_SWAP_WAIT,
	PE_T_SEND_SOURCE_CAP,
	PE_T_SINK_WAIT_CAP,
	PE_T_TIMEOUT,

	PE_T_VDM_BUSY,
	PE_T_VDM_E_MODE,
	PE_T_VDM_RCVR_RSP,
	PE_T_VDM_SNDR_RSP,
	PE_T_VDM_WAIT_MODE_E,

	PRL_T_TCPC_TX_TIMEOUT,
	PRL_T_SINK_TX,
	PRL_T_PS_HARD_RESET
};

static uint16_t timers[MAX_PD_PORTS][MAX_PD_TIMERS];

void pd_timer_init(int port)
{
	for (int i = 0; i < MAX_PD_TIMERS; i++)
		timers[port][i] = 0;
}

void tc_timer_init(int port)
{
	for (int i = TC_TIMER_SRC_RECOVER; i <= TC_TIMER_TIMEOUT; i++)
		timers[port][i] = 0;
}

void pe_timer_init(int port)
{
	for (int i = PE_TIMER_PS_SOURCE_OFF;
			i <= PE_TIMER_VDM_WAIT_MODE_E; i++)
		timers[port][i] = 0;
}

void pr_timer_init(int port)
{
	for (int i = PR_TIMER_TCPC_TX_TIMEOUT;
			i <= PR_TIMER_HARD_RESET_COMPLETE; i++)
		timers[port][i] = 0;
}

void pd_timer_enable(int port, enum pd_task_timer timer, uint32_t expires_us)
{
	/* expires_us is ignored */

	timers[port][timer] = timeouts[timer] | TIMER_ENABLE;
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

bool pd_timer_is_enabled(int port, enum pd_task_timer timer)
{
	return timers[port][timer] & TIMER_ENABLE;
}

bool pd_timer_is_expired(int port, enum pd_task_timer timer)
{
	return timers[port][timer] == TIMER_ENABLE;
}
