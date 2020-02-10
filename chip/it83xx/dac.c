/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx DAC module for Chrome EC */

#include "console.h"
#include "dac_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"

/* Data structure of DAC channel control registers. */
struct dac_ctrl_t dac_ctrl_regs[] = {
	{IT83XX_DAC_POWDN2, &IT83XX_DAC_DACDAT2},
	{IT83XX_DAC_POWDN3, &IT83XX_DAC_DACDAT3},
	{IT83XX_DAC_POWDN4, &IT83XX_DAC_DACDAT4},
	{IT83XX_DAC_POWDN5, &IT83XX_DAC_DACDAT5},
};
BUILD_ASSERT(ARRAY_SIZE(dac_ctrl_regs) == CHIP_DAC_COUNT);

static void dac_enable_channel(int ch)
{
	/* dac module enable */
	IT83XX_DAC_DACPDREG &= ~(dac_ctrl_regs[ch].dac_ctrl);
}

static void dac_disable_channel(int ch)
{
	/* dac module disable */
	IT83XX_DAC_DACPDREG |= dac_ctrl_regs[ch].dac_ctrl;
}

/* DAC module Initialization */
static void dac_init(void)
{
	int index;
	int ch;

	/* Configure GPIOs */
	gpio_config_module(MODULE_DAC, 1);

	for (index = 0; index < DAC_CH_COUNT; index++) {
		ch = dac_channels[index].channel;

		dac_disable_channel(ch);

		/*
		 *voltage 0 ~ 3.3v = dac data register
		 *raw data 0 ~ FFh (8-bit)
		 */
		*dac_ctrl_regs[ch].dac_data = dac_channels[ch].dac_raw_data;

		dac_enable_channel(ch);
	}
}
DECLARE_HOOK(HOOK_INIT, dac_init, HOOK_PRIO_INIT_DAC);
