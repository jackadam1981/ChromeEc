/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_DECLARE(console);

static void reboot_cmd(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		LOG_PRINTK("Reboot the Starfish");
		return;
	}
	sys_reboot(0);
}

/* Creating root (level 0) command "demo" without a handler */
SHELL_CMD_REGISTER(reboot, NULL, "Sim card commands", reboot_cmd);
