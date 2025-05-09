/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "usbc/pdc_runtime_port_config.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

/** Supply pdc_power_mgmt with dynamic USB-C port configuration data */
int board_get_pdc_for_port(int port, const struct device **dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	switch (port) {
	case 0:
		*dev = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(pd_pow_detect)) ?
			       DEVICE_DT_GET(DT_NODELABEL(pd_pow_port0)) :
			       NULL;
		return 0;
	case 1:
		*dev = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(pd_pow_detect)) ?
			       DEVICE_DT_GET(DT_NODELABEL(pd_pow_port1)) :
			       NULL;
		return 0;
	case 2:
		*dev = gpio_pin_get_dt(
			       GPIO_DT_FROM_NODELABEL(pd_pow_p2_detect)) ?
			       DEVICE_DT_GET(DT_NODELABEL(pd_pow_port2)) :
			       NULL;
		return 0;
	default:
	}

	return -ENOENT;
}
