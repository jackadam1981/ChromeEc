/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chipset interface APIs from subsys/ap_pwrseq */

#include "common.h"
#include "zephyr/subsys/ap_pwrseq/include/chipset.h"

int chipset_in_state(int state_mask)
{
	if (!IS_ENABLED(CONFIG_AP_PWRSEQ))
		return 0;

	return ap_pwrseq_chipset_in_state(state_mask);
}

int chipset_in_or_transitioning_to_state(int state_mask)
{
	if (!IS_ENABLED(CONFIG_AP_PWRSEQ))
		return 0;

	return ap_pwrseq_chipset_in_or_transitioning_to_state(state_mask);
}

void chipset_exit_hard_off(void)
{
	if (!IS_ENABLED(CONFIG_AP_PWRSEQ))
		return;

	ap_pwrseq_chipset_exit_hard_off();
}

/* TODO:
 * To be added later when this functionality is implemented in ap_pwrseq.
 */
void chipset_throttle_cpu(int throttle) { }
