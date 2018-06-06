/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_pd_config.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)


/* TX uses SPI1 on PB3-4 for CHG port, SPI2 on PB 13-14 for DUT port */
void spi_enable_clock(int port)
{
	if (port == 0)
		STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;
	else
		STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;
}

/* the pins used for communication need to be hi-speed */
void pd_set_pins_speed(int port)
{
	if (port == 0) {
		/* 40 MHz pin speed on SPI PB3&4,
		 * (USB_CHG_TX_CLKIN & USB_CHG_CC1_TX_DATA)
		 */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0x000003C0;
		/* 40 MHz pin speed on TIM16_CH1 (PB8),
		 * (USB_CHG_TX_CLKOUT)
		 */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0x00030000;
	} else {
		/* 40 MHz pin speed on SPI PB13/14,
		 * (USB_DUT_TX_CLKIN & USB_DUT_CC1_TX_DATA)
		 */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0x3C000000;
		/* 40 MHz pin speed on TIM15_CH2 (PB15) */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0xC0000000;
	}
}

/* Reset SPI peripheral used for TX */
void pd_tx_spi_reset(int port)
{
	if (port == 0) {
		/* Reset SPI1 */
		STM32_RCC_APB2RSTR |= (1 << 12);
		STM32_RCC_APB2RSTR &= ~(1 << 12);
	} else {
		/* Reset SPI2 */
		STM32_RCC_APB1RSTR |= (1 << 14);
		STM32_RCC_APB1RSTR &= ~(1 << 14);
	}
}

/* Drive the CC line from the TX block */
void pd_tx_enable(int port, int polarity)
{
	if (port == 0) {
		/* put SPI function on TX pin */
		if (polarity) {
			const struct gpio_info *g = gpio_list +
				GPIO_USB_CHG_CC2_TX_DATA;
			gpio_set_alternate_function(g->port, g->mask, 0);

			/* set the low level reference */
			gpio_set_flags(GPIO_USB_CHG_CC2_PD, GPIO_OUT_LOW);
		} else {
			const struct gpio_info *g = gpio_list +
				GPIO_USB_CHG_CC1_TX_DATA;
			gpio_set_alternate_function(g->port, g->mask, 0);

			/* set the low level reference */
			gpio_set_flags(GPIO_USB_CHG_CC1_PD, GPIO_OUT_LOW);
		}
	} else {
		/* put SPI function on TX pin */
		/* MCU ADC pin output low */
		if (polarity) {
			/* USB_DUT_CC2_TX_DATA: PC2 is SPI2 MISO */
			const struct gpio_info *g = gpio_list +
				GPIO_USB_DUT_CC2_TX_DATA;
			gpio_set_alternate_function(g->port, g->mask, 1);

			/* set the low level reference */
			gpio_set_flags(GPIO_USB_DUT_CC2_PD, GPIO_OUT_LOW);
		} else {
			/* USB_DUT_CC1_TX_DATA: PB14 is SPI2 MISO */
			const struct gpio_info *g = gpio_list +
				GPIO_USB_DUT_CC1_TX_DATA;
			gpio_set_alternate_function(g->port, g->mask, 0);

			/* set the low level reference */
			gpio_set_flags(GPIO_USB_DUT_CC1_PD, GPIO_OUT_LOW);
		}
	}
}

/* Put the TX driver in Hi-Z state */
void pd_tx_disable(int port, int polarity)
{
	if (port == 0) {
		if (polarity) {
			gpio_set_flags(GPIO_USB_CHG_CC2_TX_DATA, GPIO_INPUT);
			gpio_set_flags(GPIO_USB_CHG_CC2_PD, GPIO_ANALOG);
		} else {
			gpio_set_flags(GPIO_USB_CHG_CC1_TX_DATA, GPIO_INPUT);
			gpio_set_flags(GPIO_USB_CHG_CC1_PD, GPIO_ANALOG);
		}
	} else {
		if (polarity) {
			gpio_set_flags(GPIO_USB_DUT_CC2_TX_DATA, GPIO_INPUT);
			gpio_set_flags(GPIO_USB_DUT_CC2_PD, GPIO_ANALOG);
		} else {
			gpio_set_flags(GPIO_USB_DUT_CC1_TX_DATA, GPIO_INPUT);
			gpio_set_flags(GPIO_USB_DUT_CC1_PD, GPIO_ANALOG);
		}
	}
}

