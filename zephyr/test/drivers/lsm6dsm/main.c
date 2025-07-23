/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/accelgyro_lsm6dsm_public.h"
#include "emul/emul_lsm6dsm.h"
#include "motion_sense.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>

#define LSM6DSM_NODE DT_NODELABEL(lsm6dsm_emul)
#define ACC_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_lsm6dsm_accel))
#define GYR_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_lsm6dsm_gyro))

static const struct emul *emul = EMUL_DT_GET(LSM6DSM_NODE);
static struct motion_sensor_t *acc = &motion_sensors[ACC_SENSOR_ID];
// static struct motion_sensor_t *gyr = &motion_sensors[GYR_SENSOR_ID];

ZTEST_SUITE(lsm6dsm, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(lsm6dsm, test_disable_interrupt)
{
	const struct gpio_dt_spec spec =
		GPIO_DT_SPEC_GET(LSM6DSM_NODE, int_gpios);

	/* Set data rate to 10Hz, without this the FIFO will never be enabled */
	acc->drv->set_data_rate(acc, 13000, 1);

	/* Disable the interrupts */
	acc->drv->enable_interrupt(acc, false);

	/* Add an accel sample (1g, 0g, 0g) */
	emul_lsm6dsm_append_sample(emul, MOTIONSENSE_TYPE_ACCEL, 1.0f, 0.0f,
				   0.0f);

	/* Interrupt should be deasserted */
	zassert_equal(0, gpio_pin_get_dt(&spec));

	/* Flush the FIFO */
	uint32_t event = CONFIG_ACCEL_LSM6DSM_INT_EVENT;
	zassert_equal(EC_SUCCESS, acc->drv->irq_handler(acc, &event));

	/* Enable the interrupts */
	acc->drv->enable_interrupt(acc, true);

	/* Add another accel sample (1g, 0g, 0g) */
	emul_lsm6dsm_append_sample(emul, MOTIONSENSE_TYPE_ACCEL, 1.0f, 0.0f,
				   0.0f);

	/* Interrupt should be asserted */
	zassert_equal(1, gpio_pin_get_dt(&spec));
}
