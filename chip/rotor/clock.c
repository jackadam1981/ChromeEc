/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clock and power management settings */

#include "common.h"

static int freq = CPU_CLOCK;

void clock_init(void)
{
	/* Clocks should already be initialized for us. */
}

int clock_get_freq(void)
{
	return freq;
}
