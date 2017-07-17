/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "lb_common.h"

static void button_up_event(enum gpio_signal signal);
static void button_ri_event(enum gpio_signal signal);
static void button_dn_event(enum gpio_signal signal);
static void button_le_event(enum gpio_signal signal);

#include "gpio_list.h"

#define R(c) ((c>>16) & 0xff)
#define G(c) ((c>>8) & 0xff)
#define B(c) (c & 0xff)

#define BLACK   0x000000
#define WHITE   0xffffff
#define RED     0xff0000
#define LIME    0x00ff00
#define BLUE    0x0000ff
#define YELLOW  0xffff00
#define CYAN    0x00ffff
#define MAGENTA 0xff00ff
#define SILVER  0xc0c0c0
#define GRAY    0x808080
#define MAROON  0x800000
#define OLIVE   0x808000
#define GREEN   0x008000
#define VIOLET  0x800080
#define TEAL    0x000808
#define NAVY    0x000080

enum button {BUTTON_UP, BUTTON_RI, BUTTON_DN, BUTTON_LE};

struct Event {
	int led[2];
	int tog;
	int inc;
	int evt;
} button_evt[] = {
	{{GPIO_EC_LED0, GPIO_EC_LED1}, 1, 0, 0},
	{{GPIO_EC_LED2, GPIO_EC_LED3}, 1, 0, 0},
	{{GPIO_EC_LED4, GPIO_EC_LED5}, 1, 0, 0},
	{{GPIO_EC_LED6, GPIO_EC_LED7}, 1, 0, 0}
};

struct Bar {
	int color[16];
	int seg[4];
} bar = {
	{BLACK, WHITE, RED, GREEN, BLUE, YELLOW,
	 CYAN, MAGENTA, SILVER, GRAY, MAROON, OLIVE,
	 GREEN, VIOLET, TEAL, NAVY},
	{1, 5, 9, 13}
};

static void update_led(enum button but)
{
	if (!button_evt[but].evt) {
		gpio_set_level(button_evt[but].led[button_evt[but].inc],
							button_evt[but].tog);
		button_evt[but].inc++;
		if (button_evt[but].inc == 2) {
			button_evt[but].inc = 0;
			button_evt[but].tog = !button_evt[but].tog;
		}
		button_evt[but].evt = 1;
	} else {
		button_evt[but].evt = 0;
	}
}

static void button_up_event(enum gpio_signal signal)
{
	update_led(BUTTON_UP);
}

static void button_ri_event(enum gpio_signal signal)
{
	update_led(BUTTON_RI);
}

static void button_dn_event(enum gpio_signal signal)
{
	update_led(BUTTON_DN);
}

static void button_le_event(enum gpio_signal signal)
{
	update_led(BUTTON_LE);
}

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Not a real device */
	{"ECTemp", LM3_ADC_SEQ0, -225, ADC_READ_MAX, 420,
	 LM3_AIN_NONE, 0x0e /* TS0 | IE0 | END0 */, 0, 0},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"lcd", 0, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"lightbar",  0, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

void tick_event(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		lb_set_rgb(i, R(bar.color[bar.seg[i]]),
			G(bar.color[bar.seg[i]]), B(bar.color[bar.seg[i]]));
		bar.seg[i]++;
		bar.seg[i] = bar.seg[i] % 15;
	}
}
DECLARE_HOOK(HOOK_TICK, tick_event, HOOK_PRIO_DEFAULT);

static void board_init(void)
{
	lb_on();
	lb_init(1);
	gpio_enable_interrupt(GPIO_EC_UP);
	gpio_enable_interrupt(GPIO_EC_RI);
	gpio_enable_interrupt(GPIO_EC_DN);
	gpio_enable_interrupt(GPIO_EC_LE);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

