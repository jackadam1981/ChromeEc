/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include <ap_power/ap_power.h>

__overridable void board_resume_change(struct ap_power_ev_callback *cb,
				       struct ap_power_ev_data data)
{
}

static int board_resume_change_init(void)
{
	static struct ap_power_ev_callback cb = {
		.handler = board_resume_change,
		.events = AP_POWER_RESUME,
	};
	ap_power_ev_add_callback(&cb);
	return 0;
}
SYS_INIT(board_resume_change_init, APPLICATION, 0);
