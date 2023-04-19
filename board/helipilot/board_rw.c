/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fpsensor_detect.h"
#include "gpio.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
/*#include "usart_host_command.h"*/
#include "util.h"

#ifndef SECTION_IS_RW
#error "This file should only be built for RW."
#endif

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor (SCLK at 4Mhz) */
	{
		.port = CONFIG_SPI_FP_PORT,
		.div = 3,
		.gpio_cs = GPIO_SPI_MCU_CS_FP_L
	}
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

static void configure_fp_sensor_spi(void)
{
	/* The dragonclaw development board needs this enabled to enable the
	 * AND gate (U10) to CS. Production boards could disable this to save
	 * power since it's only needed for initial detection on those boards.
	 */
	gpio_set_level(GPIO_DIVIDER_HIGHSIDE, 1);

	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_CONTROLLER, 1);

	spi_enable(&spi_devices[0], 1);
}

void board_init_rw(void)
{
	/* Configure and enable SPI as master for FP sensor */
	configure_fp_sensor_spi();
}
