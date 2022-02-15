/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TODO: b/218904113: Convert to using Zephyr GPIOs */
#include "gpio_signal.h"
#include "common.h"
#include "console.h"
#include "intelrvp.h"
#include "power/icelake.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)

/******************************************************************************/
/* PWROK signal configuration */
/*
 * On ADLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
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

/*
 * Returns board information (board id[7:0] and Fab id[15:8]) on success
 * -1 on error.
 */
__override int board_get_version(void)
{
	/* Cache the ADLRVP board ID */
	static int adlrvp_board_id;

	int i;
	int rv = EC_ERROR_UNKNOWN;

	int fab_id, board_id, bom_id;

	/* Board ID is already read */
	if (adlrvp_board_id)
		return adlrvp_board_id;

	/*
	 * In coldboot cycling stress test board id read failures seen
	 * ADL rvp board uses ioexpander to read the board id and
	 * board ID ioex is on DSW-VAL rail and is enabled by separate
	 * VR which is taking time to settle.
	 * This loop retries to ensure rail is settled and read is successful
	 */
	for (i = 0; i < RVP_VERSION_READ_RETRY_CNT; i++) {

		rv  = gpio_pin_get_dt(&bom_id_config[0]);

		if (rv >= 0)
			break;

		k_msleep(1);
	}

	/* retrun -1 if failed to read board id */
	if (rv < 0)
		return -1;

	/*
	 * BOM ID[0:2]: PIN[14:15] | PIN[0]
	 * bit 0 = <&pca95xx 14 0>
	 * bit 1 = <&pca95xx 15 0>
	 * bit 2 = <&pca95xx 0 0>
	 */
	bom_id  = gpio_pin_get_dt(&bom_id_config[2]) << 2;
	bom_id |= gpio_pin_get_dt(&bom_id_config[1]) << 1;
	bom_id |= gpio_pin_get_dt(&bom_id_config[0]) << 0;

	/*
	 * FAB ID[0:1]: PIN[1:2] + 1
	 * bit 0 = <&pca95xx 1 0>
	 * bit 1 = <&pca95xx 2 0>
	 */
	fab_id  = gpio_pin_get_dt(&fab_id_config[1]) << 1;
	fab_id |= gpio_pin_get_dt(&fab_id_config[0]) << 0;
	fab_id += 1;

	/*
	 * BOARD ID[0:5]: PIN[8:13]
	 * bit 0 = <&pca95xx 8 0>
	 * bit 1 = <&pca95xx 9 0>
	 * bit 2 = <&pca95xx 10 0>
	 * bit 3 = <&pca95xx 11 0>
	 * bit 4 = <&pca95xx 12 0>
	 * bit 5 = <&pca95xx 13 0>
	 */
	board_id  = gpio_pin_get_dt(&board_id_config[5]) << 5;
	board_id |= gpio_pin_get_dt(&board_id_config[4]) << 4;
	board_id |= gpio_pin_get_dt(&board_id_config[3]) << 3;
	board_id |= gpio_pin_get_dt(&board_id_config[2]) << 2;
	board_id |= gpio_pin_get_dt(&board_id_config[1]) << 1;
	board_id |= gpio_pin_get_dt(&board_id_config[0]) << 0;

	CPRINTF("BID:0x%x, FID:0x%x, BOM:0x%x", board_id, fab_id, bom_id);

	adlrvp_board_id = board_id | (fab_id << 8);
	return adlrvp_board_id;
}
