/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C Keyboard Backlight API for Chrome EC */

#ifndef __CROS_EC_I2C_KBLIGHT_H
#define __CROS_EC_I2C_KBLIGHT_H

/**
 * The caller must register the underlying i2c
 * callback function for the keyboard backlight.
 */
struct i2c_kblight_drv {
	/**
	 * Get the brightness percentage
	 *
	 * @return the percentage of the keyboard backlight brightness.
	 */
	int (*i2c_get_brightness)(void);

	/**
	 * Set the brightness percentage
	 *
	 * @percent the brightness percentage to be set to keyboard backlight
	 */
	void (*i2c_set_brightness)(int percent);

	/**
	 * Enable/Disable the keyboard backlight function
	 *
	 * @en 1:enable, 0: disable
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*i2c_enable)(int enable);

	/**
	 * Get the enable state of keyboard backlight
	 *
	 * @return 0:disable, 1:enable
	 */
	int (*i2c_state)(void);
};

/**
 * Register i2c as the keyboard backlight underlying interface.
 * This function will register the callback function for the
 * keyboard backlight driver.
 */
void i2c_kblight_register(struct i2c_kblight_drv *drv);

#endif  /* __CROS_EC_I2C_KBLIGHT_H */
