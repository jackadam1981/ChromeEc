/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* STM32F072-discovery board configuration */

#include "atomic.h"
#include "common.h"
#include "console.h"
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
static size_t echo(out_stream const * stream,
		   uint8_t const * buffer,
		   size_t count)
{
	size_t dropped = 0;

#if 0
	size_t i;
	for (i = 0; i < count; ++i)
	{
		uint8_t	const cr = '\n';

		if (buffer[i] == '\r')
			dropped += 1 - out_stream_write(stream, &cr, 1);

		dropped += 1 - out_stream_write(stream, &buffer[i], 1);
	}
#else
	dropped += count - out_stream_write(stream, buffer, count);
#endif

	return dropped;
}

size_t dropped;
int wake_count;

void echo_task(void)
{
	usart_config const * const usarts[]     = { &usart1, &usart3, &usart4 };
	size_t const               usarts_count = 3;

	while (1)
	{
		size_t total = 0;
		int    i;

		for (i = 0; i < usarts_count; ++i)
		{
			int     j;
			uint8_t buffer[64];
			size_t  count = in_stream_read(&(usarts[i]->in),
						       buffer,
						       sizeof(buffer));

			for (j = 0; j < usarts_count; ++j)
				dropped += echo(&(usarts[j]->out),
						buffer,
						count);

			total += count;
		}

		if (total == 0)
		{
			/*
			 * There was nothing left to echo, go to sleep and be
			 * woken up by the next input.
			 */
			button_event(GPIO_USER_BUTTON);

			ccprints("sleep", total);

			task_wait_event(-1);

			wake_count++;
		}
		else
		{
			ccprints("%d bytes", total);
		}
	}
}

static void in_ready(in_stream const * stream)
{
	task_wake(TASK_ID_ECHO);
}

USART_CONFIG(usart1, usart1_hw, 115200, 64, 64, in_ready, NULL)
USART_CONFIG(usart3, usart3_hw, 115200, 64, 64, in_ready, NULL)
USART_CONFIG(usart4, usart4_hw, 115200, 64, 64, in_ready, NULL)

static int command_usart(int argc, char **argv)
{
	ccprintf("Echo task dropped %d bytes\n", dropped);
	ccprintf("Echo task woke %d times\n", wake_count);

	ccprintf("USART1 RX dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &(usart1.state->rx_dropped)));

	ccprintf("USART3 RX dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &(usart3.state->rx_dropped)));

	ccprintf("USART4 RX dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &(usart4.state->rx_dropped)));

	wake_count = 0;
	dropped    = 0;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(usart, command_usart,
			NULL,
			"Dump USART debug info",
			NULL);

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON);

	usart_init(&usart1);
	usart_init(&usart3);
	usart_init(&usart4);

	out_stream_write(&usart1.out, "Hello World!\r\n", 14);
	out_stream_write(&usart3.out, "Hello World!\r\n", 14);
	out_stream_write(&usart4.out, "Hello World!\r\n", 14);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
