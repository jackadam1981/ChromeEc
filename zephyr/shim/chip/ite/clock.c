/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/clock_control.h>
#include <kernel.h>
#include <logging/log.h>
#include <soc.h>
#include <zephyr.h>

/* include "clock_chip.h" */
#include "module_id.h"

LOG_MODULE_REGISTER(shim_clock, LOG_LEVEL_ERR);

static const int pll_to_freq[8] = {
	8000000,
	16000000,
	24000000,
	32000000,
	48000000,
	64000000,
	72000000,
	96000000
};

int clock_get_freq(void)
{
	int pll, freq;

	pll = PLLFREQR & 0xf;
	freq = pll_to_freq[pll];
	return freq;
}

void clock_turbo(void)
{
	/* TODO: implement me */
}

void clock_normal(void)
{
	/* TODO: implement me */

}

void clock_enable_module(enum module_id module, int enable)
{
	/* TODO: implement me */
}
