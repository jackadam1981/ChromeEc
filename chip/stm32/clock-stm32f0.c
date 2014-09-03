/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "chipset.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hooks.h"
#include "registers.h"
#include "util.h"
#include "system.h"

/* use 48Mhz USB-synchronized High-speed oscillator */
#define HSI48_CLOCK 48000000

/* use PLL at 38.4MHz as system clock. */
#define PLL_CLOCK 38400000

#ifdef CONFIG_LOW_POWER_IDLE
void clock_refresh_console_in_use(void)
{
}



#ifdef CONFIG_FORCE_CONSOLE_RESUME
static void enable_serial_wakeup(int enable)
{
	static uint32_t save_exticr;

	if (enable) {
		/**
		 * allow to wake up from serial port (RX on pin PA10)
		 * by setting it as a GPIO with an external interrupt.
		 */
		save_exticr = STM32_SYSCFG_EXTICR(10 / 4);
		STM32_SYSCFG_EXTICR(10 / 4) = (save_exticr & ~(0xf << 8));
		STM32_EXTI_RTSR |= (1<<10);
	} else {
		/* serial port wake up : don't go back to sleep */
		if (STM32_EXTI_PR & (1 << 10))
			disable_sleep(SLEEP_MASK_FORCE_NO_DSLEEP);
		/* restore keyboard external IT on PC10 */
		STM32_SYSCFG_EXTICR(10 / 4) = save_exticr;
	}
}
#else
static void enable_serial_wakeup(int enable)
{
}
#endif

/* Idle task.  Executed when no tasks are ready to be scheduled. */
void __idle(void)
{
	while (1) {
		asm volatile("cpsid i");

// 		if (DEEP_SLEEP_ALLOWED && (next_delay > STOP_MODE_LATENCY)) {
			/* deep-sleep in STOP mode */

// 			enable_serial_wakeup(1);

			/* set deep sleep bit */
// 			CPU_SCB_SYSCTRL |= 0x4;
//
// 			asm("wfi");
//
// 			CPU_SCB_SYSCTRL &= ~0x4;

// 			enable_serial_wakeup(0);

			/* re-lock the PLL */
// 			config_hispeed_clock();
// 		} else {
// 			/* normal idle : only CPU clock stopped */
			asm("wfi");
// 		}
		asm volatile("cpsie i");
	}
}

static int command_sleep(int argc, char **argv)
{
	asm volatile("cpsid i");
	enable_serial_wakeup(1);
	CPU_SCB_SYSCTRL |= 0x4;
	asm("wfi");
	CPU_SCB_SYSCTRL &= ~0x4;
	clock_init();
	enable_serial_wakeup(0); /*BUG: first character gets corrupted */
	asm volatile("cpsie i");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sleep, command_sleep,
			"sleep",
			"sleep",
			NULL);
#endif /* CONFIG_LOW_POWER_IDLE */

int clock_get_freq(void)
{
	return CPU_CLOCK;
}

void clock_enable_module(enum module_id module, int enable)
{
}

void clock_init(void)
{
	/*
	 * The initial state :
	 *  SYSCLK from HSI (=8MHz), no divider on AHB, APB1, APB2
	 *  PLL unlocked, RTC enabled on LSE
	 */

#if (CPU_CLOCK >= 30000)
	/* put 1 Wait-State for flash access to ensure proper reads at 48Mhz */
	STM32_FLASH_ACR = 0x1001; /* 1 WS / Prefetch enabled */

	/* Ensure that HSI48 is ON */
	if (!(STM32_RCC_CR2 & (1 << 17))) {
		/* Enable HSI */
		STM32_RCC_CR2 |= 1 << 16;
		/* Wait for HSI to be ready */
		while (!(STM32_RCC_CR2 & (1 << 17)))
			;
	}
#endif

#if (CPU_CLOCK == 8000000)
	/*
	 * HSI = 8MHz, no prescaler, no MCO, no PLL
	 * therefore PCLK = FCLK = SYSCLK = 8MHz
	 * USB uses HSI48 = 48MHz
	 */

	/* switch SYSCLK to HSI48 */
	STM32_RCC_CFGR = 0x00000000;

	/* wait until the HSI48 is the clock source */
	while ((STM32_RCC_CFGR & 0xc) != 0x0)
		;

#elif (CPU_CLOCK == HSI48_CLOCK)
	/*
	 * HSI48 = 48MHz, no prescaler, no MCO, no PLL
	 * therefore PCLK = FCLK = SYSCLK = 48MHz
	 * USB uses HSI48 = 48MHz
	 */

	/* switch SYSCLK to HSI48 */
	STM32_RCC_CFGR = 0x00000003;

	/* wait until the HSI48 is the clock source */
	while ((STM32_RCC_CFGR & 0xc) != 0xc)
		;

#elif (CPU_CLOCK == PLL_CLOCK)
	/*
	 * HSI48 = 48MHz, no prescalar, no MCO, with PLL *4/5 => 38.4MHz SYSCLK
	 * therefore PCLK = FCLK = SYSCLK = 38.4MHz
	 * USB uses HSI48 = 48MHz
	 */

	/* If PLL is the clock source, PLL has already been set up. */
	if ((STM32_RCC_CFGR & 0xc) == 0x8)
		return;

	/*
	 * Specify HSI48 clock as input clock to PLL and set PLL multiplier
	 * and divider.
	 */
	STM32_RCC_CFGR = 0x00098000;
	STM32_RCC_CFGR2 = 0x4;

	/* Enable the PLL. */
	STM32_RCC_CR |= 0x01000000;

	/* Wait until PLL is ready. */
	while (!(STM32_RCC_CR & 0x02000000))
		;

	/* Switch SYSCLK to PLL. */
	STM32_RCC_CFGR |= 0x2;

	/* wait until the PLL is the clock source */
	while ((STM32_RCC_CFGR & 0xc) != 0x8)
		;

#else
#error "CPU_CLOCK must be either 48MHz or 38.4MHz"
#endif
}
