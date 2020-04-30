/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Meowth Fingerprint MCU configuration */

#include "common.h"
#include "console.h"
#include "fpsensor_detect.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "usart_host_command.h"
#include "util.h"

/**
 * Disable restricted commands when the system is locked.
 *
 * @see console.h system.c
 */
int console_is_restricted(void)
{
	return system_is_locked();
}

#ifndef HAS_TASK_FPSENSOR
void fps_event(enum gpio_signal signal)
{
}
#endif

static void ap_deferred(void)
{
	/*
	 * in S3:   SLP_S3_L is 0 and SLP_S0_L is X.
	 * in S0ix: SLP_S3_L is X and SLP_S0_L is 0.
	 * in S0:   SLP_S3_L is 1 and SLP_S0_L is 1.
	 * in S5/G3, the FP MCU should not be running.
	 */
	int running = gpio_get_level(GPIO_PCH_SLP_S3_L)
			&& gpio_get_level(GPIO_PCH_SLP_S0_L);

	if (running) { /* S0 */
		disable_sleep(SLEEP_MASK_AP_RUN);
		hook_notify(HOOK_CHIPSET_RESUME);
	} else { /* S0ix/S3 */
		hook_notify(HOOK_CHIPSET_SUSPEND);
		enable_sleep(SLEEP_MASK_AP_RUN);
	}
}
DECLARE_DEFERRED(ap_deferred);

/* PCH power state changes */
void slp_event(enum gpio_signal signal)
{
	hook_call_deferred(&ap_deferred_data, 0);
}

#include "gpio_list.h"

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor (SCLK at 4Mhz) */
	{ .port = CONFIG_SPI_FP_PORT, .div = 3, .gpio_cs = GPIO_SPI2_NSS }
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

#ifdef SECTION_IS_RW
static void configure_fp_sensor_spi(void)
{
	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_MASTER, 1);

	/* Set all SPI master signal pins to very high speed: B12/13/14/15 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000;

	/* Enable clocks to SPI2 module (master) */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;

	spi_enable(CONFIG_SPI_FP_PORT, 1);
}
#endif

/* Initialize board. */
static void board_init(void)
{
#ifdef SECTION_IS_RW
	enum fp_sensor_type retSensor = get_fp_sensor_type();
	enum fp_transport_type retTransport = get_fp_transport_type();

	ccprints("FP_SENSOR_SEL: %s",
		fp_sensor_type_to_str(retSensor));

	/* Configure and Enable FP SPI */
	configure_fp_sensor_spi();

	ccprints("TRANSPORT_SEL: %s",
		fp_transport_type_to_str(retTransport));

	/* Initialize Transport based on bootstrap */
	if (retTransport == FP_TRANSPORT_TYPE_UART) {

#if defined(CONFIG_USART_HOST_COMMAND)
		usart_host_command_init();
#endif /* CONFIG_USART_HOST_COMMAND */

		/* Disable SPI interrupt to disable SPI transport layer */
		gpio_disable_interrupt(GPIO_SPI1_NSS);

	} else if (retTransport == FP_TRANSPORT_TYPE_SPI) {
		/* SPI transport is enabled. SPI1_NSS interrupt will process
		 * incoming request/
		 */
	} else {
		ccprints("ERROR: Selected transport is not valid.");
	}
#endif

	/* Enable interrupt on PCH power signals */
	gpio_enable_interrupt(GPIO_PCH_SLP_S3_L);
	gpio_enable_interrupt(GPIO_PCH_SLP_S0_L);

	/* enable the SPI slave interface if the PCH is up */
	hook_call_deferred(&ap_deferred_data, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
