/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f767zi development board configuration */

#include "common.h"
/* #include "dma.h" */
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
/* #include "stm32-dma.h" */
/* #include "spi.h" */
#include "task.h"
#include "timer.h"
#include "util.h"

static int debounced_gpio_state;


static void turn_off_led(void)
{
	ccprintf("Turn off LED\n");
	gpio_set_level(GPIO_LED_BLUE, 0);
}

/* A function must be explicitly declared as being deferrable. */
DECLARE_DEFERRED(turn_off_led);

void user_button_evt(void)
{
	ccprintf("Button %d, %d!\n",
		GPIO_USER_BUTTON_L, gpio_get_level(GPIO_USER_BUTTON_L));
	gpio_set_level(GPIO_LED_BLUE, !gpio_get_level(GPIO_LED_BLUE));
	hook_call_deferred(&turn_off_led_data, 1000 * MSEC);
}


static void some_interrupt_deferred(void)
{
	int gpio_state = gpio_get_level(GPIO_USER_BUTTON_L);

	if (gpio_state == debounced_gpio_state)
		return;
	debounced_gpio_state = gpio_state;
	user_button_evt(); /* Or some other useful action. */
}

/* A function must be explicitly declared as being deferrable. */
DECLARE_DEFERRED(some_interrupt_deferred);

void user_button_evt_debounce(enum gpio_signal signal)
{
	/* note: readme.md doesn't mention the & and _data...
	 * hook_call_deferred(some_interrupt_deferred, 30 * MSEC);
	 */
	hook_call_deferred(&some_interrupt_deferred_data, 30 * MSEC);
}




#ifndef HAS_TASK_FPSENSOR
void fps_event(enum gpio_signal signal)
{
}
#endif

#include "gpio_list.h"

/* elee: comment out spi for now */
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
	/* spi_configure(); elee, skip spi for now */
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
