/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB-C board functions for the Brox reference board only
 */

#include "cros_board_info.h"
#include "usbc/pdc_runtime_port_config.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(brox_usbc, LOG_LEVEL_INF);

/** Supply pdc_power_mgmt with dynamic USB-C port configuration data */
int board_get_pdc_for_port(int port, const struct device **dev)
{
	if (dev == NULL) {
		LOG_ERR("%s: Bad pointer", __func__);
		return -EINVAL;
	}

	/* Currently caboc only support RTK PDC */
	switch (port) {
	case 0:
		*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0));
		return 0;
	case 1:
		*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p1));
		return 0;
	}

	LOG_ERR("%s: No entry for port %d", __func__, port);

	*dev = NULL;
	return -ENOENT;
}
