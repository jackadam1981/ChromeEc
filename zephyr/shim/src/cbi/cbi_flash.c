/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define DT_DRV_COMPAT cros_ec_flash_layout

#include "console.h"
#include "cros_board_info.h"
#include "flash.h"
#include "write_protect.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cbi_flash, LOG_LEVEL_ERR);

#define WRITE_PROTECTED_READ_ONLY_FLASH_NODE DT_NODELABEL(wp_ro)
#define CBI_FLASH_NODE DT_NODELABEL(cbi_flash)

BUILD_ASSERT(DT_NODE_EXISTS(WRITE_PROTECTED_READ_ONLY_FLASH_NODE) == 1,
	     "Write protected read only flash DT node label not found");

BUILD_ASSERT(DT_NODE_EXISTS(CBI_FLASH_NODE) == 1,
	     "CBI flash DT node label not found");

#define CBI_FLASH_OFFSET DT_PROP(CBI_FLASH_NODE, offset)
#define CBI_FLASH_SIZE DT_PROP(CBI_FLASH_NODE, size)
#define CBI_FLASH_PRESERVE DT_PROP(CBI_FLASH_NODE, preserve)

static int flash_load(uint8_t offset, uint8_t *data, int len)
{
	return crec_flash_physical_read(offset, len, (char *)&data);
}

static int flash_is_write_protected(void)
{
	return CBI_FLASH_PRESERVE;
}

static int flash_store(uint8_t *cbi)
{
	if (CBI_FLASH_SIZE) {
		return crec_flash_physical_write(
			CBI_FLASH_OFFSET, CBI_FLASH_SIZE, (const char *)cbi);
	}
	return 0;
}

static const struct cbi_storage_driver flash_drv = {
	.store = flash_store,
	.load = flash_load,
	.is_protected = flash_is_write_protected,
};

const struct cbi_storage_config_t cbi_config = {
	.storage_type = CBI_STORAGE_TYPE_FLASH,
	.drv = &flash_drv,
};
