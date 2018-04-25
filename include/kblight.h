/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Keyboard Backlight API for Chrome EC */

#ifndef __CROS_EC_KBLIGHT_H
#define __CROS_EC_KBLIGHT_H

struct kblight_drv {
	/**
	 * Initialize the keyboard backlight.
	 *
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*init)(void);

	/**
	 * Preserve the RO setting of keyboard backlight to avoid
	 * reset the keyboard backlight value. If the underlying
	 * transfer interface doesn't need to preserve the state,
	 * just set it to NULL
	 */
	void (*preserve_state)(void);

	/**
	 * Get the brightness percentage
	 *
	 * @return the percentage of the keyboard backlight brightness.
	 */
	int (*get)(void);

	/**
	 * Set the brightness percentage
	 *
	 * @percent the brightness percentage to be set to keyboard backlight
	 */
	void (*set)(int percent);

	/**
	 * Enable/Disable the keyboard backlight function
	 *
	 * @en 1:enable, 0: disable
	 * @return EC_SUCCESS if successful, non-zero if error.
	 */
	int (*enable)(int en);

	/**
	 * Get the enable state of keyboard backlight
	 *
	 * @return 0:disable, 1:enable
	 */
	int (*is_enable)(void);
};

/*****************************************************************************/
/* Expose APIs */

/**
 * Register the callback function to keyboard backlight driver.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int kblight_driver_register(struct kblight_drv *drv);

/**
 * Get the brightness percentage
 *
 * @return the percentage of the keyboard backlight brightness.
 */
int kblight_get(void);

/**
 * Set the brightness percentage
 *
 * @percent the brightness percentage to be set to keyboard backlight
 */
void kblight_set(int percent);


/**
 * Enable/Disable the keyboard backlight function
 *
 * @en 1:enable, 0: disable
 * @return EC_SUCCESS if successful, non-zero if error.
 */
void kblight_enable(int enable);

/**
 * Get the enable state of keyboard backlight
 *
 * @return 0:disable, 1:enable
 */
int kblight_is_enable(void);

#endif  /* __CROS_EC_KBLIGHT_H */
