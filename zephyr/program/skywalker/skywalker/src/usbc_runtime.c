/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB-C board functions for the Trulo reference board only
 */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "usbc/pdc_runtime_port_config.h"

#include <stdint.h>

/** Supply pdc_power_mgmt with dynamic USB-C port configuration data */
int board_get_pdc_for_port(int port, const struct device **dev)
{
	switch (port) {
	case 0:
		*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p0));
		return 0;
	case 1:
		//	*dev = DEVICE_DT_GET(DT_NODELABEL(pdc_power_p1));
		//	return 0;
		break;
	}
	*dev = NULL;
	return -ENOENT;
}
