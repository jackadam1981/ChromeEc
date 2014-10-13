/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Hoho dongle configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "usb.h"
#include "usb_bb.h"
#include "usb_pd.h"
#include "timer.h"
#include "util.h"

#define HPD_ARR_SIZE 2

static struct hpd_data {
	timestamp_t time;
	int level;
} hpds[HPD_ARR_SIZE];
static int hpd_idx = 1;

void hpd_deferred(void)
{
	enum hpd_event hpd = hpd_none;
	int level = gpio_get_level(GPIO_DP_HPD);
	int prev_level = hpds[hpd_idx].level;
	timestamp_t now = get_time();
	int delta_us = now.val - hpds[hpd_idx].time.val;
	cprints(CC_HOOK, "HPD DEFER! prev:%d now:%d dT(us):%d",
		prev_level, level, delta_us);

	/* debounce */
	if (level == prev_level)
		return;

	if (!prev_level && delta_us >= HPD_DEBOUNCE_LVL)
		hpd = hpd_high;
	else if (prev_level && (delta_us > HPD_DEBOUNCE_IRQ) &&
		 (delta_us < HPD_DEBOUNCE_LVL))
		hpd = hpd_irq;
	else if (prev_level && (delta_us >= HPD_DEBOUNCE_LVL))
		hpd = hpd_low;

	if (hpd != hpd_none)
		pd_send_hpd(0, hpd);
}
DECLARE_DEFERRED(hpd_deferred);

void hpd_event(enum gpio_signal signal)
{
	hpds[hpd_idx].level = gpio_get_level(signal);
	hpds[hpd_idx].time = get_time();
	if (!hpds[hpd_idx].level)
		hook_call_deferred(hpd_deferred, HPD_DEBOUNCE_IRQ);
	else
		hook_call_deferred(hpd_deferred, HPD_DEBOUNCE_LVL);
	hpd_idx = !hpd_idx;
}

#include "gpio_list.h"

/* Initialize board. */
void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
	/* Remap USART DMA to match the USART driver */
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10);/* Remap USART1 RX/TX DMA */
}

#ifdef CONFIG_SPI_FLASH

static void board_init_spi2(void)
{
	/* Remap SPI2 to DMA channels 6 and 7 */
	STM32_SYSCFG_CFGR1 |= (1 << 24);

	/* Set pin NSS to general purpose output mode (01b). */
	/* Set pins SCK, MISO, and MOSI to alternate function (10b). */
	STM32_GPIO_MODER(GPIO_B) &= ~0xff000000;
	STM32_GPIO_MODER(GPIO_B) |= 0xa9000000;

	/* Set all four pins to alternate function 0 */
	STM32_GPIO_AFRH(GPIO_B) &= ~(0xffff0000);

	/* Set all four pins to output push-pull */
	STM32_GPIO_OTYPER(GPIO_B) &= ~(0xf000);

	/* Set pullup on NSS */
	STM32_GPIO_PUPDR(GPIO_B) |= 0x1000000;

	/* Set all four pins to high speed */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000;

	/* Reset SPI2 */
	STM32_RCC_APB1RSTR |= (1 << 14);
	STM32_RCC_APB1RSTR &= ~(1 << 14);

	/* Enable clocks to SPI2 module */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;
}
#endif /* CONFIG_SPI_FLASH */

/* Initialize board. */
static void board_init(void)
{
#ifdef CONFIG_SPI_FLASH
	board_init_spi2();
#endif
	gpio_enable_interrupt(GPIO_DP_HPD);
}

DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"USB_C_CC1_PD", 3300, 4096, 0, STM32_AIN(1)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 400, GPIO_MCDP_I2C_SCL, GPIO_MCDP_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

const void * const usb_strings[] = {
	[USB_STR_DESC] = usb_string_desc,
	[USB_STR_VENDOR] = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT] = USB_STRING_DESC("Hoho"),
	[USB_STR_VERSION] = NULL /* filled at runtime */,
	[USB_STR_BB_URL] = USB_STRING_DESC(USB_GOOGLE_TYPEC_URL),
};
BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/**
 * USB configuration
 * Any type-C device with alternate mode capabilities must have the following
 * set of descriptors.
 *
 * 1. Standard Device
 * 2. BOS
 *    2a. Container ID
 *    2b. Billboard Caps
 */
struct my_bos {
	struct usb_bos_hdr_descriptor bos;
	struct usb_contid_caps_descriptor contid_caps;
	struct usb_bb_caps_base_descriptor bb_caps;
	struct usb_bb_caps_svid_descriptor bb_caps_svids[1];
};

static struct my_bos bos_desc = {
	.bos = {
		.bLength = USB_DT_BOS_SIZE,
		.bDescriptorType = USB_DT_BOS,
		.wTotalLength = (USB_DT_BOS_SIZE + USB_DT_CONTID_SIZE +
				 USB_BB_CAPS_BASE_SIZE +
				 USB_BB_CAPS_SVID_SIZE * 1),
		.bNumDeviceCaps = 2,  /* contid + bb_caps */
	},
	.contid_caps =	{
		.bLength = USB_DT_CONTID_SIZE,
		.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
		.bDevCapabilityType = USB_DC_DTYPE_CONTID,
		.bReserved = 0,
		.ContainerID = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	},
	.bb_caps = {
		.bLength = (USB_BB_CAPS_BASE_SIZE + USB_BB_CAPS_SVID_SIZE * 1),
		.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
		.bDevCapabilityType = USB_DC_DTYPE_BILLBOARD,
		.iAdditionalInfoURL = USB_STR_BB_URL,
		.bNumberOfAlternateModes = 1,
		.bPreferredAlternateMode = 1,
		.VconnPower = 0,
		.bmConfigured = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.bReserved = 0,
	},
	.bb_caps_svids = {
		{
			.wSVID = 0xff01, /* TODO(tbroch) def'd in other CL remove hardcode */
			.bAlternateMode = 1,
			.iAlternateModeString = USB_STR_BB_URL, /* TODO(crosbug.com/p/32687) */
		},
	},
};

const struct bos_context bos_ctx = {
	.descp = (void *)&bos_desc,
	.size = sizeof(struct my_bos),
};
