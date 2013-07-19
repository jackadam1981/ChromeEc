/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Mock X86 chipset power control module for Chrome EC */

#include "chipset.h"
#include "chipset_x86_common.h"
#include "common.h"
#include "console.h"
#include "lpc.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

static enum x86_state state = X86_G3;  /* Current state */


void x86_set_state(enum x86_state new_state)
{
	state = new_state;
}

void chipset_force_shutdown(void)
{
	uart_puts("Force shutdown\n");
	x86_set_state(X86_G3);
}


test_mockable void chipset_reset(int cold_reset)
{
	uart_printf("X86 Power %s reset\n", cold_reset ? "cold" : "warm");
}


void chipset_throttle_cpu(int throttle)
{
	/* Print transitions */
	static int last_val;
	if (throttle != last_val) {
		if (throttle)
			uart_printf("Throttle CPU.\n");
		else
			uart_printf("No longer throttle CPU.\n");
		last_val = throttle;
	}
}


void chipset_exit_hard_off(void)
{
	x86_set_state(X86_S0);
	return;
}


int chipset_in_state(int state_mask)
{
	int need_mask = 0;

	/*
	 * TODO: what to do about state transitions?  If the caller wants
	 * HARD_OFF|SOFT_OFF and we're in G3S5, we could still return
	 * non-zero.
	 */
	switch (state) {
	case X86_G3:
		need_mask = CHIPSET_STATE_HARD_OFF;
		break;
	case X86_G3S5:
	case X86_S5G3:
		/*
		 * In between hard and soft off states.  Match only if caller
		 * will accept both.
		 */
		need_mask = CHIPSET_STATE_HARD_OFF | CHIPSET_STATE_SOFT_OFF;
		break;
	case X86_S5:
		need_mask = CHIPSET_STATE_SOFT_OFF;
		break;
	case X86_S5S3:
	case X86_S3S5:
		need_mask = CHIPSET_STATE_SOFT_OFF | CHIPSET_STATE_SUSPEND;
		break;
	case X86_S3:
		need_mask = CHIPSET_STATE_SUSPEND;
		break;
	case X86_S3S0:
	case X86_S0S3:
		need_mask = CHIPSET_STATE_SUSPEND | CHIPSET_STATE_ON;
		break;
	case X86_S0:
		need_mask = CHIPSET_STATE_ON;
		break;
	}

	/* Return non-zero if all needed bits are present */
	return (state_mask & need_mask) == need_mask;
}

void x86_interrupt(enum gpio_signal signal)
{
	/* Not implemented */
	return;
}


void chipset_task(void)
{
	/* Do nothing */
	while (1)
		sleep(5);
}


static int command_mock_power(int argc, char **argv)
{
	int mock_power_on;
	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	if (!parse_bool(argv[1], &mock_power_on))
		return EC_ERROR_PARAM1;

	if (mock_power_on)
		chipset_exit_hard_off();
	else
		chipset_force_shutdown();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(powermock, command_mock_power,
			"<on | off>",
			"Mock power state",
			NULL);
