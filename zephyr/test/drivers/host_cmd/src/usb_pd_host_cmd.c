/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "test_usb_pd_host_cmd.h"
#include "usb_pd.h"
#include "test_usb_pd_host_cmd.h"

DEFINE_FAKE_VOID_FUNC(pd_send_vdm, int, uint32_t, int, const uint32_t *, int);
DEFINE_FAKE_VALUE_FUNC(int, charge_manager_get_active_charge_port);

ZTEST_USER(usb_pd_host_cmd, test_hc_pd_host_event_status)
{
	struct ec_response_host_event_status response;
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_PD_HOST_EVENT_STATUS, 0, response);

	/* Clear events */
	zassert_ok(host_command_process(&args));

	/* Send arbitrary event */
	pd_send_host_event(1);

	zassert_ok(host_command_process(&args));
	zassert_equal(args.response_size, sizeof(response));
	zassert_true(response.status & 1);

	/* Send again to make sure the host command cleared the event */
	zassert_ok(host_command_process(&args));
	zassert_equal(args.response_size, sizeof(response));
	zassert_equal(response.status, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_host_command_hc_pd_ports)
{
	struct ec_response_usb_pd_ports response;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_RESPONSE(EC_CMD_USB_PD_PORTS, 0, response);

	zassert_ok(host_command_process(&args));
	zassert_ok(args.result);
	zassert_equal(args.response_size, sizeof(response));
	zassert_equal(response.num_ports, CONFIG_USB_PD_PORT_MAX_COUNT);
}

static void usb_pd_host_cmd_before_after(void *test_data)
{
	ARG_UNUSED(test_data);

	test_set_battery_present(true);

	RESET_FAKE(pd_send_vdm);
	RESET_FAKE(charge_manager_get_active_charge_port);
}

ZTEST_SUITE(usb_pd_host_cmd, drivers_predicate_post_main, NULL,
	    usb_pd_host_cmd_before_after, usb_pd_host_cmd_before_after, NULL);
