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

LOG_MODULE_REGISTER(uldrenite_usbc, LOG_LEVEL_INF);

/** Supply pdc_power_mgmt with dynamic USB-C port configuration data */
int board_get_pdc_for_port(int port, const struct device **dev)
{
	int rv;
	uint32_t fw_config;

	if (dev == NULL) {
		LOG_ERR("%s: Bad pointer", __func__);
		return -EINVAL;
	}

	rv = cbi_get_fw_config(&fw_config);

	if (rv) {
		LOG_ERR("%s: Cannot read CBI FW CONFIG: %d. PDC config unknown!",
			__func__, rv);

		*dev = NULL;
		return -EIO;
	}

	switch (fw_config) {
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x29:
	case 0x2A:
	case 0x30:
	case 0x32:
		/* Two type-c ports */
		switch (port) {
		case 0:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0));
			return 0;
		case 1:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p1));
			return 0;
		}
		break;
	case 0xB0:
		/* One type-c port */
		switch (port) {
		case 0:
			*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0));
			return 0;
		case 1:
			*dev = NULL;
			return 0;
		}
		break;
	}

	LOG_ERR("%s: No entry for FW config (0x%x) or port %d", __func__,
		fw_config, port);

	*dev = NULL;
	return -ENOENT;
}
