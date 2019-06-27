/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_MT8183_H
#define __CROS_EC_MT8183_H

/* power signal definitions */
enum power_signal {
	AP_IN_S3_L,
	PMIC_PWR_GOOD,

	/* Number of signals */
	POWER_SIGNAL_COUNT,
};

/* Board-specific function to assert/deassert PMIC_FORCE_RESET pin */
void board_set_pmic_force_reset(int asserted);

#endif /* __CROS_EC_MT8183_H */
