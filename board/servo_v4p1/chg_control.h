/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CHG_CONTROL_H
#define __CROS_EC_CHG_CONTROL_H

#include <stdbool.h>

enum chg_cc_t {
	CHG_OPEN,
	CHG_CC1,
	CHG_CC2
};

enum chg_power_select_t {
	CHG_POWER_OFF,
	CHG_POWER_PP5000,
	CHG_POWER_VBUS,
};

/*
 * Triggers a disconnect and reconnect on the DUT Charger port
 */
void chg_reset(void);

/*
 * Disables or selects the DUT Charger Power source
 *
 * @param type Power source used for DUT
 */
void chg_power_select(enum chg_power_select_t type);

/*
 * Removes or attaches the DUT Charger Ports CC1 and CC2 Rd resistors
 *
 * @param cc CC1 or CC2 RD to remove or attach
 * @param di false the CC RD is attached else it is removed
 */
void chg_vbus_disable(enum chg_cc_t cc, bool di);

#endif /* __CROS_EC_CHG_CONTROL_H */
