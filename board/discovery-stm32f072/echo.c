/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Task to echo any characters from the three non-console USARTs back to all
 * non-console USARTs.
 */

#include "atomic.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "task.h"
#include "usart-stm32f0.h"
#include "util.h"

static size_t dropped;

static void in_ready(in_stream const * stream)
{
	task_wake(TASK_ID_ECHO);
}

USART_CONFIG(usart1, usart1_hw, 115200, 64, 64, in_ready, NULL)
USART_CONFIG(usart3, usart3_hw, 115200, 64, 64, in_ready, NULL)
USART_CONFIG(usart4, usart4_hw, 115200, 64, 64, in_ready, NULL)

static usart_config const * const usarts[] =
{
	&usart1,
	&usart3,
	&usart4,
};

static size_t echo(usart_config const * const usarts[], size_t usarts_count)
{
	size_t total = 0;
	size_t i;

	for (i = 0; i < usarts_count; ++i)
	{
		int     j;
		uint8_t buffer[64];
		size_t  count = in_stream_read(&(usarts[i]->in),
					       buffer,
					       sizeof(buffer));

		for (j = 0; j < usarts_count; ++j)
			dropped += count - out_stream_write(&(usarts[j]->out),
							    buffer,
							    count);

		total += count;
	}

	return total;
}

void echo_task(void)
{
	char const message[] = "Hello World!\r\n";
	size_t     i;

	for (i = 0; i < ARRAY_SIZE(usarts); ++i)
	{
		usart_init(usarts[i]);
		out_stream_write(&usarts[i]->out, message, strlen(message));
	}

	while (1)
	{
		while (echo(usarts, ARRAY_SIZE(usarts)))
			;

		/*
		 * There was nothing left to echo, go to sleep and be
		 * woken up by the next input.
		 */
		task_wait_event(-1);
	}
}

static int command_usart(int argc, char **argv)
{
	ccprintf("Echo task dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &dropped));

	ccprintf("USART1 RX dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &(usart1.state->rx_dropped)));

	ccprintf("USART3 RX dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &(usart3.state->rx_dropped)));

	ccprintf("USART4 RX dropped %d bytes\n",
		 atomic_read_clear((uint32_t *) &(usart4.state->rx_dropped)));

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(usart,
			command_usart,
			NULL,
			"Dump USART debug info",
			NULL);
