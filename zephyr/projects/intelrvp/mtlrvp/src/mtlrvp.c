/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "gpio.h"
#include "i2c.h"
#include "intelrvp.h"
#include "ioexpander.h"
#include "power/meteorlake.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)

/* TODO: import pca9555.h into zephyr code base to get this value */
#define PCA9555_CMD_INPUT_PORT_0 0
/******************************************************************************/
/* PWROK signal configuration */
/*
 * On MTLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
 * as input.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_PCH_SYS_PWROK,
		.delay_ms = 3,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	{
		.gpio = GPIO_PCH_SYS_PWROK,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

__override int board_get_version(void)
{
	/* Cache the MTLRVP board ID */
	static int mtlrvp_board_id;

	int port, i;
	int rv = EC_ERROR_UNKNOWN;
	int fab_id, board_id, bom_id;

	/* Board ID is already read */
	if (mtlrvp_board_id)
		return mtlrvp_board_id;

	for (i = 0; i < RVP_VERSION_READ_RETRY_CNT; i++) {
		rv = i2c_read16(I2C_PORT_BOARD_ID_GPIO,
				I2C_ADDR_BOARD_ID_GPIO,
				PCA9555_CMD_INPUT_PORT_0,
				&port);
		if (!rv)
			break;

		k_msleep(1);
	}

	/* return -1 if failed to read board id */
	if (rv)
		return -1;
	/*
	 * bit 0     -  BOM ID(2)
	 * bit 2:1   -  FAB ID(1:0) + 1
	 * bit 15:14 -  BOM ID(1:0)
	 * bit 13:8  -  BOARD ID(5:0)
	 */
	bom_id = ((port & 0xC000) >> 14) | ((port & 0x0001) << 2);
	fab_id = ((port & 0x0006) >> 1) + 1;
	board_id = (port & 0x3F00) >> 8;

	CPRINTS("BID:0x%x, FID:0x%x, BOM:0x%x", board_id, fab_id, bom_id);

	mtlrvp_board_id = board_id | (fab_id << 8);
	return mtlrvp_board_id;
}
