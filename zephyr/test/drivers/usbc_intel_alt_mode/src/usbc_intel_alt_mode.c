/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_mux.h"

#include <time.h>

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

#include <drivers/intel_altmode.h>
#include <emul_intel_pd_controller.h>
#include <host_command.h>

#define DT_DRV_COMPAT intel_pd_altmode

/* Delay longer than driver ISR latency */
#define USBC_INTEL_ALTMODE_DELAY (500) /* 500 ms */
#define USBC_INTEL_ALTMODE_WAIT() k_msleep(USBC_INTEL_ALTMODE_DELAY)

#define USBC_INTEL_ALTMODE_EMUL_ARRAY_WITH_COMMA(node_id) EMUL_DT_GET(node_id),

#define CHECK_MUX_FLAGS(port, flags) ((usb_mux_get(port) & (flags)) == (flags))

const struct emul *emul_pd_ctrlr[] = { DT_FOREACH_STATUS_OKAY(
	DT_DRV_COMPAT, USBC_INTEL_ALTMODE_EMUL_ARRAY_WITH_COMMA) };

#define USBC_INTEL_ALTMODE_PORT_ENUM_WITH_COMMA(n) USBC_PORT##n,

enum usbc_port {
	DT_INST_FOREACH_STATUS_OKAY(USBC_INTEL_ALTMODE_PORT_ENUM_WITH_COMMA)
		USBC_PORT_COUNT,
};

BUILD_ASSERT(USBC_PORT_COUNT == CONFIG_USB_PD_PORT_MAX_COUNT);

/*
 * These functions are required to satisfy build since
 * CONFIG_PLATFORM_EC_CHARGE_MANAGER is not set.
 */
__override uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

__override int charge_get_display_charge(void)
{
	return 0;
}

int charge_manager_get_active_charge_port(void)
{
	return 0;
}

/* Test functions */
void usbc_intel_altmode_before(void *fixture)
{
	/* Give some time to driver to response settle registers before each
	 * test*/
	USBC_INTEL_ALTMODE_WAIT();
}

ZTEST_USER(usbc_intel_altmode, test_conn)
{
	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		emul_intel_pd_controller_connect_data(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();
	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i), "Port %d Failed", i);
	}
}

ZTEST_USER(usbc_intel_altmode, test_dp)
{
	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));
	emul_intel_pd_controller_connect_data(emul_pd_ctrlr[USBC_PORT0]);
	emul_intel_pd_controller_connect_dp(emul_pd_ctrlr[USBC_PORT0]);
	USBC_INTEL_ALTMODE_WAIT();
	zassert_true(pd_is_connected(USBC_PORT0));
	zassert_true(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_DP_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_DP_ENABLED));

	emul_intel_pd_controller_connect_data(emul_pd_ctrlr[USBC_PORT1]);
	emul_intel_pd_controller_connect_dp(emul_pd_ctrlr[USBC_PORT1]);
	emul_intel_pd_controller_set_dp_irq(emul_pd_ctrlr[USBC_PORT1]);
	USBC_INTEL_ALTMODE_WAIT();
	zassert_true(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_DP_ENABLED));
	zassert_true(pd_is_connected(USBC_PORT1));
	zassert_true(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_DP_ENABLED));
}

ZTEST_USER(usbc_intel_altmode, test_mux_usb2)
{
	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_USB_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		emul_intel_pd_controller_connect_data(emul_pd_ctrlr[i]);
		emul_intel_pd_controller_connect_usb2(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i));
		zassert_true(CHECK_MUX_FLAGS(i, USB_PD_MUX_USB_ENABLED),
			     "Port %d Failed", i);
	}
}

ZTEST_USER(usbc_intel_altmode, test_mux_usb3_2)
{
	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_USB_ENABLED));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_USB_ENABLED));

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		emul_intel_pd_controller_connect_data(emul_pd_ctrlr[i]);
		emul_intel_pd_controller_connect_usb3_2(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i));
		zassert_true(CHECK_MUX_FLAGS(i, USB_PD_MUX_USB_ENABLED),
			     "Port %d Failed", i);
	}
}

