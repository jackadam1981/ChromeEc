/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "drivers/ucsi_v3.h"
#include "ec_commands.h"
#include "emul/emul_pdc.h"
#include "ppm_common.h"
#include "usbc/ppm.h"
#include "zephyr/logging/log.h"

#include <stdbool.h>

#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(ucsi_ppm_test, LOG_LEVEL_DBG);

#define TEST_PORT 0
#define PDC_WAIT_FOR_ITERATIONS 3

BUILD_ASSERT(TEST_PORT < CONFIG_USB_PD_PORT_MAX_COUNT);

static struct ucsi_ppm_device *ppm_dev;

static const struct ucsi_control_t enable_all_notifications = {
	.command = UCSI_SET_NOTIFICATION_ENABLE,
	.data_length = 0,
	.command_specific = { 0xff, 0xff, 0x1, 0x0, 0x0, 0x0 },
};

static void host_cmd_pdc_reset(void *fixture)
{
	const struct device *pdc;
	const struct ucsi_pd_driver *drv;

	pdc = DEVICE_DT_GET(DT_INST(0, ucsi_ppm));
	drv = pdc->api;
	ppm_dev = drv->get_ppm_dev(pdc);
}

ZTEST_SUITE(ucsi_ppm, NULL, NULL, host_cmd_pdc_reset, host_cmd_pdc_reset, NULL);

static int write_command(const struct ucsi_control_t *control)
{
	return ucsi_ppm_write(ppm_dev, UCSI_CONTROL_OFFSET,
			      (const void *)control,
			      sizeof(struct ucsi_control_t));
}

static int write_ack_command(bool connector_change_ack,
			     bool command_complete_ack)
{
	struct ucsi_control_t control = { .command = UCSI_ACK_CC_CI,
					  .data_length = 0 };
	union ack_cc_ci_t ack_data = {
		.connector_change_ack = connector_change_ack,
		.command_complete_ack = command_complete_ack
	};
	memcpy(control.command_specific, &ack_data, sizeof(ack_data));
	return write_command(&control);
}

/**
 * Return true if commands are no longer pending.
 */
static bool wait_for_cmd_to_process(void)
{
	bool is_cmd_pending = false;

	/*
	 * After calling write, the command will be pending and will trigger the
	 * main loop. Try reading the pending state a few times to see if it
	 * clears.
	 */
	for (int i = 0; i < PDC_WAIT_FOR_ITERATIONS; ++i) {
		is_cmd_pending = ppm_test_is_cmd_pending(ppm_dev);

		LOG_DBG("[%d]: Command is %s", i,
			(is_cmd_pending ? "pending" : "not pending"));
		if (is_cmd_pending) {
			k_msleep(1);
		} else {
			break;
		}
	}

	return !is_cmd_pending;
}

static bool reset_to_idle_notify(void)
{
	struct ucsi_control_t ctrl = {};

	LOG_INF("Sending UCSI_PPM_RESET");
	ctrl.command = UCSI_PPM_RESET;
	ctrl.data_length = 0;
	if (write_command(&ctrl))
		return false;
	if (!wait_for_cmd_to_process())
		return false;

	LOG_INF("Sending SET_NOTIFICATION_ENABLE");
	if (write_command(&enable_all_notifications))
		return false;
	if (!wait_for_cmd_to_process())
		return false;

	LOG_INF("Acking SET_NOTIFICATION_ENABLE");
	if (write_ack_command(false, true))
		return false;
	if (!wait_for_cmd_to_process())
		return false;

	return true;
}

static bool read_cci(union cci_event_t *cci)
{
	return ucsi_ppm_read(ppm_dev, UCSI_CCI_OFFSET, (void *)cci,
			     sizeof(*cci)) == sizeof(*cci);
}

ZTEST(ucsi_ppm, test_unexpected_command_in_idle)
{
	struct ucsi_control_t ctrl = {};

	LOG_INF("Sending UCSI_GET_CONNECTOR_STATUS");
	ctrl.command = UCSI_GET_CONNECTOR_STATUS;
	ctrl.command_specific[0] = 1;
	zassert_equal(write_command(&ctrl), 0);
	/* Command should be ignored because PPM is still in IDLE. */
	zassert_true(wait_for_cmd_to_process());
}

ZTEST(ucsi_ppm, test_set_notification_enable)
{
	zassert_true(reset_to_idle_notify());
}

ZTEST(ucsi_ppm, test_invalid_conn)
{
	struct ucsi_control_t ctrl = {};
	union cci_event_t cci;

	zassert_true(reset_to_idle_notify());

	/*
	 * Test conn=0 using CONNECTOR_RESET.
	 */
	LOG_INF("Sending CONNECTOR_RESET");
	ctrl.command = UCSI_CONNECTOR_RESET;
	ctrl.command_specific[0] = 0;
	zassert_equal(write_command(&ctrl), 0);
	zassert_true(wait_for_cmd_to_process());

	zassert_true(read_cci(&cci));
	zassert_equal(cci.error, 1);
	zassert_equal(cci.command_completed, 1);

	LOG_INF("Acking SET_NOTIFICATION_ENABLE");
	zassert_equal(write_ack_command(false, true), 0);
	zassert_true(wait_for_cmd_to_process());

	/*
	 * Test conn=3 using CONNECTOR_RESET.
	 */
	LOG_INF("Sending CONNECTOR_RESET");
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.command = UCSI_CONNECTOR_RESET;
	ctrl.command_specific[0] = 3;
	zassert_equal(write_command(&ctrl), 0);
	zassert_true(wait_for_cmd_to_process());

	zassert_true(read_cci(&cci));
	zassert_equal(cci.error, 1);
	zassert_equal(cci.command_completed, 1);

	LOG_INF("Acking SET_NOTIFICATION_ENABLE");
	zassert_equal(write_ack_command(false, true), 0);
	zassert_true(wait_for_cmd_to_process());
}

ZTEST(ucsi_ppm, test_get_connector_capability)
{
	struct ucsi_control_t ctrl = {};
	union cci_event_t cci;

	zassert_true(reset_to_idle_notify());

	ctrl.command = UCSI_GET_CONNECTOR_CAPABILITY;
	ctrl.data_length = 0;
	ctrl.command_specific[0] = 1;
	zassert_equal(write_command(&ctrl), 0);
	zassert_true(wait_for_cmd_to_process());

	zassert_true(read_cci(&cci));
	zassert_equal(cci.command_completed, 1);
	zassert_true(cci.error != 1);
	zassert_equal(cci.data_len, 0x04);
}
