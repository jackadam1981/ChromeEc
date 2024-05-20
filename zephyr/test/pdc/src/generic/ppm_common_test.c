/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppm_common.h"

#include <zephyr/device.h>
#include <zephyr/ztest.h>

#include <include/pd_driver.h>
#include <include/platform.h>
#include <include/ppm.h>

#define PDC_NUM_PORTS 2
#define PDC_DEFAULT_CONNECTOR 1

#define PDC_WAIT_FOR_ITERATIONS 3

struct ppm_common_test_fixture {
	struct ucsi_pd_driver *pd;
	struct ucsi_ppm_driver *ppm;

	struct ucsiv3_get_connector_status_data port_status[PDC_NUM_PORTS];

	int notified_count;
};

static void opm_notify_cb(void *ctx)
{
	struct ppm_common_test_fixture *fixture =
		(struct ppm_common_test_fixture *)ctx;

	fixture->notified_count++;
	/* TODO(b/340895744) - Signal to test that there's a notification */
}

static struct ppm_common_device *
get_ppm_data(struct ppm_common_test_fixture *fixture)
{
	return (struct ppm_common_device *)fixture->ppm->dev;
}

static int ppm_initialize(struct ppm_common_test_fixture *fixture)
{
	return fixture->pd->init_ppm((const struct device *)fixture);
}

static void ppm_send_lpm_alert(struct ppm_common_test_fixture *fixture,
			       uint8_t connector)
{
	fixture->ppm->lpm_alert(fixture->ppm->dev, connector);
}

static int ppm_write_command(struct ppm_common_test_fixture *fixture,
			     struct ucsi_control *control)
{
	return fixture->ppm->write(fixture->ppm->dev, UCSI_CONTROL_OFFSET,
				   (void *)control,
				   sizeof(struct ucsi_control));
}

static bool
ppm_wait_for_async_event_to_process(struct ppm_common_test_fixture *fixture)
{
	bool is_async_pending = false;

	for (int i = 0; i < PDC_WAIT_FOR_ITERATIONS; ++i) {
		platform_mutex_lock(get_ppm_data(fixture)->ppm_lock);
		is_async_pending = get_ppm_data(fixture)->pending.async_event;
		platform_mutex_unlock(get_ppm_data(fixture)->ppm_lock);

		if (is_async_pending) {
			platform_condvar_signal(
				get_ppm_data(fixture)->ppm_condvar);
			k_msleep(1);
		} else {
			break;
		}
	}

	return !is_async_pending;
}

static bool ppm_wait_for_cmd_to_process(struct ppm_common_test_fixture *fixture)
{
	bool is_cmd_pending = false;

	/*
	 * After calling write, the command will be pending. Trigger the main
	 * loop and wait for processing to occur. We sleep to give time for the
	 * loop to run.
	 */
	for (int i = 0; i < PDC_WAIT_FOR_ITERATIONS; ++i) {
		platform_mutex_lock(get_ppm_data(fixture)->ppm_lock);
		is_cmd_pending = get_ppm_data(fixture)->pending.command;
		platform_mutex_unlock(get_ppm_data(fixture)->ppm_lock);

		if (is_cmd_pending) {
			platform_condvar_signal(
				get_ppm_data(fixture)->ppm_condvar);
			k_msleep(1);
		} else {
			break;
		}
	}

	return !is_cmd_pending;
}

/* Fake PD driver implementations. */

static int fake_pd_init_ppm(const struct device *device)
{
	struct ppm_common_test_fixture *fixture =
		(struct ppm_common_test_fixture *)device;

	int rv = fixture->ppm->register_notify(fixture->ppm->dev, opm_notify_cb,
					       fixture);

	if (rv < 0) {
		return rv;
	}

	return fixture->ppm->init_and_wait(fixture->ppm->dev, PDC_NUM_PORTS);
}

static struct ucsi_ppm_driver *fake_pd_get_ppm(const struct device *device)
{
	struct ppm_common_test_fixture *fixture =
		(struct ppm_common_test_fixture *)device;

	return fixture->ppm;
}

static int fake_pd_execute_cmd(const struct device *device,
			       struct ucsi_control *control,
			       uint8_t *lpm_data_out)
{
	return 0;
}

