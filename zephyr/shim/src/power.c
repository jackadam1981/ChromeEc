/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/util.h>

#include "console.h"
#include "power.h"
#include "power/power.h"

/* power signal list.  Must match order of enum power_signal. */

#if (SYSTEM_DT_NODE_POWER_SIGNAL_CONFIG)

const struct power_signal_info power_signal_list[] = {
	DT_FOREACH_STATUS_OKAY(named_gpios, GEN_POWER_SIGNAL_ENUM_PARENT)
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

#elif defined(CONFIG_AP_ARM_MTK_MT8192)

#warning "enum power_signal should use DT"
const struct power_signal_info power_signal_list[] = {
	{GPIO_PMIC_EC_PWRGD, POWER_SIGNAL_ACTIVE_HIGH, "PMIC_PWR_GOOD"},
	{GPIO_AP_IN_SLEEP_L, POWER_SIGNAL_ACTIVE_LOW, "AP_IN_S3_L"},
	{GPIO_AP_EC_WATCHDOG_L, POWER_SIGNAL_ACTIVE_LOW, "AP_WDT_ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

#endif
