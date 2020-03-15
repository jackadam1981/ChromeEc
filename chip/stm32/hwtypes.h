/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_HWTYPES_H
#define __CROS_EC_HWTYPES_H

/*
 * This file shall not include other headers that could circle
 * back to common.h or config.h.
 */

enum clock_osc {
	OSC_HSI = 0,	/* High-speed internal oscillator */
	OSC_CSI,	/* Multi-speed internal oscillator: NOT IMPLEMENTED */
	OSC_HSE,	/* High-speed external oscillator: NOT IMPLEMENTED */
	OSC_PLL,	/* PLL */
};

enum voltage_scale {
	VOLTAGE_SCALE0 = 0,
	VOLTAGE_SCALE1,
	VOLTAGE_SCALE2,
	VOLTAGE_SCALE3,
	VOLTAGE_SCALE_COUNT,
};

enum freq {
	FREQ_1KHZ   = 1000,
	FREQ_32KHZ  = 32  * FREQ_1KHZ,
	FREQ_56KHZ  = 56  * FREQ_1KHZ,
	FREQ_1MHZ   = 1000000,
	FREQ_2MHZ   = 2   * FREQ_1MHZ,
	FREQ_16MHZ  = 16  * FREQ_1MHZ,
	FREQ_64MHZ  = 64  * FREQ_1MHZ,
	FREQ_140MHZ = 140 * FREQ_1MHZ,
	FREQ_200MHZ = 200 * FREQ_1MHZ,
	FREQ_280MHZ = 280 * FREQ_1MHZ,
	FREQ_400MHZ = 400 * FREQ_1MHZ,
	FREQ_480MHZ = 480 * FREQ_1MHZ,
};

#endif /* __CROS_EC_HWTYPES_H */
