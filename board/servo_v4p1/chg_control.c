/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "chg_control.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "ioexpanders.h"
#include "pathsel.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "update_fw.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_gpio.h"
#include "usb_i2c.h"
#include "usb_pd.h"
#include "usb_spi.h"
#include "usb-stream.h"
#include "util.h"

#define CHG_P5V_POWER	0
#define CHG_VBUS_POWER	1

void chg_reset(void)
{
	/* Disconnect DUT Power */
	chg_power_select(CHG_POWER_OFF);

	/* Disconnect CHG CC1(Rd) and CC2(Rd) */
	chg_vbus_disable(CHG_CC1, 1);
	chg_vbus_disable(CHG_CC2, 1);

	msleep(100);

	/* Connect CHG CC1(Rd) and CC2(Rd) to detect charger */
	chg_vbus_disable(CHG_CC1, 0);
	chg_vbus_disable(CHG_CC2, 0);
}

void chg_power_select(enum chg_power_select_t type)
{
	switch (type) {
	case CHG_POWER_OFF:
		dut_chg_en(0);
		vbus_dischrg_en(1);
		break;
	case CHG_POWER_PP5000:
		vbus_dischrg_en(0);
		host_or_chg_ctl(CHG_P5V_POWER);
		dut_chg_en(1);
		break;
	case CHG_POWER_VBUS:
		vbus_dischrg_en(0);
		host_or_chg_ctl(CHG_VBUS_POWER);
		dut_chg_en(1);
		break;
	}
}

void chg_vbus_disable(enum chg_cc_t cc, bool di)
{
	if (di) {
		if (cc == CHG_CC1) {
			/*
			 * Configure USB_CHG_CC1_MCU to GPIO and
			 * drive high to trigger disconnect.
			 */
			/* Set level high */
			gpio_set_level(GPIO_USB_CHG_CC1_MCU, 1);

			/* Disable Analog mode and Enable GPO */
			STM32_GPIO_MODER(GPIO_A) = (STM32_GPIO_MODER(GPIO_A)
				& ~(3 << (2*2))) /* PA2 disable ADC */
				|  (1 << (2*2)); /* Set as GPO */
		} else {
			/*
			 * Configure USB_CHG_CC2_MCU to GPIO and
			 * drive high to trigger disconnect.
			 */
			/* Set level high */
			gpio_set_level(GPIO_USB_CHG_CC2_MCU, 1);

			/* Disable Analog mode and Enable GPO */
			STM32_GPIO_MODER(GPIO_A) = (STM32_GPIO_MODER(GPIO_A)
				& ~(3 << (2*4))) /* PA4 disable ADC */
				|  (1 << (2*4)); /* Set as GPO */
		}
	} else {
		if (cc == CHG_CC1) {
			/* Configure USB_CHG_CC1_MCU as ANALOG input */
			/* Set PA4 pin to Analog mode */
			STM32_GPIO_MODER(GPIO_A) = (STM32_GPIO_MODER(GPIO_A)
				|  (3 << (2*2))); /* PA2 in ANALOG mode */
		} else {
			/* Configure USB_CHG_CC2_MCU as ANALOG input */
			/* Set PA4 pin to Analog mode */
			STM32_GPIO_MODER(GPIO_A) = (STM32_GPIO_MODER(GPIO_A)
				|  (3 << (2*4))); /* PA4 in ANALOG mode */
		}
	}
}