/* we know the plug polarity, do the right configuration */
void pd_select_polarity(int port, int polarity)
{
	uint32_t val = STM32_COMP_CSR;

	/* Use window mode so that COMP1 and COMP2 share non-inverting input */
	val |= STM32_COMP_CMP1EN | STM32_COMP_CMP2EN | STM32_COMP_WNDWEN;

	if (port == 0) {
		/* CHG use the right comparator inverted input for COMP2 */
		STM32_COMP_CSR = (val & ~STM32_COMP_CMP2INSEL_MASK) |
			(polarity ? STM32_COMP_CMP2INSEL_INM4  /* PA4: C0_CC2 */
				  : STM32_COMP_CMP2INSEL_INM6);/* PA2: C0_CC1 */
	} else {
		/* DUT use the right comparator inverted input for COMP1 */
		STM32_COMP_CSR = (val & ~STM32_COMP_CMP1INSEL_MASK) |
			(polarity ? STM32_COMP_CMP1INSEL_INM5  /* PA5: C1_CC2 */
			 : STM32_COMP_CMP1INSEL_INM6);/* PA0: C1_CC1 */
	}
}

/* Initialize pins used for TX and put them in Hi-Z */
void pd_tx_init(void)
{
	gpio_config_module(MODULE_USB_PD, 1);
}

void pd_set_host_mode(int port, int enable)
{
	/*
	 * CHG (port == 0) port has fixed Rd attached and therefore can only
	 * present as a SNK device. If port != DUT (port == 1), then nothing to
	 * do in this function.
	 */
	if (!port)
		return;

	if (enable) {
		/*
		 * Servo_v4 in SRC mode acts as a DTS (debug test
		 * accessory) and needs to present Rp on both CC
		 * lines. In order to support orientation detection, and
		 * advertise the correct TypeC current level, the
		 * values of Rp1/Rp2 need to asymmetric with Rp1 > Rp2. This
		 * function is called without a specified Rp value so assume the
		 * servo_v4 default of USB level current. If a higher current
		 * can be supported, then the Rp value will get adjusted when
		 * VBUS is enabled.
		 */
		pd_set_rp_rd(port, TYPEC_CC_RP, TYPEC_RP_USB);

		gpio_set_flags(GPIO_USB_DUT_CC1_TX_DATA, GPIO_INPUT);
		gpio_set_flags(GPIO_USB_DUT_CC2_TX_DATA, GPIO_INPUT);
	} else {
		/* Select Rd, the Rp value is a don't care */
		pd_set_rp_rd(port, TYPEC_CC_RD, TYPEC_RP_RESERVED);
	}
}

/**
 * Initialize various GPIOs and interfaces to safe state at start of pd_task.
 *
 * These include:
 *   VBUS, charge path based on power role.
 *   Physical layer CC transmit.
 *
 * @param port        USB-C port number
 * @param power_role  Power role of device
 */
void pd_config_init(int port, uint8_t power_role)
{
	/*
	 * Set CC pull resistors, and charge_en and vbus_en GPIOs to match
	 * the initial role.
	 */
	pd_set_host_mode(port, power_role);

	/* Initialize TX pins and put them in Hi-Z */
	pd_tx_init();

}

int pd_adc_read(int port, int cc)
{
	int mv;

	if (port == 0)
		mv = adc_read_channel(cc ? ADC_CHG_CC2_PD : ADC_CHG_CC1_PD);
	else
		mv = adc_read_channel(cc ? ADC_DUT_CC2_PD : ADC_DUT_CC1_PD);

	return mv;
}
