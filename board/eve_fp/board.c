/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "spi.h"
#include "registers.h"

#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/* Interrupt line from the fingerprint senser */
void fps_event(enum gpio_signal signal)
{
	/* HACK: Forward interrupt to AP */
	gpio_set_level(GPIO_AP_INT, gpio_get_level(GPIO_FPS_INT));
	CPRINTS("FPS %d\n", gpio_get_level(GPIO_FPS_INT));
}

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor */
	{ CONFIG_SPI_SENSOR_PORT, 1, GPIO_SPI3_NSS }
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);
