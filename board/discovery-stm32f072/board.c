/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* STM32F072-discovery board configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#include "chip/stm32/usart-stm32f0.h"

static void button_event(enum gpio_signal signal);

#include "gpio_list.h"

static void button_event(enum gpio_signal signal)
{
	static int count = 0;

	gpio_set_level(GPIO_LED_U, (count & 0x03) == 0);
	gpio_set_level(GPIO_LED_R, (count & 0x03) == 1);
	gpio_set_level(GPIO_LED_D, (count & 0x03) == 2);
	gpio_set_level(GPIO_LED_L, (count & 0x03) == 3);

	count++;
}

/*
 * Simple configuration to echo any characters from the three non-console
 * USARTS back to all non-console USARTS.
 */
static void in_ready(in_stream const * stream);

USART_CONFIG(usart1, usart1_hw, 64, 64, in_ready, NULL)
USART_CONFIG(usart3, usart3_hw, 64, 64, in_ready, NULL)
USART_CONFIG(usart4, usart4_hw, 64, 64, in_ready, NULL)

static void in_ready(in_stream const * stream)
{
	uint8_t buffer[8];
	size_t  count = stream->ops->read(stream, buffer, sizeof(buffer));

	usart1.out.ops->write(&usart1.out, buffer, count);
	usart3.out.ops->write(&usart3.out, buffer, count);
	usart4.out.ops->write(&usart4.out, buffer, count);
}

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
