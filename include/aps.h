/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_APS__H
#define __CROS_EC_APS__H

#include <stdbool.h>

#include "common.h"

#define APS_FLAG_NEW_CONTRACT		BIT(0)
#define APS_FLAG_ALL			GENMASK(31, 0)

/* Adaptive PDO Selection config. */
struct aps_config_t {
	/* (0, 100) coeff for transistion to a lower power PDO*/
	int k_less_pwr;
	/* (0, 100) coeff for transistion to a higpower PDO*/
	int k_more_pwr;
	/* Number for how many the same consecutive sample to transist */
	int k_sample;
	/* Power stablied time after a new contract in ms */
	int t_stable;
	/* Next power evluation time interval in ms */
	int t_check;
	/*
	 * If the current voltage is more efficient than the previous voltage
	 *
	 * @param curr_mv: current PDO voltage
	 * @param prev_mv: previous PDO voltage
	 * @param batt_mv: battery desired voltage
	 * @param batt_mw: current battery power
	 * @param input_mw: current adapter input power
	 * @return true is curr_mv is more efficient otherwise false
	 */
	bool (*is_more_efficient)(int curr_mv, int prev_mv, int batt_mv,
				  int batt_mw, int input_mw);
};

/*
 * Get adpative voltage in the current system load
 *
 * @param port: the evaluated port
 *
 * @return a voltage that the adapter supports to charge at the given port.
 */
int aps_get_adaptive_voltage(int port);

/*
 * Check if APS is enabled.
 *
 * @return true if enabled, false otherwise.
 */
bool aps_is_enabled(void);

/*
 * Enable/Disable APS
 *
 * @param en: enable/disable
 */
void aps_enable(bool en);

/*
 * Update APS stablized timeout *
 *
 * This should be called if a new power request is negotiated.
 *
 * @param port: the port for timer reset.
 */
void aps_update_stabilized_time(int port);

void aps_set_flags(int port, uint32_t flags);
void aps_clr_flags(int port, uint32_t flags);

#endif /* __CROS_EC_APS__H */
