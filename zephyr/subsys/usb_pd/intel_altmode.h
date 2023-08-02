/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Header file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#ifndef __INTEL_ALTMODE_H
#define __INTEL_ALTMODE_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

struct pd_altmode_driver {
	int (*read)(const struct device *dev);
	int (*write)(const struct device *dev);
	int (*isr_enable)(const struct device *dev, bool en);
};

struct pd_altmode_config {
	/* I2C config */
	struct i2c_dt_spec i2c;
	/*
	 * PD interrupt to wake the task to configure alternate modes. There
	 * can be individual Interrupt pin for each PD port or all the PD
	 * interrupts can be muxed to single GPIO. This helps to keep common
	 * code for single port / dual port PD solutions offered by different
	 * PD vendors.
	 */
	struct gpio_dt_spec int_gpio;
};

struct pd_altmode_data {
	const struct device *dev;
	struct gpio_callback gpio_cb;
};

#endif /* __INTEL_ALTMODE_H */
