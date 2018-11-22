/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_GPIO, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ## args)

#define I2C_ADDR_OZ554		0x62
#define OZ554_DATA_SIZE		6

struct oz554_value {
	uint8_t offset;
	uint8_t data;
};

/* This ordering is suggested by vendor. */
static const struct oz554_value oz554_order[] = {
	{.offset = 1, .data = 0x43},
	{.offset = 2, .data = 0x65},
	{.offset = 3, .data = 0x00},
	{.offset = 4, .data = 0x00},
	{.offset = 5, .data = 0x97},
	{.offset = 0, .data = 0xF2},
};

static void set_oz554_reg(void)
{
	int i, rv;

	for (i = 0; i < OZ554_DATA_SIZE; ++i) {
		rv = i2c_write8(
			NPCX_I2C_PORT1,
			I2C_ADDR_OZ554,
			oz554_order[i].offset,
			oz554_order[i].data);

		if (rv)
			CPRINTS("Write OZ554 register index %d FAILED!", i);
	}
}

static void backlight_enable_deferred(void)
{
	if (gpio_get_level(GPIO_PANEL_BACKLIGHT_EN))
		set_oz554_reg();
}
DECLARE_DEFERRED(backlight_enable_deferred);

void backlight_enable_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&backlight_enable_deferred_data, 30 * MSEC);
}

/* Initialize board. */
static void karma_init(void)
{
	/* Enable panel backlight interrupt. */
	gpio_enable_interrupt(GPIO_PANEL_BACKLIGHT_EN);
}
DECLARE_HOOK(HOOK_INIT, karma_init, HOOK_PRIO_DEFAULT);
