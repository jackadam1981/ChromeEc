/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_POWER_QCOM_H_
#define __CROS_EC_POWER_QCOM_H_

enum power_signal {
	AP_RST_ASSERTED = 0,
	PS_HOLD,
	POWER_GOOD,
	AP_SUSPEND,
	POWER_SIGNAL_COUNT,
};

/* Swithcap functions */
void board_set_switchcap_power(int enable);
int board_is_switchcap_enabled(void);
int board_is_switchcap_power_good(void);

#endif /* __CROS_EC_POWER_QCOM_H_ */
