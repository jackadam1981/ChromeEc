/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Plankton board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"

void vbus_event(enum gpio_signal signal)
{
	ccprintf("VBUS! =%d\n", gpio_get_level(signal));
	task_wake(TASK_ID_PD);
}

#include "gpio_list.h"

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_A, 0x0020, 0, MODULE_USB_PD},/* SPI1: SCK(PA5) */
	{GPIO_B, 0x0200, 2, MODULE_USB_PD},/* TIM17_CH1: (PB9) */
	{GPIO_A, 0xC000, 1, MODULE_UART},  /* USART2: PA14/PA15 */
	{GPIO_B, 0x00C0, 1, MODULE_I2C},   /* I2C SLAVE:PB6/7 */
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* USB PD CC lines sensing. Converted to mV (3300mV/4096). */
	[ADC_CH_CC1_PD] = {"CC1_PD", 3300, 4096, 0, STM32_AIN(1)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"slave",  I2C_PORT_SLAVE, 100,
		GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static void board_init(void)
{
	/* Enable interrupts on VBUS transitions. */
	gpio_enable_interrupt(GPIO_VBUS_WAKE);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

void board_set_usb_mux(int port, enum typec_mux mux, int polarity)
{
	/* reset everything */
	gpio_set_level(GPIO_USBC_SS_EN_L, 1);
	gpio_set_level(GPIO_USBC_DP_MODE_L, 1);
	gpio_set_level(GPIO_USBC_DP_POLARITY, 1);
	gpio_set_level(GPIO_USBC_SS_USB_MODE, 0);

	if (mux == TYPEC_MUX_NONE)
		/* everything is already disabled, we can return */
		return;

	if (mux == TYPEC_MUX_USB || mux == TYPEC_MUX_DOCK) {
		/* USB 3.0 uses 2 superspeed lanes */
		gpio_set_level(GPIO_USBC_SS_USB_MODE, 1);
	}

	if (mux == TYPEC_MUX_DP || mux == TYPEC_MUX_DOCK) {
		/* DP uses available superspeed lanes (x2 or x4) */
		gpio_set_level(GPIO_USBC_DP_POLARITY, polarity);
		gpio_set_level(GPIO_USBC_DP_MODE_L, 0);
	}
	/* switch on superspeed lanes */
	gpio_set_level(GPIO_USBC_SS_EN_L, 0);
}

static int command_typec(int argc, char **argv)
{
	const char * const mux_name[] = {"none", "usb", "dp", "dock"};
	enum typec_mux mux = TYPEC_MUX_NONE;
	int i;

	if (argc < 2) {
		int has_ss = !gpio_get_level(GPIO_USBC_SS_EN_L);
		int has_usb = gpio_get_level(GPIO_USBC_SS_USB_MODE);
		int has_dp = !gpio_get_level(GPIO_USBC_DP_MODE_L);
		const char *dp_str = gpio_get_level(GPIO_USBC_DP_POLARITY) ?
					"DP2" : "DP1";
		const char *usb_str = "USB";
		/* dump current state */
		ccprintf("Port CC1 %d mV (polarity:CC%d)\n",
			pd_adc_read(0, 0), pd_get_polarity(0) + 1);
		if (!has_ss)
			ccprintf("No Superspeed connection\n");
		else
			ccprintf("Superspeed %s%s%s\n",
				 has_dp ? dp_str : "",
				 has_dp && has_usb ? "+" : "",
				 has_usb ? usb_str : "");
		return EC_SUCCESS;
	}

	for (i = 0; i < ARRAY_SIZE(mux_name); i++)
		if (!strcasecmp(argv[1], mux_name[i]))
			mux = i;
	board_set_usb_mux(0, mux, pd_get_polarity(0));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(typec, command_typec,
			"[none|usb|dp|dock]",
			"Control type-C connector muxing",
			NULL);

