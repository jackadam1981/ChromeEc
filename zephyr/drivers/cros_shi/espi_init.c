/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/espi.h>
#include <logging/log.h>
#include <kernel.h>

LOG_MODULE_REGISTER(cros_shi_espi, CONFIG_ESPI_LOG_LEVEL);

#define espi_dev DEVICE_DT_GET(DT_CHOSEN(cros_ec_espi))

/*
 * Common eSPI intitialisation and configuration.
 * If necessary in the future, could use CONFIG_ or
 * DTS to override defaults.
 */
static int espi_init(const struct device *unused)
{
	struct espi_cfg cfg = {
		.io_caps = ESPI_IO_MODE_QUAD_LINES,
		.channel_caps = ESPI_CHANNEL_VWIRE | ESPI_CHANNEL_PERIPHERAL |
				ESPI_CHANNEL_OOB,
		.max_freq = 50,
	};

	if (!device_is_ready(espi_dev)) {
		k_oops();
	}

	/* Configure eSPI */
	if (espi_config(espi_dev, &cfg)) {
		LOG_ERR("Failed to configure eSPI device: %s", espi_dev->name);
	}
	return 0;
}
SYS_INIT(espi_init, APPLICATION, 0);
