/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <pm/pm.h>
#include <soc.h>
#include <zephyr.h>

#include "console.h"
#include "cros_version.h"
#include "gpio.h"
#include "system.h"
#include "timer.h"
#include "uart.h"

static const struct pm_state_info pm_min_residency[] =
	PM_STATE_INFO_DT_ITEMS_LIST(DT_NODELABEL(cpu0));

#define CONSOLE_IN_USE_ON_BOOT_TIME (5*SECOND)
#define CONSOLE_IN_USE_TIMEOUT_SEC (5*SECOND)

static timestamp_t console_expire_time;
static timestamp_t sleep_mode_t0;

void clock_refresh_console_in_use(void)
{
	/* Set console in use expire time. */
	console_expire_time = get_time();
	console_expire_time.val += CONSOLE_IN_USE_TIMEOUT_SEC;
}

void uart_deepsleep_interrupt(enum gpio_signal signal)
{
	clock_refresh_console_in_use();
	/* Disable interrupts on UART1 RX pin to avoid repeated interrupts. */
	gpio_disable_interrupt(GPIO_UART1_RX);
}

static int clock_allow_low_power_idle(void)
{
	sleep_mode_t0 = get_time();

	/* If we are waked up by console, then keep awake at least 5s. */
	if (sleep_mode_t0.val < console_expire_time.val)
		return 0;

	return 1;
}

/* CROS PM policy handler */
struct pm_state_info pm_policy_next_state(int32_t ticks)
{
	/* Deep sleep is allowed and console is not in use. */
	if (DEEP_SLEEP_ALLOWED && clock_allow_low_power_idle()) {

		for (int i = ARRAY_SIZE(pm_min_residency) - 1; i >= 0; i--) {
			/* Find suitable power state by residency time */
			if (ticks == K_TICKS_FOREVER ||
			    ticks >= k_us_to_ticks_ceil32(
					     pm_min_residency[i]
						     .min_residency_us)) {
				return pm_min_residency[i];
			}
		}
	}

	return (struct pm_state_info){PM_STATE_ACTIVE, 0, 0};
}

static int power_policy_init(const struct device *arg)
{
	ARG_UNUSED(arg);

	console_expire_time.val = get_time().val + CONSOLE_IN_USE_ON_BOOT_TIME;

	return 0;
}
SYS_INIT(power_policy_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
