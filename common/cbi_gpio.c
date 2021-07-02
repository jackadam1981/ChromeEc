/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Support Cros Board Info GPIO */

#include "console.h"
#include "cros_board_info.h"
#include "gpio.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "CBI " format, ## args)

__overridable int board_get_sku_id(void)
{
	return -1;
}

static int gpio_read(uint8_t offset, uint8_t *data, int len)
{
	int board_id = -1;
	int sku_id = -1;

	if (cbi_get_cache_status() == CBI_CACHE_STATUS_SYNCED)
		return EC_SUCCESS;

#if defined(CONFIG_BOARD_VERSION_CUSTOM)
	board_id = board_get_version();
#elif defined(CONFIG_BOARD_VERSION_GPIO)
	board_id = (!!gpio_get_level(GPIO_BOARD_VERSION1) << 0) |
		   (!!gpio_get_level(GPIO_BOARD_VERSION2) << 1) |
		   (!!gpio_get_level(GPIO_BOARD_VERSION3) << 2);
#endif
	sku_id = board_get_sku_id();

	cbi_create();

	if (board_id != -1) {
		cbi_set_board_info(CBI_TAG_BOARD_VERSION, (uint8_t *)&board_id,
				   sizeof(int));
	}

	if (sku_id != -1) {
		cbi_set_board_info(CBI_TAG_SKU_ID, (uint8_t *)&sku_id,
				   sizeof(int));
	}

	return EC_SUCCESS;
}

static int gpio_is_write_protected(void)
{
	/*
	 * When CBI comes from strapping pins, any attempts for updating CBI
	 * storage should be rejected.
	 */
	return 1;
}

const struct cbi_storage_driver gpio_drv = {
	.load = gpio_read,
	.is_protected = gpio_is_write_protected,
};

const struct cbi_storage_config_t cbi_config = {
	.storage_type = CBI_STORAGE_TYPE_GPIO,
	.drv = &gpio_drv,
};
