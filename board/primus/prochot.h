/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Primus prochot configuration */

#ifndef __CROS_EC_PROCHOT_H
#define __CROS_EC_PROCHOT_H

struct battery_para {
	int battery_continue_discharge_wattage;
	int battery_design_wattage;
    int state_of_charge;
};

#endif /* __CROS_EC_PROCHOT_H */
