/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * USB-C board functions for the Trulo reference board only
 */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "usbc/pdc_power_mgmt.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(grogu_usbc, LOG_LEVEL_INF);

enum usb_typec_current_t pdc_power_mgmt_get_default_current_limit(int port)
{
	return TC_CURRENT_3_0A;
}
