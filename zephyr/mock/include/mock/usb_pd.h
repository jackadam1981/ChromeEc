/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_pd.h"
#include <zephyr/ztest.h>
#include <zephyr/fff.h>

DECLARE_FAKE_VALUE_FUNC(enum pd_power_role, pd_get_power_role, int);