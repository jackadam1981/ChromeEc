/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_cbi.h"
#include "motionsense_sensors.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

#define LID_ACCEL SENSOR_ID(DT_NODELABEL(lid_accel))

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);

static void joxer_form_factor_before(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(cros_cbi_get_fw_config);
}

ZTEST_SUITE(joxer_form_factor, NULL, NULL, joxer_form_factor_before, NULL,
	    NULL);

static int get_base_orientation_normal(enum cbi_fw_config_field_id field,
				       uint32_t *value)
{
	zassert_equal(field, FW_LID_INVERSION);
	*value = SENSOR_DEFAULT;
	return 0;
}

static int get_base_orientation_inverted(enum cbi_fw_config_field_id field,
					 uint32_t *value)
{
	zassert_equal(field, FW_LID_INVERSION);
	*value = SENSOR_INVERTED;
	return 0;
}

ZTEST(joxer_form_factor, test_lid_sensor_inversion)
{
	const void *const normal_rotation =
		&SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_ref));
	const void *const inverted_rotation =
		&SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_inverted));

	/*
	 * Normally this gets set to rot-standard-ref during other init,
	 * which we aren't running in this test.
	 */
	motion_sensors[LID_ACCEL].rot_standard_ref = normal_rotation;

	cros_cbi_get_fw_config_fake.custom_fake = get_base_orientation_normal;
	form_factor_init();
	zassert_equal_ptr(
		motion_sensors[LID_ACCEL].rot_standard_ref, normal_rotation,
		"normal orientation should use the standard rotation matrix");

	RESET_FAKE(cros_cbi_get_fw_config);
	cros_cbi_get_fw_config_fake.return_val = EINVAL;
	form_factor_init();
	zassert_equal_ptr(motion_sensors[LID_ACCEL].rot_standard_ref,
			  normal_rotation,
			  "errors should leave the rotation unchanged");

	cros_cbi_get_fw_config_fake.custom_fake = get_base_orientation_inverted;
	form_factor_init();
	zassert_equal_ptr(
		motion_sensors[LID_ACCEL].rot_standard_ref, inverted_rotation,
		"inverted orientation should use the inverted rotation matrix");
}
