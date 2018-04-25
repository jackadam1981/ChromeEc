/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Keyboard backlight control
 */

#include "hooks.h"
#include "lm3509.h"
#include "cros_board_info.h"
#include "kblight.h"
#include "pwm_kblight.h"

/*
 * I2C routines
 */
static void kblight_i2c_set(int percent)
{
	lm3509_set_brightness(percent);
}

static int kblight_i2c_get(void)
{
	int percent;
	if (lm3509_get_brightness(&percent))
		percent = 0;
	return percent;
}

static int kblight_i2c_power(int enable)
{
	lm3509_power(enable);
	return EC_SUCCESS;
}

static int kblight_i2c_is_enable(void)
{
	return lm3509_is_enable();
}

static struct kblight_drv i2c_kblight_drv = {
	.get = kblight_i2c_get,
	.set = kblight_i2c_set,
	.enable = kblight_i2c_power,
	.is_enable = kblight_i2c_is_enable,
};

static void kblight_config(void)
{
	uint32_t oem = PROJECT_NAMI;
	uint32_t sku = 0;

	cbi_get_oem_id(&oem);
	cbi_get_sku_id(&sku);

	switch (oem) {
	default:
	case PROJECT_NAMI:
	case PROJECT_VAYNE:
	case PROJECT_PANTHEON:
		kblight_driver_register(&i2c_kblight_drv);
		break;
	case PROJECT_SONA:
		if (sku == 0x3AE2)
			break;
		pwm_kblight_register();
		break;
	}
}
DECLARE_HOOK(HOOK_INIT, kblight_config, HOOK_PRIO_INIT_PWM + 1);
