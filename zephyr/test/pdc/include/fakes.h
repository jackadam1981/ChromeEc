/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"

#include <zephyr/fff.h>

DECLARE_FAKE_VALUE_FUNC(int, battery_design_voltage, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, battery_remaining_capacity, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, battery_status, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, battery_design_capacity, uint32_t *);
DECLARE_FAKE_VALUE_FUNC(int, battery_full_charge_capacity, uint32_t *);

void set_battery_present(enum battery_present bp);

