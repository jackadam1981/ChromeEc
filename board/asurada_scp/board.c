/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Asurada SCP configuration */

#include "registers.h"

/* Build GPIO tables */
#include "gpio_list.h"

#include "task.h"
#include "timer.h"

static uint32_t next = 127;
static uint32_t myrand(void)
{
	next = next * 1103515245 + 12345;
	return ((uint32_t)(next/65536) % 32768);
}

void dummy_task(void)
{
	int i = 0;

	while (1) {
		ccprints("%s: %d", __func__, ++i);
		cflush();
		sleep(myrand() % 10 + 1);
	}
}
