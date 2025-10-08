/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"

#include <zephyr/drivers/flash.h>

#define FT9001_SECURE_ROM_ADDR 0x80000
#define FT9001_SECURE_ROM_SIZE 4096

#define flash_ctrl_dev DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))

void chip_enter_bootloader(uint8_t mode)
{
	int ret = flash_erase(flash_ctrl_dev, FT9001_SECURE_ROM_ADDR, FT9001_SECURE_ROM_SIZE);

	if (ret) {
		printk("Failed to erase secure rom address\n");
	}
	system_reset(SYSTEM_RESET_HARD);
}

int write_protect_is_asserted_custom(void)
{
	return 0;
}
