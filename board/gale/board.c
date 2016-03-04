/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* gale board configuration */

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
#include "util.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)

#include "gpio_list.h"

const struct adc_t adc_channels[] = {
	/* PA1: STM32_AIN 1 */
	[ADC_CC1]       = {"CC1",  3300, 4096, 0, STM32_AIN(1)},
	/* PA3: STM32_AIN 3 */
	[ADC_CC2]       = {"CC2",  3300, 4096, 0, STM32_AIN(3)},
	/* PB0: 1/2 VBUS voltage */
	[ADC_VBUS]      = {"VBUS", 6600, 4096, 0, STM32_AIN(8)},
	/* PB0: sense resistor: 10 mOhm scale 50x */
	[ADC_CUR_SENSE] = {"CUR",  6600, 4096, 0, STM32_AIN(9)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct i2c_port_t i2c_ports[] = {
	{"pd", I2C_PORT_SLAVE, 1000, GPIO_SLAVE_I2C_SCL, GPIO_SLAVE_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Gale AP power state */
static int8_t gale_power;

/* USB superspeed mux */

static int usbmux_init(int i2c_addr)
{
	gpio_set_level(GPIO_USB_SS_MUX_EN_L, 1);
	return EC_SUCCESS;
}

static int usbmux_set_mux(int i2c_addr, mux_state_t mux_state)
{
	gpio_set_level(GPIO_USB_CC_POLARITY,
		       !(mux_state & MUX_POLARITY_INVERTED));
	gpio_set_level(GPIO_USB_SS_MUX_EN_L,
		       !(mux_state & MUX_USB_ENABLED));

	return EC_SUCCESS;
}

static int usbmux_get_mux(int i2c_addr, mux_state_t *mux_state)
{
	*mux_state = gpio_get_level(GPIO_USB_SS_MUX_EN_L) ? MUX_USB_ENABLED : 0;
	return EC_SUCCESS;
}

const struct usb_mux_driver usbmux_driver = {
	.init = usbmux_init,
	.set  = usbmux_set_mux,
	.get  = usbmux_get_mux,
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_COUNT] = {
	{
		.port_addr = 0,
		.driver    = &usbmux_driver,
	},
};


/* SoC power control */

static void set_error_led(int level)
{
	gpio_set_level(GPIO_ERROR_LED, level);
}

static void power_on_ap(void)
{
	/* turn on system, IO and memory power rails */
	gpio_set_level(GPIO_SYS_PWR_EN, 1);       /* system power */
	gpio_set_level(GPIO_VDD_3P3_EN, 1);       /* 3.3v - io */
	gpio_set_level(GPIO_VDD_3P3_2G_EN, 1);
	gpio_set_level(GPIO_VDD_1P8_EN, 1);       /* 1.8v */
	gpio_set_level(GPIO_VDD_1P35_EN, 1);      /* 1.35v - memory */
	msleep(10);
	gpio_set_level(GPIO_VDD_1P1_CPU_EN, 1);   /* 1.1v - cpu */
	gale_power = 1;
}
DECLARE_DEFERRED(power_on_ap);

static void power_off_ap(void)
{
	/* drive MCU_INT_L low */
	gpio_set_level(GPIO_MCU_INT_L, 0);
	/* turn off AP core power */
	gpio_set_level(GPIO_VDD_1P1_CPU_EN, 0);
	/* turn off 1.35v, 1.8v and 3.3v power rails */
	gpio_set_level(GPIO_VDD_1P35_EN, 0);
	gpio_set_level(GPIO_VDD_1P8_EN, 0);
	gpio_set_level(GPIO_VDD_3P3_2G_EN, 0);
	gpio_set_level(GPIO_VDD_3P3_EN, 0);
	gpio_set_level(GPIO_SYS_PWR_EN, 0);

	gale_power = 0;
}
DECLARE_DEFERRED(power_off_ap);

void board_power_supply_ready(int ready)
{
	set_error_led(!ready);
	if (ready) {
		hook_call_deferred(power_on_ap, 0);
		gpio_set_level(GPIO_USB_CC_POLARITY, !pd_get_polarity(0));
	} else {
		hook_call_deferred(power_off_ap, 0);
	}
}

/**
 * Check Type-C CC connects to Rp
 *
 * @param cc  PD cc voltage status
 *
 * @return 0   If not connected to source, otherwise 1;
 */
static int is_connected_rp(int cc)
{
	if (cc == TYPEC_CC_VOLT_SNK_DEF ||
	    cc == TYPEC_CC_VOLT_SNK_1_5 ||
	    cc == TYPEC_CC_VOLT_SNK_3_0)
		return 1;
	return 0;
}

static int get_typec_current(int cc)
{
	if (cc == TYPEC_CC_VOLT_SNK_3_0)
		return 3000;
	if (cc == TYPEC_CC_VOLT_SNK_1_5)
		return 1500;
	return 900;
}

static const char *get_cc_state(int cc)
{
	if (is_connected_rp(cc)) {
		switch (get_typec_current(cc)) {
		case 3000:
			return "RP3000";
		case 1500:
			return "RP1500";
		default:
			return "RPUSB";
		}
	}

	return "OPEN";
}

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
}

static void board_init(void)
{
	set_error_led(1);
	/* dev board */
	hook_call_deferred(power_on_ap, 700*MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Console commands */

static int gale_command_power(int argc, char **argv)
{
	int val;

	if (argc >= 2)
		if (parse_bool(argv[1], &val))
			board_power_supply_ready(val);

	CPRINTF("ap power - %s\n", gale_power ? "on" : "off");
	return EC_SUCCESS;
}

static int gale_command_ssmux(int argc, char **argv)
{
	int val;

	if (argc >= 2)
		if (parse_bool(argv[1], &val))
			gpio_set_level(GPIO_USB_SS_MUX_EN_L,
				       !val);

	CPRINTF("ss mux   - %s\n",
		gpio_get_level(GPIO_USB_SS_MUX_EN_L) ? "disable" : "enable");
	return EC_SUCCESS;
}

static int gale_command_polarity(int argc, char **argv)
{
	if (argc >= 2)
		gpio_set_level(GPIO_USB_CC_POLARITY,
			       !strtoi(argv[1], NULL, 0));

	CPRINTF("polarity - %d\n",
		!gpio_get_level(GPIO_USB_CC_POLARITY));
	return EC_SUCCESS;
}

static int gale_command_cc(int argc, char **argv)
{
	int cc1, cc2;
	int v1 = adc_read_channel(ADC_CC1);
	int v2 = adc_read_channel(ADC_CC2);

	tcpm_get_cc(0, &cc1, &cc2);
	CPRINTF("cc1,cc2  - %dmv(%s), %dmv(%s)\n",
		v1, get_cc_state(cc1),
		v2, get_cc_state(cc2));
	return EC_SUCCESS;
}

static int gale_command_vbus(int argc, char **argv)
{
	CPRINTF("vbus     - %dmv %dma\n",
		adc_read_channel(ADC_VBUS),
		adc_read_channel(ADC_CUR_SENSE));
	return EC_SUCCESS;
}

static int gale_command(int argc, char **argv)
{
	const struct {
		const char *name;
		int (*func)(int, char**);
	} sub_commands[] = {
		{"power",    gale_command_power},
		{"ssmux",    gale_command_ssmux},
		{"polarity", gale_command_polarity},
		{"cc",       gale_command_cc},
		{"vbus",     gale_command_vbus},
	};

	int i;

	if (argc < 2) {
		for (i = 0; i < ARRAY_SIZE(sub_commands); i++)
			sub_commands[i].func(0, NULL);
		return EC_SUCCESS;
	}

	for (i = 0; i < ARRAY_SIZE(sub_commands); i++) {
		if (!strncasecmp(argv[1],
				 sub_commands[i].name,
				 strlen(sub_commands[i].name)))
			return sub_commands[i].func(argc - 1, argv + 1);
	}

	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND(gale, gale_command,
			"[power [on|off]|ssmux [on|off]|polarity [0|1]|cc|vbus",
			"Get and set gale controls", NULL);

