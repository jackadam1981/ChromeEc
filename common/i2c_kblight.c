/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Chromebook keyboard backlight. */

#include "host_command.h"
#include "ec_commands.h"
#include "util.h"
#include "hooks.h"
#include "i2c_kblight.h"

static int _percent=0;

static struct i2c_kblight_drv *i2c_kb_drv;

static void i2c_kblight_deferred(void)
{
	if (i2c_kb_drv && i2c_kb_drv->i2c_light_set)
		i2c_kb_drv->i2c_light_set(_percent);
}
DECLARE_DEFERRED(i2c_kblight_deferred);

int i2c_kblight_driver_register(struct i2c_kblight_drv *drv)
{
	i2c_kb_drv = drv;
	return 0;
}

void i2c_kblight_set(int percent)
{
	_percent = percent;
	hook_call_deferred(&i2c_kblight_deferred_data, 0);
}

int i2c_kblight_get(void)
{
	return _percent;
}

void i2c_kblight_enable(int enable)
{
	if (i2c_kb_drv && i2c_kb_drv->i2c_light_enable)
		i2c_kb_drv->i2c_light_enable(enable);
}

int i2c_kblight_state(void)
{
	if (i2c_kb_drv && i2c_kb_drv->i2c_light_state)
		return i2c_kb_drv->i2c_light_state();
	return 0;
}
