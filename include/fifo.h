/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_FIFO_H
#define INCLUDE_FIFO_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
	size_t head;
	size_t tail;
} fifo_state;

typedef struct
{
	uint8_t * buffer;
	size_t    size;
} fifo_config;

void fifo_init(fifo_config const * config,
	       fifo_state volatile * state);

size_t fifo_read(fifo_config const * config,
		 fifo_state volatile * state,
		 uint8_t * buffer,
		 size_t count);

size_t fifo_write(fifo_config const * config,
		  fifo_state volatile * state,
		  uint8_t * buffer,
		  size_t count);

size_t fifo_space(fifo_config const * config,
		  fifo_state const volatile * state);

size_t fifo_count(fifo_config const * config,
		  fifo_state const volatile * state);

#endif //INCLUDE_FIFO_H
