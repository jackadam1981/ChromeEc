/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx DAC module for Chrome EC */

#include "common.h"

#ifndef __CROS_EC_DAC_CHIP_H
#define __CROS_EC_DAC_CHIP_H

/* List of DAC channels. */
enum chip_dac_channel {
	CHIP_DAC_CH2 = 0,
	CHIP_DAC_CH3,
	CHIP_DAC_CH4,
	CHIP_DAC_CH5,
	CHIP_DAC_COUNT
};

/* Data structure to define DAC channel control registers. */
struct dac_ctrl_t {
	uint8_t dac_ctrl;

	volatile uint8_t *dac_data;
};

/* Data structure to define DAC channels. */
struct dac_t {
	const char *name;
	uint8_t  dac_raw_data;
	enum chip_dac_channel channel;
};

/*
 * Boards must provide this list of DAC channel definitions. This must match
 * the enum dac_channel list provided by the board.
 */
extern const struct dac_t dac_channels[];

#endif /* __CROS_EC_DAC_CHIP_H */

