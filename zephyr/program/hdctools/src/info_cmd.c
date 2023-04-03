/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console_util.h"
#include "ec_version.h"

#include <zephyr/drivers/hwinfo.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_DECLARE(console);

static void info_cmd(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		LOG_PRINTK("Print the Device Info");
		return;
	}

	LOG_INF("Firmware Version: %s", CROS_EC_VERSION32);

	uint8_t buffer[12];
	char ascii[2 * sizeof(buffer) + 1];

	hwinfo_get_device_id(buffer, sizeof(buffer));

	// Print as words
	bin2hex(buffer, sizeof(buffer), ascii, sizeof(ascii));
	LOG_INF("Device ID: %s", ascii);
}

SHELL_CMD_REGISTER(info, NULL, "Sim card commands", info_cmd);
