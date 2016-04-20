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
#include "driver/tcpm/it83xx_pd.h"
#include "hooks.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"

int board_get_battery_soc(void)
{
	return 100;
}

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
	{-1, -1, &it83xx_tcpm_drv},
	{-1, -1, &it83xx_tcpm_drv},
};

void board_pd_vconn_ctrl(int port, int cc_pin, int enabled)
{
	int cc1_enabled = 0, cc2_enabled = 0;

	if (cc_pin)
		cc2_enabled = enabled;
	else
		cc1_enabled = enabled;

	if (port) {
		gpio_set_level(GPIO_USBPD_PORTB_CC2_VCONN, cc2_enabled);
		gpio_set_level(GPIO_USBPD_PORTB_CC1_VCONN, cc1_enabled);
	} else {
		gpio_set_level(GPIO_USBPD_PORTA_CC2_VCONN, cc2_enabled);
		gpio_set_level(GPIO_USBPD_PORTA_CC1_VCONN, cc1_enabled);
	}
}

void board_pd_vbus_ctrl(int port, int enabled)
{
	if (port) {
		gpio_set_level(GPIO_USBPD_PORTB_VBUS_INPUT, !enabled);
		gpio_set_level(GPIO_USBPD_PORTB_VBUS_OUTPUT, enabled);
		if (!enabled) {
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 1);
			udelay(MSEC);
		}
		gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 0);
	} else {
		gpio_set_level(GPIO_USBPD_PORTA_VBUS_INPUT, !enabled);
		gpio_set_level(GPIO_USBPD_PORTA_VBUS_OUTPUT, enabled);
		if (!enabled) {
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 1);
			udelay(MSEC);
		}
		gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 0);
	}
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Convert to mV (3000mV/1024). */
	{"ADC_VBUSSA", 3000, 1024, 0, 0}, /*GPI0*/
	{"ADC_VBUSSB", 3000, 1024, 0, 1}, /*GPI1*/
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
