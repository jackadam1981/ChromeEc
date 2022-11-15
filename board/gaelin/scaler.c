/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "console.h"
#include "gpio.h"
#include "i2c.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

#define SCALER_NAV_KEY_REG 0x0381
#define SCALER_VOL_UP 0x01
#define SCALER_VOL_DOWN 0x02
#define SCALER_BRIGHTNESS_UP 0x04
#define SCALER_BRIGHTNESS_DOWN 0x08

/*
 * TODO: Need to add the long press function.
 * How to implementation?
 */
void osd_int_interrupt(enum gpio_signal signal)
{
}

/*
 * TODO: Need to add the display mode function.
 * How to implementation?
 */
void disp_mode_interrupt(enum gpio_signal signal)
{
}

void hdmi0_cable_det_interrupt(enum gpio_signal signal)
{
	gpio_set_level(GPIO_EC_OVERRIDE_SCLR_EN, 1);
	gpio_set_level(GPIO_EC_12VSC_EN, 1);
	gpio_set_level(GPIO_EC_AMP_SD, 1);
}
