/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mock/usb_mux_mock.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "tcpci.h"
#include "usb_mux.h"
#include "hooks.h"

const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int pd_check_vconn_swap(int port)
{
	return 1;
}

void dfp_consume_cable_response(int port, int cnt, uint32_t *payload,
				uint16_t head)
{
}

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.drv = &tcpci_tcpm_drv,
	},
};

const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.driver = &mock_usb_mux_driver,
	}
};

__maybe_unused static int test_startup_and_resume(void)
{
	task_wait_event(10 * SECOND);

	hook_notify(HOOK_CHIPSET_STARTUP);
	task_wait_event(5 * MSEC);
	hook_notify(HOOK_CHIPSET_RESUME);
	task_wait_event(10 * SECOND);

	return EC_SUCCESS;
}

void before_test(void)
{
	mock_usb_mux_reset();
}

void after_test(void)
{
	ccprints("after_test");
}

void run_test(void)
{
	test_reset();

	/* Ensure that PD task initializes its state machine */
	task_wake(TASK_ID_PD_C0);
	task_wait_event(5 * MSEC);

	RUN_TEST(test_startup_and_resume);

	test_print_result();
}
