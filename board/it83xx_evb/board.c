/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* IT83xx development board configuration */

#include "adc_chip.h"
#include "pwm.h"
#include "pwm_chip.h"
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
#include "registers.h"
#include "spi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "util.h"

#if IT83XX_PD_EVB
int board_get_battery_soc(void)
{
	return 100;
}

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
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
#else
/* EC EVB */
void pd_task(void)
{
	while (1)
		task_wait_event(-1);
}
#endif

#include "gpio_list.h"
=======
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

/*
 * PWM channels. Must be in the exactly same order as in enum pwm_channel.
 * There total three 16 bits clock prescaler registers for all pwm channels,
 * so use the same frequency and prescaler register setting is required if
 * number of pwm channel greater than three.
 */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_FAN] = {
		.channel = PWM_HW_CH_DCR7,
		.flags = 0,
		.freq_hz = 30000,
		.pcfsr_sel = PWM_PRESCALER_C4,
	},
	[PWM_CH_WITH_DSLEEP_FLAG] = {
		.channel = PWM_HW_CH_DCR0,
		.flags = PWM_CONFIG_DSLEEP,
		.freq_hz = 100,
		.pcfsr_sel = PWM_PRESCALER_C6,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Convert to mV (3000mV/1024). */
	[ADC_VBUSSA] = {
		.name = "ADC_VBUSSA",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH0, /* GPI0, ADC0 */
	},
	[ADC_VBUSSB] = {
		.name = "ADC_VBUSSB",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH1, /* GPI1, ADC1 */
	},
	[ADC_EVB_CH_13] = {
		.name = "ADC_EVB_CH_13",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH13, /* GPL0, ADC13 */
	},
	[ADC_EVB_CH_14] = {
		.name = "ADC_EVB_CH_14",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH14, /* GPL1, ADC14 */
	},
	[ADC_EVB_CH_15] = {
		.name = "ADC_EVB_CH_15",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH15, /* GPL2, ADC15 */
	},
	[ADC_EVB_CH_16] = {
		.name = "ADC_EVB_CH_16",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH16, /* GPL3, ADC16 */
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
