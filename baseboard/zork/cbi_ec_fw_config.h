/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _ZORK_CBI_EC_FW_CONFIG__H_
#define _ZORK_CBI_EC_FW_CONFIG__H_

/****************************************************************************
 * CBI Zork EC FW Configuration
 */
extern int cbi_get_fw_config(uint32_t *val);
static inline uint32_t get_cbi_fw_config(void)
{
	uint32_t val;

	return (cbi_get_fw_config(&val)) ? 0 : val;
}

/*
 * USB Main Board (4 bits)
 *
 * get_cbi_ec_cfg_usb_mb() will return the MB option number.
 * The option number will be defined in a variant or board level enumeration
 */
#define EC_CFG_USB_MB_L				0
#define EC_CFG_USB_MB_H				3
#define EC_CFG_USB_MB_MASK \
				GENMASK(EC_CFG_USB_MB_H,\
					EC_CFG_USB_MB_L)
#define get_cbi_ec_cfg_usb_mb() \
	((get_cbi_fw_config() & EC_CFG_USB_MB_MASK) \
		>> EC_CFG_USB_MB_L)

/*
 * USB Daughter Board (4 bits)
 *
 * get_cbi_ec_cfg_usb_db() will return the DB option number.
 * The option number will be defined in a variant or board level enumeration
 */
#define EC_CFG_USB_DB_L				4
#define EC_CFG_USB_DB_H				7
#define EC_CFG_USB_DB_MASK \
				GENMASK(EC_CFG_USB_DB_H,\
					EC_CFG_USB_DB_L)
#define get_cbi_ec_cfg_usb_db() \
	((get_cbi_fw_config() & EC_CFG_USB_DB_MASK) \
		>> EC_CFG_USB_DB_L)

/*
 * PWM Keyboard Backlight (1 bit)
 *
 * get_cbi_ec_cfg_pwm_keyboard_backlight() will return 1 is present or 0
 */
enum ec_cfg_pwm_keyboard_backlight_type {
	PWM_KEYBOARD_BACKLIGHT_NONE = 0,
	PWM_KEYBOARD_BACKLIGHT_PRESENT = 1,
};
#define EC_CFG_PWM_KEYBOARD_BACKLIGHT_L		8
#define EC_CFG_PWM_KEYBOARD_BACKLIGHT_H		8
#define EC_CFG_PWM_KEYBOARD_BACKLIGHT_MASK \
				GENMASK(EC_CFG_PWM_KEYBOARD_BACKLIGHT_H,\
					EC_CFG_PWM_KEYBOARD_BACKLIGHT_L)

static inline enum ec_cfg_pwm_keyboard_backlight_type
ec_config_has_pwm_keyboard_backlight(void)
{
	return ((get_cbi_fw_config() & EC_CFG_PWM_KEYBOARD_BACKLIGHT_MASK)
			>> EC_CFG_PWM_KEYBOARD_BACKLIGHT_L);
}

/*
 * Lid Angle Tablet Mode (1 bit)
 *
 * get_cbi_ec_cfg_lid_angle_tablet_mode() will return 1 is present or 0
 */
enum ec_cfg_lid_angle_tablet_mode_type {
	LID_ANGLE_TABLET_MODE_NONE = 0,
	LID_ANGLE_TABLET_MODE_PRESENT = 1,
};
#define EC_CFG_LID_ANGLE_TABLET_MODE_L		9
#define EC_CFG_LID_ANGLE_TABLET_MODE_H		9
#define EC_CFG_LID_ANGLE_TABLET_MODE_MASK \
				GENMASK(EC_CFG_LID_ANGLE_TABLET_MODE_H,\
					EC_CFG_LID_ANGLE_TABLET_MODE_L)

static inline enum ec_cfg_lid_angle_tablet_mode_type
ec_config_has_lid_angle_tablet_mode(void)
{
	return ((get_cbi_fw_config() & EC_CFG_LID_ANGLE_TABLET_MODE_MASK)
			>> EC_CFG_LID_ANGLE_TABLET_MODE_L);
}

/*
 * Lid Accelerometer Sensor (3 bits)
 *
 * get_cbi_ec_cfg_lid_accel_sensor() will return ec_cfg_lid_accel_sensor_type
 */
enum ec_cfg_lid_accel_sensor_type {
	LID_ACCEL_NONE = 0,
	LID_ACCEL_KX022 = 1,
	LID_ACCEL_LIS2DWL = 2,
};
#define EC_CFG_LID_ACCEL_SENSOR_L		10
#define EC_CFG_LID_ACCEL_SENSOR_H		12
#define EC_CFG_LID_ACCEL_SENSOR_MASK	\
				GENMASK(EC_CFG_LID_ACCEL_SENSOR_H,\
					EC_CFG_LID_ACCEL_SENSOR_L)

static inline enum ec_cfg_lid_accel_sensor_type
ec_config_has_lid_accel_sensor(void)
{
	return ((get_cbi_fw_config() & EC_CFG_LID_ACCEL_SENSOR_MASK)
			>> EC_CFG_LID_ACCEL_SENSOR_L);
}

/*
 * Base Gyro Sensor (3 bits)
 *
 * get_cbi_ec_cfg_base_gyro_sensor() will return ec_cfg_base_gyro_type
 */
enum ec_cfg_base_gyro_sensor_type {
	BASE_GYRO_NONE = 0,
	BASE_GYRO_BMI160 = 1,
	BASE_GYRO_LSM6DSM = 2,
};
#define EC_CFG_BASE_GYRO_SENSOR_L		13
#define EC_CFG_BASE_GYRO_SENSOR_H		15
#define EC_CFG_BASE_GYRO_SENSOR_MASK	\
				GENMASK(EC_CFG_BASE_GYRO_SENSOR_H,\
					EC_CFG_BASE_GYRO_SENSOR_L)

static inline enum ec_cfg_base_gyro_sensor_type
ec_config_has_base_gyro_sensor(void)
{
	return ((get_cbi_fw_config() & EC_CFG_BASE_GYRO_SENSOR_MASK)
			>> EC_CFG_BASE_GYRO_SENSOR_L);
}

#endif /* _ZORK_CBI_EC_FW_CONFIG__H_ */
