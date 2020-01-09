/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f767zi development board configuration */

#include "common.h"
#include "dma.h"
#include "ec_version.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
/* #include "i2c.h" */
#include "registers.h"
#include "stm32-dma.h"
/* #include "spi.h" */
#include "task.h"
#include "timer.h"

/* #include "update_fw.h" */
#include "usb_descriptor.h"
#include "usb_dwc_console.h"
/* #include "usb_dwc_i2c.h" */
#include "usb_dwc_stream.h"
/* #include "usb_dwc_update.h" */
#include "usb_hw.h"
/* #include "usb_power.h" */
#include "util.h"


/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]		= usb_string_desc,
	[USB_STR_VENDOR]	= USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]	= USB_STRING_DESC("nucleo-f767"),
	[USB_STR_SERIALNO]	= USB_STRING_DESC("1234-a"),
	[USB_STR_VERSION]	= USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_I2C_NAME]	= USB_STRING_DESC("I2C"),
	[USB_STR_CONSOLE_NAME]	= USB_STRING_DESC("Sweetberry EC Shell"),
	[USB_STR_UPDATE_NAME]	= USB_STRING_DESC("Firmware update"),
};


/* USB power interface. */
/* USB_POWER_CONFIG(sweetberry_power, USB_IFACE_POWER, USB_EP_POWER); */

struct dwc_usb usb_ctl = {
	.ep = {
		&ep0_ctl,
		&ep_console_ctl,
		/* &usb_update_ep_ctl,
		 * &sweetberry_power_ep_ctl,
		 * &i2c_usb__ep_ctl,
		 */
	},
	.speed = USB_SPEED_FS,
	.phy_type = USB_PHY_INTERNAL, /*USB_PHY_ULPI, */

	/*1, elee: OK, this DOES seem to need
	 * to be 1!  If 0 then it hangs when plugging in USB.
	 */
	.dma_en = 1,
	.irq = STM32_IRQ_OTG_FS, /*STM32_IRQ_OTG_HS, */
};


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

/* elee: Note: this line should not be at the top.  Or it will include
 * gpio.inc and reference interrupt functions before they are defined!
 */
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

#if 0
/* elee: looks like this isn't needed until we have an external ULPI chip */
void board_config_post_gpio_init(void)
{
	/* We use MCO2 clock passthrough to provide a clock to USB HS */
	gpio_config_module(MODULE_MCO, 1);
	/* GPIO PC9 to high speed */
	GPIO_SET_HS(C,  9);

	/*elee: shouldn't need this, DON'T have external phy (yet),
	 * so no gpio to enable it!
	 */
	if (usb_ctl.phy_type == USB_PHY_ULPI)
		gpio_set_level(GPIO_USB_MUX_SEL, 0);
	else
		gpio_set_level(GPIO_USB_MUX_SEL, 1);
}
#endif

/* Initialize board. */
static void board_init(void)
{
	/* spi_configure(); elee, skip spi for now */
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
