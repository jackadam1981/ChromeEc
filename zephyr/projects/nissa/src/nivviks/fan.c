/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <devicetree.h>
#include <drivers/gpio.h>
#include <init.h>
#include <logging/log.h>

#include "cros_cbi.h"
#include "fan.h"
#include "gpio/gpio.h"

#include "nissa_common.h"

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/*
 * Nirwen fan support
 */
static int fan_init(const struct device *unused)
{
	int ret;
	uint32_t val;
	/*
	 * Retrieve the fan config.
	 */
	ret = cros_cbi_get_fw_config(FW_FAN, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d",
			FW_FAN);
		return 0;
	}
	if (val != FW_FAN_PRESENT) {
		/* Disable the fan */
		fan_set_count(0);
	} else {
		/* Configure the fan enable GPIO */
		gpio_pin_configure_dt(
			GPIO_DT_FROM_NODELABEL(gpio_fan_enable),
			GPIO_OUTPUT);
	}
	return 0;
}

SYS_INIT(fan_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
