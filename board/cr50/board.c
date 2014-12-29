/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "i2c.h"

#include "gpio_list.h"

void button_event(enum gpio_signal signal)
{
	int val = gpio_get_level(signal);
	ccprintf("Button %d = %d\n", signal, gpio_get_level(signal));

	gpio_set_level(GPIO_LED_4, val);
}

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_SW_N);
	gpio_enable_interrupt(GPIO_SW_S);
	gpio_enable_interrupt(GPIO_SW_W);
	gpio_enable_interrupt(GPIO_SW_E);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);


/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
		GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
	{"slave",  I2C_PORT_SLAVE, 100,
		GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

