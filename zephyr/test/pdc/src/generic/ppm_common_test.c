/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppm_common.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <include/pd_driver.h>
#include <include/platform.h>
#include <include/ppm.h>

#define PDC_NUM_PORTS 2
#define PDC_DEFAULT_CONNECTOR 1
#define PDC_WAIT_FOR_ITERATIONS 3

#define CMD_WAIT_TIMEOUT K_MSEC(200)
#define CMD_QUEUE_SIZE 4

#define LPM_DATA_MAX 32
struct expected_command_t {
	uint32_t queue_header;

	uint8_t ucsi_command;
	int result;

	bool has_lpm_data;
	uint8_t lpm_data[LPM_DATA_MAX];
} __attribute__((aligned(4)));

struct ppm_common_test_fixture {
	struct ucsi_pd_driver *pd;
	struct ucsi_ppm_driver *ppm;

	struct ucsiv3_get_connector_status_data port_status[PDC_NUM_PORTS];

	int notified_count;

	/* Commands handling. */
	struct expected_command_t next_command_result;
	struct k_queue *cmd_queue;
	struct k_sem cmd_sem;

	/* Allocate fixed array of commands and use a free queue to avoid
	 * allocations.
	 */
	struct k_queue *free_cmd_queue;
	struct expected_command_t cmd_memory[CMD_QUEUE_SIZE];

	/* Notifications handling. */
	struct k_sem opm_sem;
};

static void opm_notify_cb(void *ctx)
{
	struct ppm_common_test_fixture *fixture =
		(struct ppm_common_test_fixture *)ctx;

	fixture->notified_count++;
	k_sem_give(&fixture->opm_sem);
}

static struct ppm_common_device *
get_ppm_data(struct ppm_common_test_fixture *fixture)
{
	return (struct ppm_common_device *)fixture->ppm->dev;
}

static bool ppm_cci_matches(struct ppm_common_test_fixture *fixture,
			    const struct ucsi_cci *cci)
{
	struct ucsi_cci actual_cci;
	int rv = fixture->ppm->read(fixture->ppm->dev, UCSI_CCI_OFFSET,
				    (void *)&actual_cci, sizeof(actual_cci));

	if (rv < 0) {
		return false;
	}

	return *((uint32_t *)cci) == *((uint32_t *)&actual_cci);
}

static void
ppm_complete_specific_command(struct ppm_common_test_fixture *fixture,
			      uint8_t ucsi_command, int result,
			      uint8_t *lpm_data)
{
	fixture->next_command_result.ucsi_command = ucsi_command;
	fixture->next_command_result.result = result;
	fixture->next_command_result.has_lpm_data = lpm_data != NULL;
	if (lpm_data) {
		memcpy(fixture->next_command_result.lpm_data, lpm_data,
		       LPM_DATA_MAX);
	}

	k_sem_give(&fixture->cmd_sem);
	DLOG("Signaled for command 0x%x", ucsi_command);
}

