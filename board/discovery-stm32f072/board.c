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
 * USARTs back to all non-console USARTs.
 */
static size_t echo(in_stream const * stream)
{
	uint8_t buffer[8];
	size_t  count = in_stream_read(stream, buffer, sizeof(buffer));

	if (count)
	{
		out_stream_write(&usart1.out, buffer, count);
		out_stream_write(&usart3.out, buffer, count);
		out_stream_write(&usart4.out, buffer, count);
	}

	return count;
}

void echo_task(void)
{
	while (1)
	{
		int	count = 0;

		count += echo(&usart1.in);
		count += echo(&usart3.in);
		count += echo(&usart4.in);

		if (count == 0)
		{
			/*
			 * There was nothing left to echo, go to sleep and be
			 * woken up by the next input.
			 */
			button_event(GPIO_USER_BUTTON);

			task_wait_event(-1);
		}
	}
}

static void in_ready(in_stream const * stream)
{
	task_wake(TASK_ID_ECHO);
}

USART_CONFIG(usart1, usart1_hw, 115200, 16, 16, in_ready, NULL)
USART_CONFIG(usart3, usart3_hw, 115200, 16, 16, in_ready, NULL)
USART_CONFIG(usart4, usart4_hw, 115200, 16, 16, in_ready, NULL)

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);

	usart_init(&usart1);
	usart_init(&usart3);
	usart_init(&usart4);

	out_stream_write(&usart1.out, "Hello World!\n", 13);
	out_stream_write(&usart3.out, "Hello World!\n", 13);
	out_stream_write(&usart4.out, "Hello World!\n", 13);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