ZTEST_USER(usbc_intel_altmode, test_mux_hpd_lvl)
{
	zassert_false(pd_is_connected(USBC_PORT0));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT0, USB_PD_MUX_HPD_LVL));
	zassert_false(pd_is_connected(USBC_PORT1));
	zassert_false(CHECK_MUX_FLAGS(USBC_PORT1, USB_PD_MUX_HPD_LVL));

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		emul_intel_pd_controller_connect_data(emul_pd_ctrlr[i]);
		emul_intel_pd_controller_set_hpd_lvl(emul_pd_ctrlr[i]);
	}
	USBC_INTEL_ALTMODE_WAIT();

	for (int i = 0; i < USBC_PORT_COUNT; i++) {
		zassert_true(pd_is_connected(i));
		zassert_true(CHECK_MUX_FLAGS(i, USB_PD_MUX_HPD_LVL),
			     "Port %d Failed", i);
	}
}

#ifdef CONFIG_CONSOLE_CMD_USBPD_INTEL_ALTMODE
static int usbc_intel_altmode_run_console_cmd(const char *cmd,
					      const char **resp,
					      size_t *resp_size)
{
	const struct shell *sh = get_ec_shell();
	int rv;

	shell_backend_dummy_clear_output(sh);

	rv = shell_execute_cmd(sh, cmd);

	if (resp && resp_size) {
		*resp = shell_backend_dummy_get_output(sh, resp_size);
	}

	return rv;
}

ZTEST_USER(usbc_intel_altmode, test_console_cmd)
{
	union data_status_reg _status = { 0 };
	union data_status_reg status = { 0 };
	const char *rdval = "RD_VAL:";
	const char *buffer;
	size_t buffer_size;
	unsigned int temp[INTEL_ALTMODE_DATA_STATUS_REG_LEN];
	int rv;

	rv = usbc_intel_altmode_run_console_cmd("altmode read 0", &buffer,
						&buffer_size);
	zassert_equal(EC_SUCCESS, rv);

	zassert_not_null(buffer);

	/* Place `buffer` pointer in the right position for scanning */
	buffer = strstr(buffer, rdval);
	zassert_not_null(buffer);

	rv = sscanf(buffer,
		    "RD_VAL: [0]0x%x, [1]0x%x, [2]0x%x, [3]0x%x, [4]0x%x",
		    &temp[0], &temp[1], &temp[2], &temp[3], &temp[4]);
	zassert_equal(INTEL_ALTMODE_DATA_STATUS_REG_LEN, rv);

	for (int i = 0; i < INTEL_ALTMODE_DATA_STATUS_REG_LEN; i++) {
		_status.raw_value[i] = temp[i] & 0xFF;
	}
	zassert_equal(0, strncmp(&status.raw_value[0], &_status.raw_value[0],
				 sizeof(status)));

	rv = usbc_intel_altmode_run_console_cmd(
		"altmode write 0 0x0 0x1 0x2 0x3 0x4 0x5", NULL, 0);

	zassert_equal(EC_SUCCESS, rv);
}

ZTEST_USER(usbc_intel_altmode, test_error_console_cmd)
{
	const char *buffer;
	size_t buffer_size;
	const char *err_msg = "Invalid port";
	int rv;

	/* Test read command error */
	rv = usbc_intel_altmode_run_console_cmd("altmode read 5", &buffer,
						&buffer_size);
	zassert_equal(-EINVAL, rv);
	zassert_not_null(buffer);

	buffer = strstr(buffer, err_msg);
	zassert_not_null(buffer);

	/* Test write command error */
	rv = usbc_intel_altmode_run_console_cmd(
		"altmode write 5 0x0 0x1 0x2 0x3 0x4 0x5", &buffer,
		&buffer_size);
	zassert_equal(-EINVAL, rv);
	zassert_not_null(buffer);

	buffer = strstr(buffer, err_msg);
	zassert_not_null(buffer);
}
#endif /* CONFIG_CONSOLE_CMD_USBPD_INTEL_ALTMODE */

ZTEST_SUITE(usbc_intel_altmode, drivers_predicate_post_main, NULL,
	    usbc_intel_altmode_before, NULL, NULL);
