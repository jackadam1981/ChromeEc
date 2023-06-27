/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger_profile_override.h"

#include <zephyr/ztest.h>

struct charge_state_data curr;

ZTEST(temp_current, test_placeholder)
{
	charger_profile_override(&curr);
}

ZTEST_SUITE(temp_current, NULL, NULL, NULL, NULL, NULL);
