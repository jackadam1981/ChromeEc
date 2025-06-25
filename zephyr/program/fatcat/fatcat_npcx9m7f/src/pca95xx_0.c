/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "gpio.h"
#include "hooks.h"
#include "intel_rvp_board_id.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <ap_power/ap_pwrseq_sm.h>
#include <power_signals.h>

LOG_MODULE_REGISTER(pca95xx_0, LOG_LEVEL_INF);

static void get_fab_bom_id(void)
{
	static bool setup_completed = false;
	int rvp_board_id;
	int rvp_bom_id;
	int rvp_fab_id;
	uint32_t model_id;
	int rv;

	if (setup_completed)
		return;

	rv = cbi_get_model_id(&model_id);
	if (rv == EC_SUCCESS) {
		/*
		 * Model ID is already set, don't change it.
		 */
		return;
	}

	/*
	 * BOARD ID[5:0] : IOEX[13:8]
	 */
	rvp_board_id = gpio_pin_get_dt(&board_id_config[0]);
	rvp_board_id |= gpio_pin_get_dt(&board_id_config[1]) << 1;
	rvp_board_id |= gpio_pin_get_dt(&board_id_config[2]) << 2;
	rvp_board_id |= gpio_pin_get_dt(&board_id_config[3]) << 3;
	rvp_board_id |= gpio_pin_get_dt(&board_id_config[4]) << 4;
	rvp_board_id |= gpio_pin_get_dt(&board_id_config[5]) << 5;
	LOG_INF("RVP BOARD_ID: 0x%02x", rvp_board_id);

	/*
	 * BOM ID [2]   : IOEX[0]
	 * BOM ID [1:0] : IOEX[15:14]
	 */
	rvp_bom_id = gpio_pin_get_dt(&bom_id_config[0]);
	rvp_bom_id |= gpio_pin_get_dt(&bom_id_config[1]) << 1;
	rvp_bom_id |= gpio_pin_get_dt(&bom_id_config[2]) << 2;
	LOG_INF("RVP BOM_ID: 0x%02x", rvp_bom_id);

	/*
	 * FAB ID [1:0] : IOEX[2:1] + 1
	 */
	rvp_fab_id = gpio_pin_get_dt(&fab_id_config[0]);
	rvp_fab_id |= gpio_pin_get_dt(&fab_id_config[1]) << 1;
	rvp_fab_id += 1;

	LOG_INF("RVP FAB_ID: 0x%02x", rvp_fab_id);

	/*
	 * NOTE:
	 * The original PTL RVP board ID does not include the BOM ID bits.
	 */
	model_id = (rvp_bom_id << 16) | (rvp_fab_id << 8) | rvp_board_id;

	rv = cbi_set_model_id(model_id);
	if (rv == EC_RES_ACCESS_DENIED) {
		LOG_ERR("system is locked: Can not set MODEL_ID in CBI");
	}

	setup_completed = true;
}

static void pca95xx_deferred_init_cb(const struct device *dev,
				     const enum ap_pwrseq_state entry,
				     const enum ap_pwrseq_state exit)
{
	const struct device *pca95xx;

	if (exit == AP_POWER_STATE_G3) {
		LOG_INF("G3 to S5 callback triggered");
		pca95xx = DEVICE_DT_GET(DT_NODELABEL(pca95xx_0));
		if (!device_is_ready(pca95xx)) {
			LOG_INF("Initializing pca95xx_0");
			device_init(pca95xx);
			get_fab_bom_id();
		}
	}
}

static int setup_pca95xx_init_callback(void)
{
	static struct ap_pwrseq_state_callback ap_pwrseq_entry_cb;
	const struct device *ap_pwrseq_dev = ap_pwrseq_get_instance();

	ap_pwrseq_entry_cb.cb = pca95xx_deferred_init_cb;
	ap_pwrseq_entry_cb.states_bit_mask = BIT(AP_POWER_STATE_S5);

	ap_pwrseq_register_state_entry_callback(ap_pwrseq_dev,
						&ap_pwrseq_entry_cb);

	return 0;
}
SYS_INIT(setup_pca95xx_init_callback, POST_KERNEL,
	 CONFIG_APPLICATION_INIT_PRIORITY);
