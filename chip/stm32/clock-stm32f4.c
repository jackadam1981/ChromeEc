/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

#include "atomic.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "hwtimer.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"


/*
 * HSI = 16MHz
 * VCO = HSI * (N / M) = 168 Mhz, N = 105, M = 10
 * [100Mhz<= VCO=168Mhz <= 432Mhz]
 * SYSCLK : PLL = VCO / P = 84Mhz, P = 2
 * Q = 4, F random generator VCO /Q = 42 MHz.
 */
#define RCC_PLLCFGR ((2 << 24) | (0 << 16) | (105 << 6) | (10 << 0))


/*
 * AHB  = 84Mhz (Prescaler /1)
 * APB2 = 84Mhz (Prescaler /1, Max = 84Mhz)
 * APB1 = 42Mhz (Prescaler /2, Max = 42Mhz)
 */
#define RCC_CFGR 0x00001000

void config_hispeed_clock(void)
{
	/* Ensure that HSI is ON */
	if (!(STM32_RCC_CR & STM32_RCC_CR_HSIRDY)) {
		/* Enable HSI */
		STM32_RCC_CR |= STM32_RCC_CR_HSION;
		/* Wait for HSI to be ready */
		while (!(STM32_RCC_CR & STM32_RCC_CR_HSIRDY))
			;
	}

	STM32_RCC_CFGR = RCC_CFGR | (STM32_RCC_CFGR & 0x300);
	STM32_RCC_PLLCFGR = RCC_PLLCFGR | (STM32_RCC_PLLCFGR & 0xF0BC8000);
	/* Enable the PLL */
	STM32_RCC_CR |= STM32_RCC_CR_PLLON;
	/* Wait for the PLL to lock */
	while (!(STM32_RCC_CR & STM32_RCC_CR_PLLRDY))
		;
	/* switch to SYSCLK to the PLL */
	STM32_RCC_CFGR |= STM32_RCC_CFGR_SW_PLL;

	/* wait until the PLL is the clock source */
	while ((STM32_RCC_CFGR & STM32_RCC_CFGR_SWS_MASK) !=
		STM32_RCC_CFGR_SWS_PLL)
		;
}

int clock_get_freq(enum clock_type type)
{
	switch (type) {
	case CLOCK_TYPE_CPU:
	case CLOCK_TYPE_FAST_PERIPH:  /* APB2 */
		return CPU_CLOCK;
	case CLOCK_TYPE_SLOW_PERIPH:  /* APB1 */
		return CPU_CLOCK / 2;
	default:
		return 0;
	}

}

void clock_init(void)
{
	/* Enable 3 Wait State, prefecth, Instruction and Data Caches */
	STM32_FLASH_ACR = 0x703;
	while ((STM32_FLASH_ACR & 0x3) != 0x3)
		;

	config_hispeed_clock();
}