static void
ppm_queue_command_with_result(struct ppm_common_test_fixture *fixture,
			      uint8_t ucsi_command, int result,
			      uint8_t *lpm_data)
{
	struct expected_command_t *cmd =
		k_queue_get(fixture->free_cmd_queue, K_NO_WAIT);
	zassert_true(cmd != NULL);
	if (!cmd) {
		return;
	}

	DLOG("Queueing command result for 0x%x with result %d", ucsi_command,
	     result);

	cmd->ucsi_command = ucsi_command;
	cmd->result = result;
	cmd->has_lpm_data = lpm_data != NULL;
	if (lpm_data) {
		memcpy(cmd->lpm_data, lpm_data, LPM_DATA_MAX);
	}

	k_queue_append(fixture->cmd_queue, cmd);
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

static void
ppm_trigger_connector_changed(struct ppm_common_test_fixture *fixture,
			      uint8_t connector)
{
	uint8_t lpm_data[LPM_DATA_MAX];
	struct ucsiv3_get_connector_status_data *data =
		(struct ucsiv3_get_connector_status_data *)lpm_data;

	data->connector_status_change = 1;

	ppm_queue_command_with_result(fixture, UCSI_CMD_GET_CONNECTOR_STATUS,
				      /*result=*/0, lpm_data);
	ppm_send_lpm_alert(fixture, connector);
}

static int ppm_write_command(struct ppm_common_test_fixture *fixture,
			     struct ucsi_control *control)
{
	return fixture->ppm->write(fixture->ppm->dev, UCSI_CONTROL_OFFSET,
				   (void *)control,
				   sizeof(struct ucsi_control));
}

static int ppm_write_ack_command(struct ppm_common_test_fixture *fixture,
				 bool connector_change_ack,
				 bool command_complete_ack)
{
	struct ucsi_control control = { .command = UCSI_CMD_ACK_CC_CI,
					.data_length = 0 };
	struct ucsiv3_ack_cc_ci_cmd ack_data = {
		.connector_change_ack = connector_change_ack,
		.command_complete_ack = command_complete_ack
	};
	memcpy(control.command_specific, &ack_data, sizeof(ack_data));
	return ppm_write_command(fixture, &control);
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

static bool ppm_wait_for_notification(struct ppm_common_test_fixture *fixture,
				      int expected_count)
{
	if (expected_count <= fixture->notified_count)
		return true;

	while (fixture->notified_count < expected_count) {
		if (k_sem_take(&fixture->opm_sem, CMD_WAIT_TIMEOUT) < 0) {
			return false;
		}
	}

	return true;
}

static void
ppm_initialize_to_idle_notify(struct ppm_common_test_fixture *fixture)
{
	zassert_false(ppm_initialize(fixture) < 0);
	ppm_queue_command_with_result(fixture, UCSI_CMD_SET_NOTIFICATION_ENABLE,
				      /*result=*/0, /*lpm_data=*/NULL);

	struct ucsi_control control = {
		.command = UCSI_CMD_SET_NOTIFICATION_ENABLE, .data_length = 0
	};
	zassert_false(ppm_write_command(fixture, &control) < 0);
	zassert_true(ppm_wait_for_cmd_to_process(fixture));
	zassert_equal(get_ppm_data(fixture)->ppm_state,
		      PPM_STATE_WAITING_CC_ACK);

	ppm_queue_command_with_result(fixture, UCSI_CMD_ACK_CC_CI,
				      /*result=*/0, /*lpm_data=*/NULL);
	zassert_false(ppm_write_ack_command(fixture,
					    /*connector_change_ack*/ false,
					    /*command_complete_ack*/ true) < 0);
	zassert_true(ppm_wait_for_cmd_to_process(fixture));
	zassert_equal(get_ppm_data(fixture)->ppm_state, PPM_STATE_IDLE_NOTIFY);
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
	struct ppm_common_test_fixture *fixture =
		(struct ppm_common_test_fixture *)device;
	uint8_t ucsi_command = control->command;
	int rv;

	DLOG("Executing fake cmd for UCSI_CMD:0x%x", ucsi_command);

	/* Return any commands that were queued up to return. */
	if (!k_queue_is_empty(fixture->cmd_queue)) {
		struct expected_command_t *cmd =
			(struct expected_command_t *)k_queue_get(
				fixture->cmd_queue, K_NO_WAIT);

		if (cmd == NULL) {
			DLOG("Command queue is unexpectedly empty!");
			return -ENOTSUP;
		}

		k_queue_append(fixture->free_cmd_queue, cmd);

		if (ucsi_command != cmd->ucsi_command) {
			DLOG("Expected queued command 0x%x doesn't match actual 0x%x",
			     cmd->ucsi_command, ucsi_command);
			return -ENOTSUP;
		}

		if (cmd->has_lpm_data) {
			memcpy(lpm_data_out, cmd->lpm_data, LPM_DATA_MAX);
		}

		DLOG("Returning queued result: %d", cmd->result);
		return cmd->result;
	}

	/* Since there were no commands queued up, wait for a signal to use a
	 * single next command.
	 */
	rv = k_sem_take(&fixture->cmd_sem, CMD_WAIT_TIMEOUT);

	if (rv != 0 ||
	    ucsi_command != fixture->next_command_result.ucsi_command) {
		DLOG("Sem take result(%d). Expected command %x vs actual %x",
		     fixture->next_command_result.ucsi_command, ucsi_command);
		return -ENOTSUP;
	}

	if (fixture->next_command_result.has_lpm_data) {
		memcpy(lpm_data_out, fixture->next_command_result.lpm_data,
		       LPM_DATA_MAX);
	}

	rv = fixture->next_command_result.result;

	DLOG("Returning specific result: %d", rv);
	return rv;
}

static int fake_pd_get_active_port_count(const struct device *dev)
{
	return PDC_NUM_PORTS;
}

/* Globals for the tests. */

static struct ppm_common_test_fixture test_fixture;
K_QUEUE_DEFINE(cmd_queue);
K_QUEUE_DEFINE(free_cmd_queue);

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

	test_fixture.cmd_queue = &cmd_queue;
	test_fixture.free_cmd_queue = &free_cmd_queue;

	for (int i = 0; i < CMD_QUEUE_SIZE; ++i) {
		k_queue_append(test_fixture.free_cmd_queue,
			       &test_fixture.cmd_memory[i]);
	}

	k_sem_init(&test_fixture.cmd_sem, 0, 1);
	k_sem_init(&test_fixture.opm_sem, 0, 1);

	return &test_fixture;
}

static void ppm_common_test_before(void *f)
{
	/* Clear command queue. */
	struct expected_command_t *cmd;
	while ((cmd = k_queue_get(test_fixture.cmd_queue, K_NO_WAIT))) {
		k_queue_append(test_fixture.free_cmd_queue, cmd);
	}

	/* Reset semaphores. */
	k_sem_reset(&test_fixture.cmd_sem);
	k_sem_reset(&test_fixture.opm_sem);

	ppm_queue_command_with_result(&test_fixture, UCSI_CMD_PPM_RESET,
				      /*result=*/0,
				      /*lpm_data=*/NULL);

	/* Open ppm_common implementation with fake driver for testing. */
	test_fixture.ppm = ppm_open(test_fixture.pd, test_fixture.port_status,
				    (const struct device *)&test_fixture);
}

static void ppm_common_test_after(void *f)
{
	/* Must clean up between tests to re-init the state machine. */
	test_fixture.ppm->cleanup(test_fixture.ppm);
}

const struct ucsi_cci cci_cmd_complete = { .cmd_complete = 1 };
const struct ucsi_cci cci_busy = { .busy = 1 };
const struct ucsi_cci cci_error = { .error = 1, .cmd_complete = 1 };
const struct ucsi_cci cci_ack_command = { .ack_command = 1 };

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

/* From the Idle Notify, complete a full command loop:
 *   - Send command, CCI notifies busy
 *   - Command complete, CCI notifies command complete.
 *   - Send ACK_CC_CI, CCI notifies busy
 *   - Command complete, CCI notifies ack command complete.
 */
ZTEST_USER_F(ppm_common_test, test_IDLENOTIFY_full_command_loop)
{
	ppm_initialize_to_idle_notify(fixture);
	int notified_count = 0;
	fixture->notified_count = 0;

	/* Emulate a UCSI write from the OPM, and wait for a notification with
	 * CCI.busy=1
	 */
	struct ucsi_control control = { .command = UCSI_CMD_GET_ALTERNATE_MODES,
					.data_length = 0 };
	zassert_false(ppm_write_command(fixture, &control) < 0);
	zassert_true(ppm_wait_for_notification(fixture, ++notified_count));
	zassert_true(ppm_cci_matches(fixture, &cci_busy));

	/* Send a fake response from the PD driver, and expect a notification to
	 * the OPM with CCI.cmd_complete=1.
	 */
	ppm_complete_specific_command(fixture, UCSI_CMD_GET_ALTERNATE_MODES,
				      /*result=*/0, /*lpm_data=*/NULL);
	zassert_true(ppm_wait_for_cmd_to_process(fixture));
	zassert_true(ppm_wait_for_notification(fixture, ++notified_count));
	zassert_true(ppm_cci_matches(fixture, &cci_cmd_complete));

	/* OPM acknowledges the PPM's cmd_complete. */
	ppm_queue_command_with_result(fixture, UCSI_CMD_ACK_CC_CI,
				      /*result=*/0,
				      /*lpm_data=*/NULL);
	zassert_false(ppm_write_ack_command(fixture,
					    /*connector_change_ack*/ false,
					    /*command_complete_ack*/ true) < 0);
	zassert_true(ppm_wait_for_notification(fixture, ++notified_count));
	zassert_true(ppm_cci_matches(fixture, &cci_ack_command));
	zassert_equal(get_ppm_data(fixture)->ppm_state, PPM_STATE_IDLE_NOTIFY);
}

/* When processing an async event, PPM will figure out which port changed and
 * then send the connector change event for that port.
 */
ZTEST_USER_F(ppm_common_test,
	     test_IDLENOTIFY_process_async_event_and_send_connector_change)
{
	ppm_initialize_to_idle_notify(fixture);

	int notified_count = 0;
	fixture->notified_count = 0;

	ppm_trigger_connector_changed(fixture, PDC_DEFAULT_CONNECTOR);
	zassert_true(ppm_wait_for_async_event_to_process(fixture));
	zassert_true(ppm_wait_for_notification(fixture, ++notified_count));

	struct ucsi_cci cci = { .connector_changed = PDC_DEFAULT_CONNECTOR };
	zassert_true(ppm_cci_matches(fixture, &cci));
}
