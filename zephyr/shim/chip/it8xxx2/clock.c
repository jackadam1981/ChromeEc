/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/clock_control.h>
#include <kernel.h>
#include <logging/log.h>
#include <soc.h>
#include <zephyr.h>

#include "module_id.h"

LOG_MODULE_REGISTER(shim_clock, LOG_LEVEL_ERR);

static const int pll_reg_to_freq[8] = {
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
	return pll_reg_to_freq[IT8XXX2_ECPM_PLLFREQR_MASK];
}