static int fake_pd_get_active_port_count(const struct device *dev)
{
	return PDC_NUM_PORTS;
}

/* Globals for the tests. */

static struct ppm_common_test_fixture test_fixture;

/* Fake PD driver used for emulating peer PDC. */
static struct ucsi_pd_driver fake_pd_driver = {
	.init_ppm = fake_pd_init_ppm,
	.get_ppm = fake_pd_get_ppm,
	.execute_cmd = fake_pd_execute_cmd,
	.get_active_port_count = fake_pd_get_active_port_count,
};

static void *ppm_common_test_setup(void)
{
	platform_set_debug(true);

	test_fixture.pd = &fake_pd_driver;
	return &test_fixture;
}

static void ppm_common_test_before(void *f)
{
	/* Open ppm_common implementation with fake driver for testing. */
	test_fixture.ppm = ppm_open(test_fixture.pd, test_fixture.port_status,
				    (const struct device *)&test_fixture);
}

static void ppm_common_test_after(void *f)
{
	/* Must clean up between tests to re-init the state machine. */
	test_fixture.ppm->cleanup(test_fixture.ppm);
}

ZTEST_SUITE(ppm_common_test, /*predicate=*/NULL, ppm_common_test_setup,
	    ppm_common_test_before, ppm_common_test_after, /*teardown=*/NULL);

/* On init, PPM should go into the Idle State. */
ZTEST_USER_F(ppm_common_test, test_initialize_to_idle)
{
	zassert_equal(ppm_initialize(fixture), 0);

	/* System should be in the idle state at the end of init. */
	zassert_equal(get_ppm_data(fixture)->ppm_state, PPM_STATE_IDLE);
}

/* From the IDLE state, only PPM_RESET and SET_NOTIFICATION_ENABLE is allowed.
 */
ZTEST_USER_F(ppm_common_test, test_IDLE_drops_unexpected_commands)
{
	zassert_equal(ppm_initialize(fixture), 0);

	/* Try all commands except PPM_RESET and SET_NOTIFICATION_ENABLE.
	 * They should result in no change to the state.
	 */
	for (uint8_t cmd = UCSI_CMD_PPM_RESET; cmd <= UCSI_CMD_MAX; cmd++) {
		if (cmd == UCSI_CMD_PPM_RESET ||
		    cmd == UCSI_CMD_SET_NOTIFICATION_ENABLE) {
			continue;
		}

		struct ucsi_control control = { .command = cmd,
						.data_length = 0 };

		/* Make sure Write completed and then wait for pending command
		 * to be cleared. Only the .command part will really matter as
		 * that's how we determine whether the next command should be
		 * executed.
		 */
		zassert_false(ppm_write_command(fixture, &control) < 0);
		zassert_true(ppm_wait_for_cmd_to_process(fixture));
		zassert_equal(get_ppm_data(fixture)->ppm_state, PPM_STATE_IDLE);
	}

	struct ucsi_control control = {
		.command = UCSI_CMD_SET_NOTIFICATION_ENABLE, .data_length = 0
	};

	/* SET_NOTIFICATION_ENABLE should then switch it to a non-idle state. */
	zassert_false(ppm_write_command(fixture, &control) < 0);
	zassert_true(ppm_wait_for_cmd_to_process(fixture));
	zassert_equal(get_ppm_data(fixture)->ppm_state,
		      PPM_STATE_WAITING_CC_ACK);
}

/* From the Idle state, we process async events but we do not notify the OPM or
 * change the PPM state (i.e. silently drop).
 */
ZTEST_USER_F(ppm_common_test, test_IDLE_silently_processes_async_event)
{
	zassert_equal(ppm_initialize(fixture), 0);
	fixture->notified_count = 0;

	/* Send an alert on default connector. */
	ppm_send_lpm_alert(fixture, PDC_DEFAULT_CONNECTOR);

	zassert_true(ppm_wait_for_async_event_to_process(fixture));
	zassert_equal(fixture->notified_count, 0);
	zassert_equal(get_ppm_data(fixture)->ppm_state, PPM_STATE_IDLE);
}
