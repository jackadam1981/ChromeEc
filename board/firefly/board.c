/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Firefly board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "usb_descriptor.h"
#include "usb_pd.h"
#include "util.h"

/* Debounce time for voltage buttons */
#define BUTTON_DEBOUNCE_US (100 * MSEC)

static enum gpio_signal button_pressed;

static uint32_t *recv_src_cap;
static int8_t recv_src_cap_count;
int8_t current_cap = 0;

void board_process_source_cap(int port, int cnt, uint32_t *src_cap)
{
	if (port)
		return;

	recv_src_cap_count = cnt;
	recv_src_cap = src_cap;
}

static int get_next_cap_voltage(void)
{
	current_cap++;

	if (current_cap >= recv_src_cap_count || !recv_src_cap_count) {
		current_cap = 0;
		return 5000;
	}

	return ((recv_src_cap[current_cap] >> 10) & 0x3ff) * 50;
}

/* Handle debounced button press */
static void button_deferred(void)
{
	int mv;

	/* bounce ? */
	if (gpio_get_level(button_pressed) != 0)
		return;

	switch (button_pressed) {
	case GPIO_SW_PP20000:
		mv = get_next_cap_voltage();
		break;
	case GPIO_SW_PP12000:
		return;
	case GPIO_SW_PP5000:
		mv = 5000;
		break;
	default:
		mv = -1;
	}
	pd_request_source_voltage(0, mv);
	ccprintf("Button %d = %d => Vout=%d mV\n",
		 button_pressed, gpio_get_level(button_pressed), mv);
}
DECLARE_DEFERRED(button_deferred);

void button_event(enum gpio_signal signal)
{
	button_pressed = signal;
	/* reset debounce time */
	hook_call_deferred(&button_deferred_data, BUTTON_DEBOUNCE_US);
}

void vbus_event(enum gpio_signal signal)
{
	ccprintf("VBUS! =%d\n", gpio_get_level(signal));
	task_wake(TASK_ID_PD_C0);
}

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
	/* Remap USART DMA to match the USART driver */
	/*
	 * the DMA mapping is :
	 *  Chan 2 : TIM1_CH1  (PD RX)
	 *  Chan 3 : SPI1_TX   (PD TX)
	 *  Chan 4 : USART1_TX
	 *  Chan 5 : USART1_RX
	 */
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10);/* Remap USART1 RX/TX DMA */
}

#include "gpio_list.h"

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"CC1_PD", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_CH_CC2_PD] = {"CC2_PD", 3300, 4096, 0, STM32_AIN(2)},
	/* VBUS voltage sensing is behind a 10K/100K voltage divider */
	[ADC_CH_VBUS_SENSE] = {"VBUS", 36300, 4096, 0, STM32_AIN(5)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void board_init(void)
{
	gpio_enable_interrupt(GPIO_SW_PP20000);
	gpio_enable_interrupt(GPIO_SW_PP12000);
	gpio_enable_interrupt(GPIO_SW_PP5000);

	/* Enable interrupts on VBUS transitions. */
	gpio_enable_interrupt(GPIO_VBUS_WAKE);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* USB */
const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Firefly"),
	[USB_STR_VERSION] = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Shell"),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

static int command_volt(int argc, char **argv)
{
	int millivolt = -1;
	if (argc >= 2) {
		char *e;
		millivolt = strtoi(argv[1], &e, 10) * 1000;
	}
	ccprintf("Request Vout=%d mV\n", millivolt);
	pd_request_source_voltage(0, millivolt);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(volt, command_volt,
			"[5|12|20]",
			"set voltage through USB PD",
			NULL);
