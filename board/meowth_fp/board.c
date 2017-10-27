/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Meowth Fingerprint MCU configuration */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "util.h"

void user_button_evt(enum gpio_signal signal)
{
	ccprintf("Button %d, %d!\n", signal, gpio_get_level(signal));
	gpio_set_level(GPIO_LED_RED, !gpio_get_level(GPIO_LED_RED));
}

#include "gpio_list.h"

#if 0
/* SPI devices */
const struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor */
	{ CONFIG_SPI_FP_PORT, 5, GPIO_SPI3_NSS }
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

static void spi_configure(void)
{
	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_MASTER, 1);
	/* Set all SPI master signal pins to very high speed: pins B3/B4/B5 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x00000fc0;
	/* Enable clocks to SPI3 module (master) */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI3;

	spi_enable(CONFIG_SPI_FP_PORT, 1);
}
#endif

/* Initialize board. */
static void board_init(void)
{
#if 0
	spi_configure();
#endif
	gpio_enable_interrupt(GPIO_WAKEUP_BUTTON);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
