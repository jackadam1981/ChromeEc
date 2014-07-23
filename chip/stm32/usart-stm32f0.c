/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chip/stm32/usart-impl.h"

#include "common.h"
#include "clock.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"

/**
 * Handle clock frequency changes
 */
static void usart_freq_change(usart_config const * config)
{
	int      div  = DIV_ROUND_NEAREST(clock_get_freq(), config->baud);
	intptr_t base = config->hw->base;

	if (div / 16 > 0) {
		/*
		 * CPU clock is high enough to support x16 oversampling.
		 * BRR = (div mantissa)<<4 | (4-bit div fraction)
		 */
		STM32_USART_CR1(base) &= ~STM32_USART_CR1_OVER8;
		STM32_USART_BRR(base) = div;
	} else {
		/*
		 * CPU clock is low; use x8 oversampling.
		 * BRR = (div mantissa)<<4 | (3-bit div fraction)
		 */
		STM32_USART_BRR(base) = ((div / 8) << 4) | (div & 7);
		STM32_USART_CR1(base) |= STM32_USART_CR1_OVER8;
	}
}

static void freq_change(void)
{
#if defined(CONFIG_USART1)
	usart_freq_change(&CONFIG_USART1);
#endif

#if defined(CONFIG_USART2)
	usart_freq_change(&CONFIG_USART2);
#endif

#if defined(CONFIG_USART3)
	usart_freq_change(&CONFIG_USART3);
#endif

#if defined(CONFIG_USART4)
	usart_freq_change(&CONFIG_USART4);
#endif
}

DECLARE_HOOK(HOOK_FREQ_CHANGE, freq_change, HOOK_PRIO_DEFAULT);

void usart_variant_init(usart_config const * config)
{
	usart_freq_change(config);
}

/*
 * USART interrupt bindings.  These functions can not be defined as static or
 * they will be removed by the linker because of the way that DECLARE_IRQ works.
 */
#if defined(CONFIG_USART1)
usart_hw_config const usart1_hw =
{
	.base           = STM32_USART1_BASE,
	.irq            = STM32_IRQ_USART1,
	.clock_register = &STM32_RCC_APB2ENR,
	.clock_enable   = STM32_RCC_PB2_USART1,
};

void usart1_interrupt(void)
{
	usart_interrupt(&CONFIG_USART1);
}

DECLARE_IRQ(STM32_IRQ_USART1, usart1_interrupt, 2);
#endif

#if defined(CONFIG_USART2)
usart_hw_config const usart2_hw =
{
	.base           = STM32_USART2_BASE,
	.irq            = STM32_IRQ_USART2,
	.clock_register = &STM32_RCC_APB1ENR,
	.clock_enable   = STM32_RCC_PB1_USART2,
};

void usart2_interrupt(void)
{
	usart_interrupt(&CONFIG_USART2);
}

DECLARE_IRQ(STM32_IRQ_USART2, usart2_interrupt, 2);
#endif

#if defined(CONFIG_USART3)
usart_hw_config const usart3_hw =
{
	.base           = STM32_USART3_BASE,
	.irq            = STM32_IRQ_USART3_4,
	.clock_register = &STM32_RCC_APB1ENR,
	.clock_enable   = STM32_RCC_PB1_USART3,
};
#endif

#if defined(CONFIG_USART4)
usart_hw_config const usart4_hw =
{
	.base           = STM32_USART4_BASE,
	.irq            = STM32_IRQ_USART3_4,
	.clock_register = &STM32_RCC_APB1ENR,
	.clock_enable   = STM32_RCC_PB1_USART4,
};
#endif

#if defined(CONFIG_USART3) || defined(CONFIG_USART4)
void usart3_4_interrupt(void)
{
#if defined(CONFIG_USART3)
	usart_interrupt(&CONFIG_USART3);
#endif

#if defined(CONFIG_USART4)
	usart_interrupt(&CONFIG_USART4);
#endif
}

DECLARE_IRQ(STM32_IRQ_USART3_4, usart3_4_interrupt, 2);
#endif
