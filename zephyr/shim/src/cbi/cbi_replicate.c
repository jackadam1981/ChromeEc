/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cbi_replicate.h"
#include "cros_board_info.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cbi_transfer, LOG_LEVEL_ERR);

static bool is_cbi_data_empty(uint8_t *data, int len)
{
	for (int i = 0; i < len; i++) {
		if (data[i] != 0xFF) {
			return false;
		}
	}
	return true;
}

void cros_cbi_replicate_data_from_eeprom_to_flash(void)
{
	uint8_t cbi_data[CBI_IMAGE_SIZE];

	flash_cbi_config.drv->load(0, cbi_data, CBI_IMAGE_SIZE);
	if (is_cbi_data_empty(cbi_data, CBI_IMAGE_SIZE)) {
		eeprom_cbi_config.drv->load(0, cbi_data, CBI_IMAGE_SIZE);
		flash_cbi_config.drv->store(cbi_data);
	}
}
