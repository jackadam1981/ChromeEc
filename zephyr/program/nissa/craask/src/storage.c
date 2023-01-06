/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "fan.h"
#include "gpio/gpio.h"
#include "hooks.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

#define STORAGE_EMMC (0 << 30)
#define STORAGE_NVME (1 << 30)
#define STORAGE_MASK 0xC0000000

static void board_storage_control(void)
{
	int ret;
	int storge_det;
	uint32_t val;
	uint32_t fw_config;

	ret = cros_cbi_get_fw_config(FW_STORAGE, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_STORAGE);
		return;
	}

	/*
	 * If both masks are enabled or disabled, read the EMMC_DET pin
	 * (should happen only in the factory).
	 */

	storge_det = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_emmc_det));

	ret = cbi_get_fw_config(&fw_config);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FW_STORAGE);
		return;
	}
	fw_config &= ~STORAGE_MASK;

	LOG_ERR("CBI: Detected eMMC SKU, disabling NVMe");

	/* 0 = eMMC SKU, 1 = NVMe SKU */
	if (storge_det == FW_STORAGE_EMMC) {
		LOG_ERR("CBI: Detected eMMC SKU, disabling NVMe");
		fw_config |= STORAGE_EMMC;
	} else {
		LOG_ERR("CBI: Detected NVMe SKU, disabling eMMC");
		fw_config |= STORAGE_NVME;

	LOG_ERR("CBI FW_CONFIG field %x", fw_config);

	/*
	 * Retrieve the storge config.
	 */

	cbi_set_board_info(CBI_TAG_FW_CONFIG, (uint8_t *)&fw_config,
		   sizeof(fw_config));
	}
}
DECLARE_HOOK(HOOK_INIT, board_storage_control, HOOK_PRIO_POST_FIRST);
