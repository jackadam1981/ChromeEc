/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/mgmt/ec_host_cmd/backend.h>
#include <zephyr/mgmt/ec_host_cmd/ec_host_cmd.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>

#include <stdarg.h>
#include "console.h"
#include "host_command.h"
#include "hwtimer.h"
#include "printf.h"
#include "uart.h"
#include "usb_console.h"
#include "util.h"
#include <zephyr/linker/linker-defs.h>

void usb_set_mkbp(int active)
{
	if (active) {
		ec_host_cmd_backend_usb_trigger_event();
	}
}

int mkbp_set_host_active_via_custom(int active, uint32_t *timestamp)
{
	usb_set_mkbp(active);
	if (timestamp) {
		*timestamp = __hw_clock_source_read();
	}

	return EC_SUCCESS;
}
