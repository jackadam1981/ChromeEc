/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fifo.h"

#include "util.h"

void fifo_init(fifo_config const * config)
{
	ASSERT(POWER_OF_TWO(config->size));

	config->state->head = 0;
	config->state->tail = 0;
}

size_t fifo_read(fifo_config const * config, uint8_t * buffer, size_t count)
{
	size_t transfer = MIN(count, fifo_count(config));
	size_t i;

	for (i = 0; i < transfer; ++i)
	{
		size_t index = (i + config->state->tail) & (config->size - 1);

		buffer[i] = config->buffer[index];
	}

	config->state->tail += transfer;

	return transfer;
}

size_t fifo_write(fifo_config const * config, uint8_t * buffer, size_t count)
{
	size_t transfer = MIN(count, fifo_space(config));
	size_t i;

	for (i = 0; i < transfer; ++i)
	{
		size_t index = (i + config->state->head) & (config->size - 1);

		config->buffer[index] = buffer[i];
	}

	config->state->head += transfer;

	return transfer;
}

size_t fifo_space(fifo_config const * config)
{
	return config->size - fifo_count(config);
}

size_t fifo_count(fifo_config const * config)
{
	return config->state->head - config->state->tail;
}
