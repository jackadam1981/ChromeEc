/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Driver file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#define DT_DRV_COMPAT intel_pd_altmode

static int pd_altmode_read(const struct device *dev)
{
	return 0;
}

static int pd_altmode_write(const struct device *dev)
{
	return 0;
}

static const struct pd_altmode_driver pd_altmode_driver_api = {
	.read = pd_altmode_read,
	.write = pd_altmode_write,
};

static int pd_altmode_init(const struct device *dev)
{
	return 0;
}

#define INTEL_ALTMODE_DEFINE(inst)                                        \
	static struct pd_altmode_data pd_altmode_data_##inst;             \
                                                                          \
	static const struct pd_altmode_config pd_altmode_config##inst = { \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
	};                                                                \
                                                                          \
	DEVICE_DT_INST_DEFINE(inst, pd_altmode_init, NULL,                \
			      &pd_altmode_data_##inst,                    \
			      &pd_altmode_config##inst, POST_KERNEL,      \
			      CONFIG_APPLICATION_INIT_PRIORITY,           \
			      &pd_altmode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(INTEL_ALTMODE_DEFINE)
