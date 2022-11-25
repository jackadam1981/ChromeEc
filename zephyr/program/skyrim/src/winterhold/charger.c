/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "charger/isl923x_public.h"
#include "hooks.h"

static void charger_prochot_init(void)
{
	isl923x_set_ac_prochot(CHARGER_SOLO, CONFIG_AC_PROCHOT_CURRENT_MA);
	isl923x_set_dc_prochot(CHARGER_SOLO, CONFIG_DC_PROCHOT_CURRENT_MA);
}
DECLARE_HOOK(HOOK_INIT, charger_prochot_init, HOOK_PRIO_POST_FIRST);
