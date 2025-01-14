/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Driver for anx7688 USB-C switch.
 */

#ifndef __CROS_EC_ANX7688_MUX_H__
#define __CROS_EC_ANX7688_MUX_H__

#include "compile_time_macros.h"
#include "gpio_signal.h"
#include "usb_mux.h"

#define ANX7688_I2C_ADDR0_FLAGS 0x2c
#define ANX7688_I2C_ADDR1_FLAGS 0x28

extern const struct usb_mux_driver anx7688_mux_driver;

/* ADDR0 register define */
#define ANX7688_OCM_CTRL	0x83

/* ADDR1 register define */
#define ANX7688_ANA_CTRL1	0x42
#define ANX7688_ANA_CTRL2	0x43
#define ANX7688_ANA_CTRL5	0x46
#define ANX7688_GPIO_MAP5	0x62
#define ANX7688_GPIO_CTRL0	0x63
#define ANX7688_GPIO_CTRL1	0x64

#define ANX7688_POWER_STANDBY	0x00
#define ANX7688_POWER_INIT	0x01
#define ANX7688_POWER_EN	0x02
#define ANX7688_POWER_RESET	0x03
#define ANX7688_POWER_ON	0x04
#define ANX7688_POWER_ON_SET	0x05

struct anx7688_control_t {
	enum gpio_signal pwr3v3;
	enum gpio_signal pwr_en;
	enum gpio_signal reset_n;
};

extern struct anx7688_control_t anx7688_control;

void anx7688_mux_power_on(const struct usb_mux *me);
void anx7688_mux_standby(const struct usb_mux *me);

#endif /* __CROS_EC_ANX7688_MUX_H__ */
