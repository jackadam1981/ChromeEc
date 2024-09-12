/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_DRIVERS_USBC_PDC_CONFIG_H
#define ZEPHYR_DRIVERS_USBC_PDC_CONFIG_H

#include "drivers/ucsi_v3.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>

/**
 * @brief PDC Config object
 */
struct pdc_config_t {
	/** I2C config */
	struct i2c_dt_spec i2c;
	/** pdc power path interrupt */
	struct gpio_dt_spec irq_gpios;
	/** connector number of this port */
	uint8_t connector_number;
	/** Notification enable bits */
	union notification_enable_t bits;
	/** Create thread function */
	void (*create_thread)(const struct device *dev);
	/** Process event function */
	void (*process_event)(const struct device *dev, uint32_t event);
	/** If true, do not apply PDC FW updates to this port */
	bool no_fw_update;
};

#endif /* ZEPHYR_DRIVERS_USBC_PDC_CONFIG_H */