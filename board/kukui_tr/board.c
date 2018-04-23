/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kukui eMMC emulator board configuration */

#include "clock.h"
#include "common.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "printf.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "gpio_list.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
	/* Set all four SPI pins to high speed */
	/* pins PA4/5/6/7, PB4/5/6/7 */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0x0000ff00;
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x0000ff00;
}
/* This needs to happen before PWM is initialized. */
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_INIT_PWM - 1);

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/* Remap USART DMA to match the USART driver */
	/*
	 * the DMA mapping is :
	 *  Chan 4 : USART1_TX
	 *  Chan 5 : USART1_RX
	 */
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10); /* Remap USART1 RX/TX DMA */
}

