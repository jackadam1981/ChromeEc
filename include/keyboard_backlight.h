/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_KEYBOARD_BACKLIGHT_H
#define __CROS_EC_KEYBOARD_BACKLIGHT_H

struct kblight_conf {
	const struct kblight_drv *drv;
};

struct kblight_drv {
	int (*init)(void);
	int (*set)(int percent);
	int (*get)(void);
	int (*enable)(int enable);
};

/**
 * Initialize keyboard backlight per board
 */
void board_kblight_init(void);

/**
 * Set keyboard backlight brightness
 *
 * @param percent Brightness in percentage
 * @return EC_SUCCESS or EC_ERROR_*
 */
int kblight_set(int percent);

/**
 * Get keyboard backlight brightness
 *
 * @return Brightness in percentage
 */
int kblight_get(void);

/**
 * Enable or disable keyboard backlight
 *
 * @param enable: 1=Enable, 0=Disable.
 * @return EC_SUCCESS or EC_ERROR_*
 */
int kblight_enable(int enable);

/**
 * Register keyboard backlight controller
 *
 * @param drv: Driver of keyboard backlight controller
 * @return EC_SUCCESS or EC_ERROR_*
 */
int kblight_register(const struct kblight_drv *drv);

extern const struct kblight_drv kblight_pwm;

#endif /* __CROS_EC_KEYBOARD_BACKLIGHT_H */
