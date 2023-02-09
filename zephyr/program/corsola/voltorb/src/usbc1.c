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

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

static void limit_discharge_current(void)
{
    int i,tc_state,rp;
    const struct batt_params *batt = charger_current_battery_params();

    CPRINTS("--- battery %d%%", batt->state_of_charge);	
	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		tc_state = tc_is_attached_src(i);
		CPRINTS("--- tc%d = %d", i, tc_state);
        if (tc_state)
        {
            rp = typec_get_default_current_limit_rp(i);
            CPRINTS("--- suspend default rp = %d", rp);
            // if (rp == TYPEC_RP_3A0)
            if (1)
            {
                rp = TYPEC_RP_1A5;
                CPRINTS("--- suspend set current limit");
                typec_set_source_current_limit(i, rp);
            }                       
        }
	}
    CPRINTS("--- limit_discharge_current");
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, limit_discharge_current, HOOK_PRIO_LAST);

static void resume_discharge_current(void)
{
    int i,rp;

    for (i = 0; i < board_get_usb_pd_port_count(); i++) {
        rp = typec_get_default_current_limit_rp(i);
        CPRINTS("--- resume current limit rp=%d", rp);
        typec_set_source_current_limit(i, rp);

	}
    CPRINTS("--- resume_discharge_current");
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, resume_discharge_current, HOOK_PRIO_DEFAULT - 1);