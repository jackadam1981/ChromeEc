/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* /\* Initialize board. *\/ */
/* static void board_init(void) */
/* { */
/* 	/\* Deassert reset to TCPCs *\/ */
/* 	gpio_set_level(GPIO_USB_C0_RST_L, 1); */
/* 	gpio_set_level(GPIO_USB_C1_RST_L, 1); */
/* } */
/* DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT); */

int system_set_console_force_enabled(int enabled)
{
	return 0;
}

int system_get_console_force_enabled(void)
{
	return 0;
}

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"tcpc0",   I2C_PORT_TCPC0, 1000, GPIO_I2C1_SCL,  GPIO_I2C1_SDA},
	{"tcpc1",   I2C_PORT_TCPC1, 1000, GPIO_I2C2_SCL,  GPIO_I2C2_SDA},
	{"tcpc2",   I2C_PORT_TCPC2, 1000, GPIO_I2C5_SCL,  GPIO_I2C5_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Dummy ISR */
void extpower_interrupt(enum gpio_signal signal) {}
void lid_interrupt(enum gpio_signal signal) {}
void power_button_interrupt(enum gpio_signal signal) {}
void tablet_mode_interrupt(enum gpio_signal signal) {}
void button_interrupt(enum gpio_signal signal) {}
void switch_interrupt(enum gpio_signal signal) {}
void power_signal_interrupt(enum gpio_signal signal) {}
void vbus0_evt(enum gpio_signal signal) {}
void tcpc_alert_event(enum gpio_signal signal) {}
void vbus1_evt(enum gpio_signal signal) {}
void host_set_events(uint32_t mask) {}

#include "gpio_list.h"

