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
	int i;
	int max_uw = 0;
	int max_i = -1;

	/* Get max power */
	for (i = 0; i < cnt; i++) {
		int uw;
		if ((src_caps[i] & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
			uw = 250000 * (src_caps[i] & 0x3FF);
		} else {
			int ma = src_caps[i] & 0x3FF;
			int mv = (src_caps[i] >> 10) & 0x3FF;
			uw = ma * mv;
		}
		if (uw > max_uw) {
			max_i = i;
			max_uw = uw;
		}
	}
	if (max_i < 0)
		return EC_ERROR_UNKNOWN;

	/* request all the power ... */
	if ((src_caps[max_i] & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
		int uw = 250000 * (src_caps[i] & 0x3FF);
		*rdo = RDO_BATT(max_i + 1, uw/2, uw, 0);
		ccprintf("Request [%d] %d/%d mW\n", max_i, uw/1000, uw/1000);
	} else {
		int ma = 10 * (src_caps[max_i] & 0x3FF);
		*rdo = RDO_FIXED(max_i + 1, ma / 2, ma, 0);
		ccprintf("Request [%d] %d mA\n", max_i, ma/2, ma);
	}
	return EC_SUCCESS;
}

int pd_request_voltage(void *ctxt, uint32_t rdo)
{
	int op_ma = rdo & 0x3FF;
	int max_ma = (rdo >> 10) & 0x3FF;
	int idx = rdo >> 28;
	uint32_t pdo;
	uint32_t pdo_ma;

	if (idx > pd_src_pdo_cnt)
		return EC_ERROR_INVAL; /* Invalid index */

	/* check current ... */
	pdo = pd_src_pdo[idx - 1];
	pdo_ma = (pdo & 0x3ff);
	if (op_ma > pdo_ma)
		return EC_ERROR_INVAL; /* too much op current */
	if (max_ma > pdo_ma)
		return EC_ERROR_INVAL; /* too much max current */

	ccprintf("Switch to %d V %d mA (for %d/%d mA)\n",
		 ((pdo >> 10) & 0x3ff) * 50, (pdo & 0x3ff) * 10,
		 ((rdo >> 10) & 0x3ff) * 10, (rdo & 0x3ff) * 10);

	/* trigger a PS_RDY after a 50-ms fake delay */
	last_ctxt = ctxt;
	hook_call_deferred(fake_voltage_transition, 50000);

	return EC_SUCCESS;
}

void pd_power_supply_reset(void)
{
}
