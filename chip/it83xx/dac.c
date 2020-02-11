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
#include "util.h"

static void dac_enable_channel(enum chip_dac_channel ch)
{
	/* DAC module enable */
	IT83XX_DAC_DACPDREG &= ~IT83XX_DAC_POWDN(ch);
}

static void dac_disable_channel(enum chip_dac_channel ch)
{
	/* DAC module disable */
	IT83XX_DAC_DACPDREG |= IT83XX_DAC_POWDN(ch);
}

/* Set DAC output raw data */
static void dac_set_output_voltage(enum chip_dac_channel ch, uint8_t data)
{
	IT83XX_DAC_DACDAT(ch) = data;
}

/* Get DAC output raw data */
static uint8_t dac_get_output_voltage(enum chip_dac_channel ch)
{
	return IT83XX_DAC_DACDAT(ch);
}

/* DAC module Initialization */
static void dac_init(void)
{
	/* Configure GPIOs */
	gpio_config_module(MODULE_DAC, 1);
}
DECLARE_HOOK(HOOK_INIT, dac_init, HOOK_PRIO_INIT_DAC);

static int command_dactest(int argc, char **argv)
{
	char *e;
	int ch, enable;
	uint8_t dac_raw_data, rv;

	if (argc < 4)
		return EC_ERROR_PARAM_COUNT;

	ch = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;
	if (ch < 2 || ch > 5) {
		ccprintf("ch%d is not supported\n", ch);
		return EC_ERROR_PARAM2;
	}

	enable = strtoi(argv[3], &e, 0);
	if (*e)
		return EC_ERROR_PARAM3;

	if (enable) {
		if (strcasecmp(argv[1], "r") == 0) {
			/* Get DAC output raw data */
			rv = dac_get_output_voltage(ch);
			ccprintf("dac ch%d raw data=0x%02x\n", ch, rv);
		} else if (strcasecmp(argv[1], "w") == 0) {
			/*
			 * DAC data register raw data
			 * 0 ~ 0xFF(8-bit) = voltage 0 ~ 3.3v
			 */
			dac_raw_data = strtoi(argv[4], &e, 0);
			if (*e)
				return EC_ERROR_PARAM4;

			/* Set DAC output raw data */
			dac_set_output_voltage(ch, dac_raw_data);
		} else {
			return EC_ERROR_PARAM1;
		}
		dac_enable_channel(ch);
	} else
		dac_disable_channel(ch);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dactest, command_dactest,
			"r/w ch2-ch5 [enable=1|disable=0] [dac_raw_data]",
			"Read or write DAC");
