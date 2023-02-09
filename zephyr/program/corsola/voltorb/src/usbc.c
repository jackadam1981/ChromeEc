/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "charge_state.h"
#include "usb_tc_sm.h"
#include "usb_pd.h"
#include "typec_control.h"
#include "usbc_ppc.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

static void limit_output_current(void)
{
	int i,tc_state;

	CPRINTS("--- battery %d%%", charge_get_percent());
	if (charge_get_percent() < 30)
	{
		for (i = 0; i < board_get_usb_pd_port_count(); i++) {
			tc_state = tc_is_attached_src(i);
			CPRINTS("--- port%d source = %d", i, tc_state);

			if (tc_state) {
				CPRINTS("--- suspend set output current 1A5");
				typec_select_src_current_limit_rp(i,
						 TYPEC_RP_1A5);
				typec_update_cc(i);
			}
		}
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, limit_output_current, HOOK_PRIO_LAST);

static void resume_output_current(void)
{
	int i,tc_state;

	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		tc_state = tc_is_attached_src(i);
		CPRINTS("--- port%d source = %d", i, tc_state);

		if (tc_state) {
			CPRINTS("--- resume set output current 3A0");
			typec_select_src_current_limit_rp(i, TYPEC_RP_3A0);
			typec_update_cc(i);
		}
	}
	CPRINTS("--- run resume");
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, resume_output_current, HOOK_PRIO_DEFAULT - 1);
