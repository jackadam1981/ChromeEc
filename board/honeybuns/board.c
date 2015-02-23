/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Honeybuns board configuration */

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
#include "usb.h"
#include "usb_pd.h"
#include "util.h"



void vbus_event(enum gpio_signal signal)
{
	ccprintf("VBUS!\n");
}


#include "gpio_list.h"


static void honeybuns_test_led_update(void)
{
	static int toggle_count;
	toggle_count++;

	gpio_set_level(GPIO_TP6, toggle_count&1);
	hook_call_deferred(honeybuns_test_led_update, 500*MSEC);
}
DECLARE_DEFERRED(honeybuns_test_led_update);

/* Initialize board. */
static void board_init(void)
{
	/* kick off test led flashing */
	hook_call_deferred(honeybuns_test_led_update, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Initialize board. */
void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/*
	 * the DMA mapping is :
	 *  Chan 2 : TIM1_CH1  (C0 RX)
	 *  Chan 3 :
	 *  Chan 4 : USART1_TX
	 *  Chan 5 : USART1_RX
	 *  Chan 6 :
	 *  Chan 7 : SPI2_TX   (C0 TX)
	 */
	/* Remap USART DMA to match the USART driver */
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10);
	/* Remap SPI2 to DMA channels 6 and 7 */
	STM32_SYSCFG_CFGR1 |= (1 << 24);
}


/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"CC1_PD", 3300, 4096, 0, STM32_AIN(0)},
	[ADC_CH_VIN_DIV_P] = {"VIN_DIV_P", 3300, 4096, 0, STM32_AIN(5)},
	[ADC_CH_VIN_DIV_N] = {"VIN_DIV_N", 3300, 4096, 0, STM32_AIN(6)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 100,
		GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Honeybuns"),
	[USB_STR_VERSION] = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_BB_URL] = USB_STRING_DESC(USB_GOOGLE_TYPEC_URL),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);




void board_set_usb_mux(int port, enum typec_mux mux, int polarity)
{
#if 0
	/* reset everything */
	gpio_set_level(GPIO_SS1_EN_L, 1);
	gpio_set_level(GPIO_SS2_EN_L, 1);
	gpio_set_level(GPIO_DP_MODE, 0);
	gpio_set_level(GPIO_SS1_USB_MODE_L, 1);
	gpio_set_level(GPIO_SS2_USB_MODE_L, 1);

	if (mux == TYPEC_MUX_NONE)
		/* everything is already disabled, we can return */
		return;

	if (mux == TYPEC_MUX_USB || mux == TYPEC_MUX_DOCK) {
		/* USB 3.0 uses 2 superspeed lanes */
		gpio_set_level(polarity ? GPIO_SS2_USB_MODE_L :
					  GPIO_SS1_USB_MODE_L, 0);
	}

	if (mux == TYPEC_MUX_DP || mux == TYPEC_MUX_DOCK) {
		/* DP uses available superspeed lanes (x2 or x4) */
		gpio_set_level(GPIO_DP_POLARITY_L, !polarity);
		gpio_set_level(GPIO_DP_MODE, 1);
	}
	/* switch on superspeed lanes */
	gpio_set_level(GPIO_SS1_EN_L, 0);
	gpio_set_level(GPIO_SS2_EN_L, 0);
#endif
}

int board_get_usb_mux(int port, const char **dp_str, const char **usb_str)
{
#if 0
	int has_ss = !gpio_get_level(GPIO_SS1_EN_L);
	int has_usb = !gpio_get_level(GPIO_SS1_USB_MODE_L) ||
		      !gpio_get_level(GPIO_SS2_USB_MODE_L);
	int has_dp = !!gpio_get_level(GPIO_DP_MODE);

	if (has_dp)
		*dp_str = gpio_get_level(GPIO_DP_POLARITY_L) ? "DP1" : "DP2";
	else
		*dp_str = NULL;

	if (has_usb)
		*usb_str = gpio_get_level(GPIO_SS1_USB_MODE_L) ?
				"USB2" : "USB1";
	else
		*usb_str = NULL;

	return has_ss;
#else
	return 0;
#endif
}
