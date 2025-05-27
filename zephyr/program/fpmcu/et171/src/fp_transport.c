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
	// printk("USB MKBP active: %d\n", active);
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

static __ramfunc int command_usb(int argc, const char **argv)
{
	host_set_single_event(EC_HOST_EVENT_INTERFACE_READY);

	return EC_SUCCESS;
};
DECLARE_SAFE_CONSOLE_COMMAND(usb, command_usb,
			     "[ save | restore | <mask> | <name> ]",
			     "Save, restore, get or set console channel mask");

int write_protect_is_asserted_custom(void)
{
	return 0;
}
