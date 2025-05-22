/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "intel_rvp_board_id.h"

#include <ap_power/ap_pwrseq_sm.h>

#define DT_DRV_COMPAT intel_rvp_board_id

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1,
	     "Unsupported RVP Board ID instance");

LOG_MODULE_REGISTER(board_id_intel_rvp, LOG_LEVEL_INF);

#define RVP_ID_GPIO_DT_SPEC_GET(idx, node_id, prop) \
	GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx),

#define RVP_ID_CONFIG_LIST(node_id, prop)                                \
	LISTIFY(DT_PROP_LEN(node_id, prop), RVP_ID_GPIO_DT_SPEC_GET, (), \
		node_id, prop)

#if RVP_ID_HAS_BOM_GPIOS
const struct gpio_dt_spec bom_gpios_config[] = { RVP_ID_CONFIG_LIST(
	DT_DRV_INST(0), bom_gpios) };
#endif

#if RVP_ID_HAS_FAB_GPIOS
const struct gpio_dt_spec fab_gpios_config[] = { RVP_ID_CONFIG_LIST(
	DT_DRV_INST(0), fab_gpios) };
#endif

const struct gpio_dt_spec board_gpios_config[] = { RVP_ID_CONFIG_LIST(
	DT_DRV_INST(0), board_gpios) };

struct rvp_board_id_config {
	int deferred_init_driver;
};

/*
 * Returns board information (board id[7:0] and Fab id[15:8]) on success
 * -1 on error.
 */
__override int board_get_version(void)
{
	/* Cache the board ID */
	static int rvp_board_id;

	int board_id = -1;
#if RVP_ID_HAS_BOM_GPIOS
	int bom_id = -1;
#endif
#if RVP_ID_HAS_FAB_GPIOS
	int fab_id = -1;
#endif
	/* Board ID is already read */
	if (rvp_board_id)
		return rvp_board_id;

	if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(pca95xx_0)))) {
		LOG_ERR("PCA95XX ioexpander not initialized cannot access it");
		return -1;
	}

	/*
	 * BOARD ID[5:0] : IOEX[13:8]
	 */
	board_id = gpio_pin_get_dt(&board_gpios_config[0]);
	board_id |= gpio_pin_get_dt(&board_gpios_config[1]) << 1;
	board_id |= gpio_pin_get_dt(&board_gpios_config[2]) << 2;
	board_id |= gpio_pin_get_dt(&board_gpios_config[3]) << 3;
	board_id |= gpio_pin_get_dt(&board_gpios_config[4]) << 4;
	board_id |= gpio_pin_get_dt(&board_gpios_config[5]) << 5;

	rvp_board_id = board_id;

	LOG_INF("BOARD_ID:0x%x", board_id);

#if RVP_ID_HAS_BOM_GPIOS
	/*
	 * BOM ID [2]   : IOEX[0]
	 * BOM ID [1:0] : IOEX[15:14]
	 */
	bom_id = gpio_pin_get_dt(&bom_gpios_config[0]);
	bom_id |= gpio_pin_get_dt(&bom_gpios_config[1]) << 1;
	bom_id |= gpio_pin_get_dt(&bom_gpios_config[2]) << 2;
	LOG_INF("BOM_ID:0x%x", bom_id);
#endif

#if RVP_ID_HAS_FAB_GPIOS
	/*
	 * FAB ID [1:0] : IOEX[2:1] + 1
	 */
	fab_id = gpio_pin_get_dt(&fab_gpios_config[0]);
	fab_id |= gpio_pin_get_dt(&fab_gpios_config[1]) << 1;
	fab_id += 1;

	LOG_INF("FAB_ID:0x%x", fab_id);
	rvp_board_id |= (fab_id << 8);
#endif

	return rvp_board_id;
}

static void pca95xx_deferred_init_cb(const struct device *dev,
				     const enum ap_pwrseq_state entry,
				     const enum ap_pwrseq_state exit)
{
	const struct device *pca95xx;

	if (exit == AP_POWER_STATE_G3) {
		LOG_INF("S5 callback triggered, when exiting G3");
		pca95xx = DEVICE_DT_GET(DT_NODELABEL(pca95xx_0));
		if (!device_is_ready(pca95xx)) {
			LOG_INF("Initializing PCA95XX Ioexpander");
			device_init(pca95xx);
		}
	}
}

static int rvp_board_id_init(const struct device *dev)
{
	const struct rvp_board_id_config *config = dev->config;

	if (config->deferred_init_driver) {
		static struct ap_pwrseq_state_callback ap_pwrseq_entry_cb;
		const struct device *ap_pwrseq_dev = ap_pwrseq_get_instance();

		LOG_INF("setup_pca95xx_init_callback");
		ap_pwrseq_entry_cb.cb = pca95xx_deferred_init_cb;
		ap_pwrseq_entry_cb.states_bit_mask = BIT(AP_POWER_STATE_S5);

		ap_pwrseq_register_state_entry_callback(ap_pwrseq_dev,
							&ap_pwrseq_entry_cb);
	}

	return 0;
}

static const struct rvp_board_id_config rvp_board_id_cfg = {
	.deferred_init_driver =
		DT_NODE_HAS_PROP(DT_DRV_INST(0), deferred_init_driver),
};

DEVICE_DT_INST_DEFINE(0, rvp_board_id_init, NULL, NULL, &rvp_board_id_cfg,
		      POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);
