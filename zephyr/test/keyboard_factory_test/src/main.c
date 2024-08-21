/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "keyboard_scan.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/fff.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_kbd_matrix.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, vnd_keyboard_pm_action, const struct device *,
		enum pm_device_action);
FAKE_VALUE_FUNC(int, pinctrl_configure_pins, const pinctrl_soc_pin_t *, uint8_t,
		uintptr_t);
FAKE_VALUE_FUNC(int, system_is_locked);

#define VND_KEYBOARD_NODE DT_INST(0, vnd_keyboard_input_device)

PM_DEVICE_DT_DEFINE(VND_KEYBOARD_NODE, vnd_keyboard_pm_action);

DEVICE_DT_DEFINE(VND_KEYBOARD_NODE, NULL, PM_DEVICE_DT_GET(VND_KEYBOARD_NODE),
		 NULL, NULL, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		 NULL);

ZTEST(keyboard_factory_test, test_factory_test_hc)
{
	struct ec_response_keyboard_factory_test resp;
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_KEYBOARD_FACTORY_TEST, 0, resp);

	zassert_ok(host_command_process(&args));
}

ZTEST(keyboard_factory_test, test_factory_test_shell)
{
	const struct shell *shell_zephyr = shell_backend_dummy_get_ptr();
	const char *outbuffer;
	size_t buffer_size;

	/* Give the backend time to initialize */
	k_sleep(K_MSEC(100));

	shell_backend_dummy_clear_output(shell_zephyr);

	zassert_ok(shell_execute_cmd(shell_zephyr, "kbfactorytest"));
	outbuffer = shell_backend_dummy_get_output(shell_zephyr, &buffer_size);
	zassert_true(buffer_size > 0, NULL);

	zassert_not_null(strstr(outbuffer,
				"Keyboard factory test: shorted=0000 (0, 0)"));
}

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(system_is_locked);
}

ZTEST_SUITE(keyboard_factory_test, NULL, NULL, reset, reset, NULL);
