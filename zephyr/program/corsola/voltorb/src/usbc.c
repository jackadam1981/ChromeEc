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
#include "tcpm/tcpm.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

static const uint32_t pd_src_pdo_1A5[] = {
    PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};

static const uint32_t pd_src_pdo_3A[] = {
    PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
};

static bool current_limited = false;

int dpm_get_source_pdo(const uint32_t **src_pdo, const int port)
{
	int pdo_cnt = 0;

	if (current_limited) {
        *src_pdo = pd_src_pdo_1A5;
		pdo_cnt = ARRAY_SIZE(pd_src_pdo_1A5);
        CPRINTS("\n---get 1.5A pdo--\n");
    } else {
        *src_pdo = pd_src_pdo_3A;
		pdo_cnt = ARRAY_SIZE(pd_src_pdo_3A);
        CPRINTS("\n---get 3A pdo--\n");
    }

	return pdo_cnt;
}

static void limit_output_current(void)
{
	int i,tc_state;

	CPRINTS("--- battery %d%%", charge_get_percent());
	if (charge_get_percent() < 30)
	{
		current_limited = true;
		for (i = 0; i < board_get_usb_pd_port_count(); i++) {
			tc_state = tc_is_attached_src(i);
			CPRINTS("--- port%d source = %d", i, tc_state);

			if (tc_state) {
				CPRINTS("--- suspend 1A5");
				pd_update_contract(i);
			}
		}
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, limit_output_current, HOOK_PRIO_DEFAULT);

static void resume_output_current(void)
{
	int i,tc_state;

	current_limited = false;

	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		tc_state = tc_is_attached_src(i);
		CPRINTS("--- port%d source = %d", i, tc_state);

		if (tc_state) {
			CPRINTS("--- resume 3A0");
			pd_update_contract(i);
		}
	}
	CPRINTS("--- run resume");
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, resume_output_current, HOOK_PRIO_DEFAULT);
