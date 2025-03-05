// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define DT_DRV_COMPAT egis_egis630

#include "fingerprint_egis630.h"
#include "fingerprint_egis630_private.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/linker/section_tags.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/fingerprint.h>

LOG_MODULE_REGISTER(cros_fingerprint, LOG_LEVEL_INF);

static inline int egis630_disable_irq(const struct device *dev)
{
	const struct ec630_cfg *cfg = dev->config;
	int rc;

	rc = gpio_pin_interrupt_configure_dt(&cfg->interrupt, GPIO_INT_DISABLE);
	if (rc < 0) {
		LOG_ERR("Can't disable interrupt: %d", rc);
	}

	return rc;
}

static void egis630_enable_irq(const struct device *dev,
			       struct gpio_callback *cb, uint32_t pins)
{
	struct egis630_data *data =
		CONTAINER_OF(cb, struct egis630_data, irq_cb);

	egis630_disable_irq(data->dev);

	if (data->callback != NULL) {
		data->callback(dev);
	}
}
