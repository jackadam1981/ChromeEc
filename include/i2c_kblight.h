/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C Keyboard Backlight API for Chrome EC */

#ifndef __CROS_EC_I2C_KBLIGHT_H
#define __CROS_EC_I2C_KBLIGHT_H

struct i2c_kblight_drv {
//	void (*init)(void);
//	void (*preserve_state)(void);
	void (*i2c_light_set)(int percent);
	int (*i2c_light_get)(void);
	void (*i2c_light_enable)(int);
	int (*i2c_light_state)(void);
};

int i2c_kblight_driver_register(struct i2c_kblight_drv *drv);
void i2c_kblight_set(int percent);
int i2c_kblight_get(void);
void i2c_kblight_enable(int enable);
int i2c_kblight_state(void);

#endif  /* __CROS_EC_I2C_KBLIGHT_H */
