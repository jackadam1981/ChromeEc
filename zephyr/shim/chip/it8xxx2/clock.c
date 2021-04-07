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
#include <soc/ite_it8xxx2/reg_def_cros.h>

#include "module_id.h"

LOG_MODULE_REGISTER(shim_clock, LOG_LEVEL_ERR);

#define ECPM_NODE		DT_INST(0, ite_it8xxx2_pcc)
#define HAL_CDCG_REG_BASE_ADDR \
			((struct ecpm_reg *)DT_REG_ADDR_BY_IDX(ECPM_NODE, 0))

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
	struct ecpm_reg *const ecpm_base = HAL_CDCG_REG_BASE_ADDR;
	int reg_val = ecpm_base->ECPM_PLLFREQ & 0xf;

	return pll_reg_to_freq[reg_val];
}
