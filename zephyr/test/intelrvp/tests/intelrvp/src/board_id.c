/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/emul_ap_callbacks.h"
#include "fakes.h"
#include "intel_rvp_board_id.h"
#include "intelrvp.h"
#include "system.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <ap_power/ap_pwrseq.h>

#define NUM_BOM_GPIOS DT_PROP_LEN(DT_INST(0, intel_rvp_board_id), bom_gpios)
#define NUM_FAB_GPIOS DT_PROP_LEN(DT_INST(0, intel_rvp_board_id), fab_gpios)
#define NUM_BOARD_GPIOS DT_PROP_LEN(DT_INST(0, intel_rvp_board_id), board_gpios)

static void configure_board_id_gpios_input(void)
{
	for (int i = 0; i < NUM_BOM_GPIOS; i++)
		gpio_pin_configure(bom_id_config[i].port, bom_id_config[i].pin,
				   (GPIO_INPUT | GPIO_ACTIVE_HIGH));

	for (int i = 0; i < NUM_FAB_GPIOS; i++)
		gpio_pin_configure(fab_id_config[i].port, fab_id_config[i].pin,
				   (GPIO_INPUT | GPIO_ACTIVE_HIGH));

	for (int i = 0; i < NUM_BOARD_GPIOS; i++)
		gpio_pin_configure(board_id_config[i].port,
				   board_id_config[i].pin,
				   (GPIO_INPUT | GPIO_ACTIVE_HIGH));
}

static void send_ap_pwr_callback(void)
{
	const struct device *dev = ap_pwrseq_get_instance();

	ap_pwrseq_emul_entry_callback(dev, AP_POWER_STATE_S5,
				      AP_POWER_STATE_G3);
}

static void test_set_board_id_gpios(void)
{
	configure_board_id_gpios_input();
	gpio_emul_input_set(bom_id_config[0].port, bom_id_config[0].pin, 0);
	gpio_emul_input_set(bom_id_config[1].port, bom_id_config[1].pin, 1);
	gpio_emul_input_set(bom_id_config[2].port, bom_id_config[2].pin, 0);
	/*
	 * FAB ID [1:0] : IOEX[2:1] + 1
	 */
	gpio_emul_input_set(fab_id_config[0].port, fab_id_config[0].pin, 1);
	gpio_emul_input_set(fab_id_config[1].port, fab_id_config[1].pin, 1);

	/*
	 * BOARD ID[5:0] : IOEX[13:8]
	 */
	gpio_emul_input_set(board_id_config[0].port, board_id_config[0].pin, 0);
	gpio_emul_input_set(board_id_config[1].port, board_id_config[1].pin, 1);
	gpio_emul_input_set(board_id_config[2].port, board_id_config[2].pin, 1);
	gpio_emul_input_set(board_id_config[3].port, board_id_config[3].pin, 0);
	gpio_emul_input_set(board_id_config[4].port, board_id_config[4].pin, 1);
	gpio_emul_input_set(board_id_config[5].port, board_id_config[5].pin, 1);

	return;
}

ZTEST(board_version_tests, test_board_get_version)
{
	/* PTL requires AP callback to enable PCA95XX */
	send_ap_pwr_callback();
	/* Set up fake return values for successful GPIO pin reading */
	int expected_board_id = CONFIG_TEST_PROJECT_INTELRVP_BOARD_ID_VAL;

	/* Verification: Correct version is computed and returned */
	int version = board_get_version();

	zassert_equal(
		expected_board_id, version,
		"Expected version didn't match actual version. Expected: %d, Actual: %d",
		expected_board_id, version);
}

/* Board ID gpios need to be initialized before other applications */
SYS_INIT(test_set_board_id_gpios, POST_KERNEL, 99);
#ifdef CONFIG_TEST_PROJECT_PTLRVP_MCHP
ZTEST(board_version_tests, pca95xx_not_init_test)
{
	int expected_board_id = CONFIG_TEST_PROJECT_INTELRVP_BOARD_ID_VAL;
	int version = board_get_version();

	zassert_not_equal(
		expected_board_id, version,
		"Expected versions match PCA95XX shouldn't be initialized");
}
#endif

/* Test Suite Setup */
ZTEST_SUITE(board_version_tests, NULL, NULL, NULL, NULL, NULL);
