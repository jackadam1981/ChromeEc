/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

const uint32_t pd_src_pdo[] = {
		PDO_FIXED(5000,  1500, PDO_FIXED_EXTERNAL),
		PDO_FIXED(20000, 3000, PDO_FIXED_EXTERNAL),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

const uint32_t pd_snk_pdo[] = {
		PDO_BATT(4500,   5500, 15000),
		PDO_BATT(11500, 12500, 36000),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

/* Cap on the max voltage requested as a sink (in millivolts) */
static unsigned max_mv = -1; /* default 5V selection */

int pd_choose_voltage(int cnt, uint32_t *src_caps, uint32_t *rdo)
{
	int i;
	int sel_mv;
	int select_i = 1;
	int ma = 1500; /* request 1.5A */
	int max_ma;

	/* Get the requested voltage */
	if (max_mv > 0)
		for (i = 0; i < cnt; i++) {
			int mv = ((src_caps[i] >> 10) & 0x3FF) * 50;
			if (mv == max_mv) {
				select_i = i;
				break;
			}
		}

	if (select_i < cnt)
		return -EC_ERROR_UNKNOWN;

	/* we do not deal with battery powered source for that simple mode */
	if ((src_caps[select_i] & PDO_TYPE_MASK) == PDO_TYPE_BATTERY)
		return -EC_ERROR_INVAL;

	/* cap requested current */
	max_ma = (src_caps[select_i] & 0x3FF) * 10;
	if (ma > max_ma)
		ma = max_ma;

	sel_mv = ((src_caps[select_i] >> 10) & 0x3FF) * 50;
	*rdo = RDO_FIXED(select_i, ma, ma, 0);
	ccprintf("Request [%d] %dV %d mA\n",
		 select_i, sel_mv/1000, ma);

	return EC_SUCCESS;
}

void pd_set_max_voltage(unsigned mv)
{
	max_mv = mv;
}

int pd_request_voltage(uint32_t rdo)
{
	int op_ma = rdo & 0x3FF;
	int max_ma = (rdo >> 10) & 0x3FF;
	int idx = rdo >> 28;
	uint32_t pdo;
	uint32_t pdo_ma;

	if (!idx || idx > pd_src_pdo_cnt)
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

	return EC_SUCCESS;
}

int pd_set_power_supply_ready(void)
{
	/* provide VBUS */
	gpio_set_level(GPIO_USB_C_5V_EN, 1);

	return EC_SUCCESS; /* we are ready */
}

void pd_power_supply_reset(void)
{
	/* Kill VBUS */
	gpio_set_level(GPIO_USB_C_5V_EN, 0);
}

int pd_board_checks(void)
{
	return EC_SUCCESS;
}
