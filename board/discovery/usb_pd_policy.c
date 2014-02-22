/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

const uint32_t pd_src_pdo[] = {
		PDO_FIXED(5000,   500, PDO_FIXED_EXTERNAL),
		PDO_FIXED(5000,  3000, 0),
		PDO_FIXED(12000, 3000, 0),
		PDO_FIXED(20000, 2000, 0),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

const uint32_t pd_snk_pdo[] = {
		PDO_BATT(4500,   5500, 15000),
		PDO_BATT(11500, 12500, 36000),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

static void *last_ctxt;

static void fake_voltage_transition(void)
{
	if (last_ctxt)
		pd_power_supply_ready(last_ctxt);
}
DECLARE_DEFERRED(fake_voltage_transition);

int pd_choose_voltage(int cnt, uint32_t *src_caps, uint32_t *rdo)
{
	*rdo = RDO_FIXED(1, 500, 500, 0);

	return EC_SUCCESS;
}

int pd_request_voltage(void *ctxt, uint32_t rdo)
{
	if ((rdo >> 28) >= pd_src_pdo_cnt)
		return EC_ERROR_INVAL; /* Invalid index */

	/*TODO check current ... */

	/* trigger a PS_RDY after a 50-ms fake delay */
	last_ctxt = ctxt;
	hook_call_deferred(fake_voltage_transition, 50000);

	return EC_SUCCESS;
}

void pd_power_supply_reset(void)
{
}
