/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Console command to query USART state
 */
#include "atomic.h"
#include "common.h"
#include "console.h"
#include "usart.h"
#include STRINGIFY(usart-CHIP_FAMILY.h)

static int command_usart_info(int argc, char **argv)
{
        size_t i;

	for (i = 0; i < STM32_USARTS_MAX; i++) {
		struct usart_config const *config = usart_configs[i];

		if (config == NULL)
			continue;

		ccprintf("USART%d\n"
			 "    dropped %d bytes\n"
			 "    overran %d times\n",
			 config->hw->index + 1,
			 atomic_read_clear(&(config->state->rx_dropped)),
			 atomic_read_clear(&(config->state->rx_overrun)));
	}

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(usart_info,
			command_usart_info,
			NULL,
			"Display USART info",
			NULL);
