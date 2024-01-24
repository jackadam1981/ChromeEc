/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h"
#include "hooks.h"
#include "typec_control.h"
#include "usb_pd.h"
#include "usb_pd_dpm_sm.h"
#include "usb_tc_sm.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ##args)

#define PDO_FIXED_FLAGS \
	(PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)

static bool current_limited;
static bool port_status[2];

static const uint32_t pd_src_pdo_1A5[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};

static const uint32_t pd_src_pdo_3A[] = {
	PDO_FIXED(5000, 3000, PDO_FIXED_FLAGS),
};

int dpm_get_source_pdo(const uint32_t **src_pdo, const int port)
{
	if (current_limited) {
		*src_pdo = pd_src_pdo_1A5;
		return ARRAY_SIZE(pd_src_pdo_1A5);
	}

	*src_pdo = pd_src_pdo_3A;

	return ARRAY_SIZE(pd_src_pdo_3A);
}

static void check_src_port(void)
{
	int i;

	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		port_status[i] = tc_is_attached_src(i);
	}

	if (port_status[0] && !port_status[1]) {
		pd_update_contract(0);
		current_limited = false;
		CPRINTS("###C0 port 5V3A");
	} else if (!port_status[0] && port_status[1]) {
		pd_update_contract(1);
		current_limited = false;
		CPRINTS("###C1 port 5V3A");
	} else {
		current_limited = true;
		pd_update_contract(0);
		pd_update_contract(1);
		CPRINTS("###C0/C1 port 5V31.5");
	}
}
DECLARE_HOOK(HOOK_USB_PD_CONNECT, check_src_port, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_USB_PD_DISCONNECT, check_src_port, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_POWER_SUPPLY_CHANGE, check_src_port, HOOK_PRIO_DEFAULT);
