/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "button.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "motionsense_sensors.h"
#include "nissa_sub_board.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

#define LIS_ALT_MAT SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_bma422))
#define BMA_ALT_MAT SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_ref))
#define BASE_NORMAL_MAT SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(base_rot_ref))
#define BASE_INVERTED_MAT SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(base_rot_ver1))
#define LID_SENSOR SENSOR_ID(DT_NODELABEL(lid_accel))
#define BASE_SENSOR SENSOR_ID(DT_NODELABEL(base_accel))
#define BASE_GYRO SENSOR_ID(DT_NODELABEL(base_gyro))
#define ALT_LID_S SENSOR_ID(DT_NODELABEL(alt_lid_accel))

void form_factor_init(void);
void buttons_init(void);

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VALUE_FUNC(int, cbi_get_board_version, uint32_t *);
FAKE_VALUE_FUNC(enum nissa_sub_board_type, nissa_get_sb_type);
FAKE_VOID_FUNC(usb_interrupt_c1, enum gpio_signal);
FAKE_VOID_FUNC(button_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(bmi3xx_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(lsm6dso_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(bma4xx_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(lis2dw12_interrupt, enum gpio_signal);
FAKE_VOID_FUNC(gmr_tablet_switch_disable);

static void test_before(void *fixture)
{
	RESET_FAKE(cbi_get_board_version);
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(nissa_get_sb_type);
}

ZTEST_SUITE(craask, NULL, NULL, test_before, NULL, NULL);

static int board_version;

static int cbi_get_board_version_mock(uint32_t *value)
{
	*value = board_version;
	return 0;
}

ZTEST(craask, test_base_orientation)
{
	const void *const normal_rotation = &BASE_NORMAL_MAT;
//		&SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(base_rot_ref));
	const void *const inverted_rotation = &BASE_INVERTED_MAT;
//		&SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(base_rot_ver1));

	/*
	 * Normally this gets set to rot-standard-ref during other init,
	 * which we aren't running in this test.
	 */
//	motion_sensors[BASE_SENSOR].rot_standard_ref = normal_rotation;

	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_mock;

	board_version = 2;
	form_factor_init();
	zassert_equal_ptr(
		motion_sensors[BASE_SENSOR].rot_standard_ref, normal_rotation,
		"normal orientation should use the standard rotation matrix");

	board_version = 1;
	form_factor_init();
	zassert_equal_ptr(
		motion_sensors[BASE_SENSOR].rot_standard_ref, inverted_rotation,
		"inverted orientation should use the inverted rotation matrix");
}

static bool lid_inversed;

static int get_lid_orientation_config(enum cbi_fw_config_field_id field,
				      uint32_t *value)
{
	if (field == FW_LID_INVERSION)
		*value = lid_inversed ? FW_LID_XY_ROT_180 : FW_LID_REGULAR;
	else
		return -EINVAL;
	return 0;
}

ZTEST(craask, test_lid_orientation)
{
//	const int LID_ACCEL = SENSOR_ID(DT_NODELABEL(lid_accel));
	const void *const normal_rotation =
		&SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_ref));
	const void *const inverted_rotation =
		&SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_bma422));

	/*
	 * Normally this gets set to rot-standard-ref during other init,
	 * which we aren't running in this test.
	 */
	motion_sensors[LID_SENSOR].rot_standard_ref = normal_rotation;

	cros_cbi_get_fw_config_fake.custom_fake = get_lid_orientation_config;

	lid_inversed = false;
	form_factor_init();
	zassert_equal_ptr(
		motion_sensors[LID_SENSOR].rot_standard_ref, normal_rotation,
		"normal orientation should use the standard rotation matrix");

	lid_inversed = true;
	form_factor_init();
	zassert_equal_ptr(
		motion_sensors[LID_SENSOR].rot_standard_ref, inverted_rotation,
		"inverted orientation should use the inverted rotation matrix");
}
/*
enum test_button {
	BUTTON_VOLUME_UP,
	BUTTON_VOLUME_DOWN,
};

struct button_config buttons[BUTTON_COUNT] = {
	[BUTTON_VOLUME_UP] = {
		.name = "Volume Up",
		.type = KEYBOARD_BUTTON_VOLUME_UP, 
		.gpio = GPIO_VOLUME_UP_L,
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},

	[BUTTON_VOLUME_DOWN] = {
		.name = "Volume Down",
		.type = KEYBOARD_BUTTON_VOLUME_DOWN,
		.gpio = GPIO_VOLUME_DOWN_L,
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},
};
*/
ZTEST(craask, test_volum_up_dn_buttons)
{
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_mock;

	nissa_get_sb_type_fake.return_val = NISSA_SB_NONE;

	board_version = 1;
	buttons_init();
	zassert_equal(buttons[BUTTON_VOLUME_UP].gpio, GPIO_VOLUME_UP_L);
	zassert_equal(buttons[BUTTON_VOLUME_DOWN].gpio, GPIO_VOLUME_DOWN_L);
}
