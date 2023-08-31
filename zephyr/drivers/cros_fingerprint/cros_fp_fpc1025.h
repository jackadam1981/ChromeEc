/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_FP_FPC1025_H__
#define __CROS_FP_FPC1025_H__

#include <drivers/cros_fingerprint.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

struct fpc1025_cfg {
	struct spi_dt_spec spi;
	struct gpio_dt_spec interrupt;
	struct gpio_dt_spec reset_pin;
	struct fingerprint_info info;
};

struct fpc1025_data {
	uint16_t errors;
};

#endif /* __CROS_FP_FPC1025_H__ */
