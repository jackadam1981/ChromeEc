/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Servo micro board configuration */

#include "common.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "queue_policies.h"
#include "registers.h"
#include "task.h"
#include "usb_descriptor.h"
#include "util.h"


/******************************************************************************
 * Build GPIO tables and expose a subset of the GPIOs over USB.
 */

#include "gpio_list.h"

static enum gpio_signal const usb_gpio_list[] = {
};

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
		GPIO_MASTER_I2C1_SCL, GPIO_MASTER_I2C1_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);


/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
