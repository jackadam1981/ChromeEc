/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chromebook i2c keyboard backlight interface. */

#include "hooks.h"
#include "kblight.h"
#include "i2c_kblight.h"

static struct i2c_kblight_drv *i2c_drv;

static int i2c_kblight_init(void)
{
	/* Reserve for the common i2c keyboard light initialization */
	return EC_SUCCESS;
}

static void i2c_kblight_preserve_state(void)
{
	/* Reserve for the common i2c keyboard light RO data preserve */
}

static int i2c_kblight_get(void)
{
	if (i2c_drv && i2c_drv->i2c_get_brightness)
		return i2c_drv->i2c_get_brightness();
	return 0;
}

static void i2c_kblight_set(int percent)
{
	if (i2c_drv && i2c_drv->i2c_set_brightness)
		i2c_drv->i2c_set_brightness(percent);
}

static int i2c_kblight_enable(int enable)
{
	if (i2c_drv && i2c_drv->i2c_enable)
		return i2c_drv->i2c_enable(enable);
	return EC_ERROR_INVALID_CONFIG;
}

static int i2c_kblight_state(void)
{
	if (i2c_drv && i2c_drv->i2c_state)
		return i2c_drv->i2c_state();
	return 0;
}

static struct kblight_drv i2c_kblight_drv = {
	.init = i2c_kblight_init,
	.preserve_state = i2c_kblight_preserve_state,
	.get = i2c_kblight_get,
	.set = i2c_kblight_set,
	.enable = i2c_kblight_enable,
	.state = i2c_kblight_state,
};

void i2c_kblight_register(struct i2c_kblight_drv *drv)
{
	i2c_drv = drv;
	kblight_driver_register(&i2c_kblight_drv);
}
