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
	CHIP_DAC_CH2 = 2,
	CHIP_DAC_CH3,
	CHIP_DAC_CH4,
	CHIP_DAC_CH5,
};

#endif /* __CROS_EC_DAC_CHIP_H */

