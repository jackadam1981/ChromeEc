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

#define CONSOLE_IN_USE_ON_BOOT_TIME (15*SECOND)
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
	printk("[power]uart_deepsleep_interrupt\n");
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

extern volatile int wait_interrupt_fired;

/* CROS PM policy handler */
struct pm_state_info pm_policy_next_state(int32_t ticks)
{
	uint32_t mie, meie = BIT(11);

	/* Deep sleep is allowed and console is not in use. */
	if (DEEP_SLEEP_ALLOWED && clock_allow_low_power_idle()) {

		/* Read and clear MEIE bit of MIE register. */
		__asm__ volatile ("csrrc %0, mie, %1" : "=r"(mie) : "r"(meie));

		/* enable uart wui */
		gpio_enable_interrupt(GPIO_UART1_RX);

		/* deep doze mode */
		chip_pll_ctrl(CHIP_PLL_DEEP_DOZE);

		wait_interrupt_fired = 1;

		/* Wait for interrupt */
		__asm__ volatile ("wfi");

		/* bit11: restore mie */
		__asm__ volatile ("csrw mie, %0" : : "r"(mie));

		disable_sleep(SLEEP_MASK_SPI); //test power policy!

		while (wait_interrupt_fired) {
			//printk("wfi=%d\n",wait_interrupt_fired);
			__asm__ volatile ("wfi");
		}

		return pm_min_residency[ARRAY_SIZE(pm_min_residency) - 1];
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
