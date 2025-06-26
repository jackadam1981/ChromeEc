/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB-C board functions for the skywalker reference board only
 */

#include "cros_board_info.h"
#include "usbc/pdc_runtime_port_config.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(skywalker_usbc, LOG_LEVEL_INF);

/** Supply pdc_power_mgmt with dynamic USB-C port configuration data */
int board_get_pdc_for_port(int port, const struct device **dev)
{
	int rv;
	uint64_t rework_id = 0;

	if (dev == NULL) {
		LOG_ERR("%s: Bad pointer", __func__);
		return -EINVAL;
	}

	rv = cbi_get_rework_id(&rework_id);
	if (rv) {
		LOG_ERR("%s: Cannot read rework ID: %d. Use default PDC config.",
			__func__, rv);
	}

	switch (rework_id) {
	default:
		/* RTK PDC */
		switch (port) {
		case 0:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0));
			return 0;
		case 1:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p1));
			return 0;
		}
		break;
	case 0x1:
		/* TI PDC */
		switch (port) {
		case 0:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0_ti));
			return 0;
		case 1:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p1_ti));
			return 0;
		}
		break;
	}

	LOG_ERR("%s: No entry for rework_id (0x%llx) or port %d", __func__,
		rework_id, port);

	*dev = NULL;
	return -ENOENT;
}
