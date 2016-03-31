/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* IT8320 development board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "usb_pd.h"
#include "usb_pd_phy_chip.h"
#include "util.h"

/* Reset PD MCU */
void board_reset_pd_mcu(void)
{

}

int board_get_battery_soc(void)
{
	return 100;
}

void board_pd_vconn_ctrl(int port, int cc_pin, int enabled)
{
	if (enabled) {
		if (USBPD_CC_PIN_1 == cc_pin) {
			if (port == USBPD_PORT_A) {
				gpio_set_level(GPIO_USBPD_PORTA_CC2_VCONN, 0);
				gpio_set_level(GPIO_USBPD_PORTA_CC1_VCONN, 1);
			} else {
				gpio_set_level(GPIO_USBPD_PORTB_CC2_VCONN, 0);
				gpio_set_level(GPIO_USBPD_PORTB_CC1_VCONN, 1);
			}
		} else {
			if (port == USBPD_PORT_A) {
				gpio_set_level(GPIO_USBPD_PORTA_CC1_VCONN, 0);
				gpio_set_level(GPIO_USBPD_PORTA_CC2_VCONN, 1);
			} else {
				gpio_set_level(GPIO_USBPD_PORTB_CC1_VCONN, 0);
				gpio_set_level(GPIO_USBPD_PORTB_CC2_VCONN, 1);
			}
		}
	} else {
		if (port == USBPD_PORT_A) {
			gpio_set_level(GPIO_USBPD_PORTA_CC1_VCONN, 0);
			gpio_set_level(GPIO_USBPD_PORTA_CC2_VCONN, 0);
		} else {
			gpio_set_level(GPIO_USBPD_PORTB_CC1_VCONN, 0);
			gpio_set_level(GPIO_USBPD_PORTB_CC2_VCONN, 0);
		}
	}
}

void board_pd_vbus_ctrl(int port, int enabled)
{
	if (enabled) {
		if (port == USBPD_PORT_A) {
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_INPUT, 0);
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_OUTPUT, 1);
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 0);
		} else {
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_INPUT, 0);
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_OUTPUT, 1);
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 0);
		}
	} else{
		if (port == USBPD_PORT_A) {
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_OUTPUT, 0);
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_INPUT, 1);
			/* drop vlot */
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 1);
			udelay(MSEC);
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 0);
		} else {
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_OUTPUT, 0);
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_INPUT, 1);
			/* drop vlot */
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 1);
			udelay(MSEC);
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 0);
		}
	}
}

void board_pd_init(int port, int role)
{
	/* disable all */
	board_pd_vconn_ctrl(port, 0, 0);
	board_pd_vbus_ctrl(port, 0);
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	/*
	 * Default no low power idle for EVB,
	 * use console command "sleepmask" to enable it if necessary.
	 */
	disable_sleep(SLEEP_MASK_FORCE_NO_DSLEEP);
	/*
	 * The GPIOH.5/6 may be used for flashing purposes if WP pin
	 * is deasserted. The clock of this module needs to be enabled.
	 * So we disable the clock when WP pin is asserted,
	 * this can help to reduce power consumption.
	 */
#ifdef CONFIG_WP_ACTIVE_HIGH
	if (gpio_get_level(GPIO_WP))
		clock_disable_peripheral(CGC_OFFSET_USB, 0, 0);
	else
		clock_enable_peripheral(CGC_OFFSET_USB, 0, 0);
#else
	if (!gpio_get_level(GPIO_WP_L))
		clock_disable_peripheral(CGC_OFFSET_USB, 0, 0);
	else
		clock_enable_peripheral(CGC_OFFSET_USB, 0, 0);
#endif
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Convert to mV (3000mV/1024). */
	{"ADC_VBUSSA", 3000, 1024, 0, 0}, /*GPI0*/
	{"ADC_VBUSSB", 3000, 1024, 0, 1}, /*GPI1*/
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
