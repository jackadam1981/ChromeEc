/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks and power management settings */

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
#define AHB_PRESCALAR 1
#define APB2_PRESCALAR 1
#define APB1_PRESCALAR 2

#define AHB_PRESCALAR_FIELD(x) \
	((x) == 1 ? 0 : 0x8 | (30 - __builtin_clz(x)))

#define APB_PRESCALAR_FIELD(x) \
	((x) == 1 ? 0 : 0x4 | (30 - __builtin_clz(x)))

#define RCC_CFGR ((APB_PRESCALAR_FIELD(APB2_PRESCALAR) << 13) | \
		  (APB_PRESCALAR_FIELD(APB1_PRESCALAR) << 10) | \
		  (AHB_PRESCALAR_FIELD(AHB_PRESCALAR)))

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
		return CPU_CLOCK;
	case CLOCK_TYPE_HOST_PERIPH:  /* AHB1/2 */
		return CPU_CLOCK / AHB_PRESCALAR;
	case CLOCK_TYPE_FAST_PERIPH:  /* APB2 */
		return CPU_CLOCK / (AHB_PRESCALAR * APB2_PRESCALAR);
	case CLOCK_TYPE_SLOW_PERIPH:  /* APB1 */
		return CPU_CLOCK / (AHB_PRESCALAR * APB1_PRESCALAR);
	default:
		return 0;
	}

}

void clock_wait_bus_cycles(enum bus_type bus, uint32_t cycles)
{
	volatile uint32_t dummy __attribute__((unused));

	if (bus == BUS_AHB) {
		while (cycles--)
			dummy = STM32_DMA1_REGS->isr[0];
	} else { /* APB */
		/* There is 2 kind of APB buses, APB1 the slowest */
		clock_wait_bus_cycles(BUS_AHB, cycles * APB1_PRESCALAR);
	}
}

void clock_init(void)
{
	/* Flash settings depeend on the voltage and frequency */
	int ws = (clock_get_freq(CLOCK_TYPE_CPU) / CONFIG_FLASH_WS_DIV) <<
		STM32_FLASH_ACR_SHIFT;
	int acr = ws;
#ifndef CONFIG_VOLTAGE_CORE_1800
	/* Enable Wait State, prefetch, Instruction and Data Caches */
	acr |= STM32_FLASH_ACR_PRFTEN | STM32_FLASH_ACR_ICEN |
		STM32_FLASH_ACR_DCEN;
#endif
	STM32_FLASH_ACR = acr;
	while ((STM32_FLASH_ACR & STM32_FLASH_ACR_LAT_MASK) != ws)
		;

	config_hispeed_clock();
}
