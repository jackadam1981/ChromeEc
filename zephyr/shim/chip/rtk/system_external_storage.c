/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//#include "clock_chip.h"
#include "common.h"
#include "config_chip.h"
#include "system.h"
//#include "system_chip.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/bbram.h>
#include <zephyr/drivers/watchdog.h>
#include <soc.h>

// RTK_NEED_IMP: jumping, OTP?
#define DBGRAM_STUB ((volatile uint32_t *)0x200060FCul)
#define DBGRAM_IMG ((volatile uint32_t *)0x200060F8ul)

uint32_t system_get_lfw_address(void)
{
	if (IS_ENABLED(CONFIG_WATCHDOG)) {
		const struct device *wdt_dev =
			DEVICE_DT_GET(DT_NODELABEL(wdog));
		if (!device_is_ready(wdt_dev)) {
			// LOG_ERR("device %s not ready", wdt_dev->name);
			return 0;
		}

		wdt_disable(wdt_dev);
	}
	return *DBGRAM_STUB;
}
enum ec_image system_get_shrspi_image_copy(void)
{
	enum ec_image img = EC_IMAGE_UNKNOWN;
	if (((*DBGRAM_IMG) != EC_IMAGE_RO) && ((*DBGRAM_IMG) != EC_IMAGE_RW)){
		*DBGRAM_IMG = EC_IMAGE_RO;
	}
	img = (enum ec_image)(*DBGRAM_IMG);

	return img;
}

void system_set_image_copy(enum ec_image copy)
{
	uint32_t value = (uint32_t)copy;

	switch (copy) {
	case EC_IMAGE_RW:
	case EC_IMAGE_RW_B:
		value = EC_IMAGE_RW;
		break;
	case EC_IMAGE_RO:
	default:
		value = EC_IMAGE_RO;
		break;
	}
	
	*DBGRAM_IMG = value;
}
