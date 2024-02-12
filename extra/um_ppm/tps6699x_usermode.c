/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/pd_driver.h"
#include "include/platform.h"
#include "tps6699x.h"

#include <stdio.h>

int tps6699x_get_info(struct ucsi_pd_driver *pd)
{
	struct tps6699x_device *dev = (struct tps6699x_device *)pd->dev;
	struct tps6699x_boot_flags flags = { 0 };
	struct tps6699x_device_info info = { 0 };
	uint32_t version = 0;

	int ret = tps6699x_get_boot_flags(dev, &flags);
	if (ret <= 0) {
		ELOG("Failed to get boot flags: %d", ret);
		return -1;
	}

	ret = tps6699x_get_version(dev, &version);
	if (ret <= 0) {
		ELOG("Failed to get version: %d", ret);
		return -1;
	}

	ret = tps6699x_get_device_info(dev, &info);
	if (ret <= 0) {
		ELOG("Failed to get device info: %d", ret);
		return -1;
	}

	printf("Active bank: %u, Are Banks Valid: [%b; %b]\n",
	       ACTIVE_BANK_MASK(flags.bank_info), BANK0_VALID(flags.bank_info),
	       BANK1_VALID(flags.bank_info));
	printf("Bank 0 FW (0x%x), Bank 1 FW (0x%x)\n", flags.fw_version_bank0,
	       flags.fw_version_bank1);
	printf("Fw version: 0x%x\n", version);
	printf("Device info: %s\n", info.data);

	return 0;
}

int tps6699x_do_firmware_update(struct ucsi_pd_driver *pd, const char *filepath,
				int dry_run)
{
	return -1;
}
