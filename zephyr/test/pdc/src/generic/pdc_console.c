/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "emul/emul_pdc.h"

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#define TEST_PORT 0
#define SLEEP_MS 120
#define RTS5453P_NODE DT_NODELABEL(rts5453p_emul)

static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);

static void console_cmd_pdc_setup(void)
{
	struct pdc_info_t info = {
		.fw_version = 0x001a2b3c,
		.pd_version = 0xabcd,
		.pd_revision = 0x1234,
		.vid_pid = 0x12345678,
	};

	/* Set a FW version in the emulator for `test_info` */
	emul_pdc_set_info(emul, &info);

	zassume(TEST_PORT < CONFIG_USB_PD_PORT_MAX_COUNT,
		"TEST_PORT is invalid");
}

ZTEST_SUITE(console_cmd_pdc, NULL, console_cmd_pdc_setup, NULL, NULL, NULL);

ZTEST_USER(console_cmd_pdc, test_no_args)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "pdc");
	zassert_equal(rv, SHELL_CMD_HELP_PRINTED, "Expected %d, but got %d",
		      SHELL_CMD_HELP_PRINTED, rv);
}

ZTEST_USER(console_cmd_pdc, test_cable_prop)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "pdc cable_prop 99");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	rv = shell_execute_cmd(get_ec_shell(), "pdc cable_prop 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
}

ZTEST_USER(console_cmd_pdc, test_trysrc)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc enable");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
	k_sleep(K_MSEC(SLEEP_MS));

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc 1");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
	k_sleep(K_MSEC(SLEEP_MS));

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc 2");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);
}

ZTEST_USER(console_cmd_pdc, test_info)
{
	int rv;

	/* Bad chip number */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info x");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	/* Bad live param (should be int) */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info 0 y");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	/* Get chip #0 info live */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);

	/* Get chip #0 info cached */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info 0 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
}
