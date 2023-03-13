/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/charger/isl9241.h"
#include "hooks.h"
#include "util.h"

static void charger_set_frequence_to_600KHZ(void)
{
	charger_set_frequence(ISL9241_CONTROL1_SWITCHING_FREQ_600KHZ);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, charger_set_frequence_to_600KHZ,
	     HOOK_PRIO_DEFAULT);

static void charger_set_frequence_to_1020KHZ(void)
{
	charger_set_frequence(ISL9241_CONTROL1_SWITCHING_FREQ_1020KHZ);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, charger_set_frequence_to_1020KHZ,
	     HOOK_PRIO_DEFAULT);
