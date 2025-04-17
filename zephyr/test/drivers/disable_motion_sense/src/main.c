/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "ec_tasks.h"
#include "hooks.h"
#include "motion_sense.h"
#include "task.h"
#include "test/drivers/test_state.h"

#include <zephyr/ztest.h>

extern enum chipset_state_mask sensor_active;

ZTEST_SUITE(motion_sense, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(motion_sense, test_disable_sensor_stack)
{
	task_wake(TASK_ID_MOTIONSENSE);
	k_msleep(10);
	sensor_stack_runtime_disable();
	k_msleep(10);
	task_wake(TASK_ID_MOTIONSENSE);
	k_msleep(10);
	k_thread_join(task_id_to_thread_id(TASK_ID_MOTIONSENSE), K_FOREVER);

	zassert_equal(sensor_active, SENSOR_ACTIVE_S5);
	for (int i = 0; i < motion_sensor_count; ++i) {
		struct motion_sensor_t *s = &motion_sensors[i];

		zassert_equal(s->config[SENSOR_CONFIG_AP].odr, 0,
			      "Sensor [%d] odr expected to be 0 but was %d", i,
			      s->config[SENSOR_CONFIG_AP].odr);
		zassert_equal(s->config[SENSOR_CONFIG_AP].ec_rate, 0,
			      "Sensor [%d] ec_rate expected to be 0 but was %d",
			      i, s->config[SENSOR_CONFIG_AP].ec_rate);
	}

	/* Run the suspend hook and ensure it did nothing */
	sensor_active = SENSOR_ACTIVE_S0;
	hook_notify(HOOK_CHIPSET_SUSPEND);
	k_msleep(10);
	zassert_equal(sensor_active, SENSOR_ACTIVE_S0);

	/* Run the resume hook and ensure it did nothing */
	sensor_active = SENSOR_ACTIVE_S5;
	hook_notify(HOOK_CHIPSET_RESUME);
	k_msleep(10);
	zassert_equal(sensor_active, SENSOR_ACTIVE_S5);
}
