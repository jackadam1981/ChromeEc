// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZEPHYR_DRIVERS_FINGERPRINT_ELANI80SA_H_
#define ZEPHYR_DRIVERS_FINGERPRINT_ELANI80SA_H_

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#include <drivers/fingerprint.h>

struct elani80sa_cfg {
	struct spi_dt_spec spi;
	struct gpio_dt_spec interrupt;
	struct gpio_dt_spec reset_pin;
	struct fingerprint_info info;
};

struct elani80sa_data {
	const struct device *dev;
	fingerprint_callback_t callback;
	struct gpio_callback irq_cb;
	uint16_t errors;
};

#endif /* ZEPHYR_DRIVERS_FINGERPRINT_ELANI80SA_H_ */
