/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_POWER_STATUS_H__
#define __CROS_EC_POWER_STATUS_H__

/* Power status related items that need to be exported */

struct board_power_config {
	/* Battery 1C rating */
	const int batt_1c_level;
	/* Efficiency of AC adapter power conversion */
	const int nominal_charger_eff;
	/* Efficiency of RoP VRs at average power */
	const int rop_avg_eff;
	/* Efficiency of RoP VRs at peak power */
	const int rop_peak_eff;
	/* Efficiency of SOC VRs at average power */
	const int soc_avg_eff;
	/* Efficiency of SOC VRs at peak power */
	const int soc_peak_eff;
	/* Worst case RoP power */
	const int rop_worst;
	/* Average RoP power */
	const int rop_avg;
	/* Peak RoP power */
	const int rop_peak;
	/* DBPT v2 - in mOhm */
	const int sys_resistance;
	/* DBPT v2 - in mV */
	const int min_sys_voltage;
	/* Adapter rating - in W */
	const int adapter_rating;
};

/* Expected to be provided by the board */
const struct board_power_config *board_get_power_config(void);

#endif /* __CROS_EC_POWER_STATUS_H__ */
