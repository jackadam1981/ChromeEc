/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fpsensor_detect.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "board_rw.h"

#ifndef SECTION_IS_RW
#error "This file should only be built for RW."
#endif

/* SPI devices */
struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor (SCLK at 4Mhz) */
	{ .port = CONFIG_SPI_FP_PORT, .div = 3, .gpio_cs = GPIO_SPI4_NSS }
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

static void spi_configure(enum fp_sensor_spi_select spi_select)
{
	if (spi_select == FP_SENSOR_SPI_SELECT_DEVELOPMENT) {
		/* SPI4 master to sensor: PE12/13/14 (CLK/MISO/MOSI) */
		gpio_set_flags_by_mask(GPIO_E, 0x7000, 0);
		gpio_set_alternate_function(GPIO_E, 0x7000, GPIO_ALT_SPI);
	} else {
		gpio_config_module(MODULE_SPI_CONTROLLER, 1);
	}

	/* Set all SPI master signal pins to very high speed: pins E2/4/5/6 */
	STM32_GPIO_OSPEEDR(GPIO_E) |= 0x00003f30;
	/* Enable clocks to SPI4 module (master) */
	STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI4;

	if (spi_select == FP_SENSOR_SPI_SELECT_DEVELOPMENT)
		spi_devices[0].gpio_cs = GPIO_SPI4_ALT_NSS;
	spi_enable(&spi_devices[0], 1);
}

void board_init_rw(void)
{
	enum fp_sensor_spi_select spi_select = get_fp_sensor_spi_select();

	ccprints("FP_SPI_SEL: %s", fp_sensor_spi_select_to_str(spi_select));

	spi_configure(spi_select);

	ccprints("TRANSPORT_SEL: %s",
		 fp_transport_type_to_str(get_fp_transport_type()));
}
