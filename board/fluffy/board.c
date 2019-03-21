/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fluffy configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_descriptor.h"
#include "registers.h"
#include "timer.h"
#include "usb_pd.h"
#include "util.h"

#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Fluffy"),
	[USB_STR_SERIALNO]     = USB_STRING_DESC("1234-a"),
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Fluffy Shell"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* Sensing the VBUS voltage at the DUT side.  Converted to mV. */
	[ADC_PPVAR_VBUS_DUT] = {"PPVAR_VBUS_DUT", 3300, 4096, 0, STM32_AIN(0)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", 1, 400, GPIO_I2C_SCL, GPIO_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

static enum gpio_signal enabled_port = GPIO_EN_C0;
static uint8_t output_en;

static void print_port_status(void)
{
	if (!output_en)
		CPRINTS("No ports enabled. zZZ");
	else
		CPRINTS("Port %d is ON", enabled_port - GPIO_EN_C0);

	CPRINTS("CC Flip: %s", gpio_get_level(GPIO_EN_CC_FLIP) ? "YES" : "NO");
	CPRINTS("USB MUX: %s", gpio_get_level(GPIO_EN_USB_MUX2) ? "ON" : "OFF");
}

static int command_cc_flip(int argc, char *argv[])
{
	int enable;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	if (!parse_bool(argv[1], &enable))
		return EC_ERROR_INVAL;

	if (output_en) {
		gpio_set_level(enabled_port, 0);
		gpio_set_level(GPIO_EN_USB_MUX2, 0);
		/* Wait long enough for CC to discharge. */
		usleep(500 * MSEC);
	}

	gpio_set_level(GPIO_EN_CC_FLIP, enable);
	/* Allow some time for new CC configuration to settle. */
	usleep(500 * MSEC);

	if (output_en) {
		gpio_set_level(enabled_port, 1);
		gpio_set_level(GPIO_EN_USB_MUX2, 1);
	}

	print_port_status();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ccflip, command_cc_flip,
			"<enable/disable>",
			"enable or disable flipping CC orientation");
/*
 * Support tca6416 I2C ioexpander.
 */
#define GPIOX_I2C_ADDR		0x40
#define GPIOX_IN_PORT_A		0x0
#define GPIOX_IN_PORT_B		0x1
#define GPIOX_OUT_PORT_A	0x2
#define GPIOX_OUT_PORT_B	0x3
#define GPIOX_DIR_PORT_A	0x6
#define GPIOX_DIR_PORT_B	0x7

static void i2c_expander_init(void)
{
	gpio_set_level(GPIO_XP_RESET_L, 1);

	/*
	 * Setup P00, P02, P04, P10, and P12 on the I/O expander as an output.
	 */
	i2c_write8(1, GPIOX_I2C_ADDR, GPIOX_DIR_PORT_A, 0xea);
	i2c_write8(1, GPIOX_I2C_ADDR, GPIOX_DIR_PORT_B, 0xfa);
}
DECLARE_HOOK(HOOK_INIT, i2c_expander_init, HOOK_PRIO_INIT_I2C+1);

/* Write a GPIO output on the tca6416 I2C ioexpander. */
static void write_ioexpander(int bank, int gpio, int val)
{
	int tmp;

	/* Read output port register */
	i2c_read8(1, GPIOX_I2C_ADDR, GPIOX_OUT_PORT_A + bank, &tmp);
	if (val)
		tmp |= BIT(gpio);
	else
		tmp &= ~BIT(gpio);
	/* Write back modified output port register */
	i2c_write8(1, GPIOX_I2C_ADDR, GPIOX_OUT_PORT_A + bank, tmp);
}

enum led_ch {
	LED_5V = 0,
	LED_9V,
	LED_12V,
	LED_15V,
	LED_20V,
	LED_COUNT,
};

static void set_led(enum led_ch led, int enable)
{
	int bank;
	int gpio;
	int tmp;

	switch (led) {
	case LED_5V:
		bank = 0;
		gpio = 0;
		break;

	case LED_9V:
		bank = 0;
		gpio = 2;
		break;

	case LED_12V:
		bank = 0;
		gpio = 4;
		break;

	case LED_15V:
		bank = 1;
		gpio = 0;
		break;

	case LED_20V:
		bank = 1;
		gpio = 2;
		break;

	default:
		return;
	}

	/*
	 * Setup the LED as an output if enabled, otherwise as an input to keep
	 * the LEDs off.
	 */
	i2c_read8(1, GPIOX_I2C_ADDR, GPIOX_DIR_PORT_A + bank, &tmp);
	if (enable)
		tmp &= ~BIT(gpio);
	else
		tmp |= BIT(gpio);
	i2c_write8(1, GPIOX_I2C_ADDR, GPIOX_DIR_PORT_A + bank, tmp);

	/* The LEDs are active low. */
	if (enable)
		write_ioexpander(bank, gpio, 0);
}

void show_output_voltage_on_leds(void);
DECLARE_DEFERRED(show_output_voltage_on_leds);

static void board_init(void)
{
	/* Do a sweeping LED dance. */
	for (enum led_ch led = 0; led < LED_COUNT; led++) {
		set_led(led, 1);
		msleep(100);
	}

	msleep(500);

	for (enum led_ch led = 0; led < LED_COUNT; led++)
		set_led(led, 0);

	show_output_voltage_on_leds();
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);


enum usb_mux {
	USB_MUX0 = 0,
	USB_MUX1,
	USB_MUX2,
	USB_MUX_COUNT,
};

static void set_mux(enum usb_mux mux, uint8_t val)
{
	enum gpio_signal c0;
	enum gpio_signal c1;
	enum gpio_signal c2;

	switch (mux) {
	case USB_MUX0:
		c0 = GPIO_USB_MUX0_C0;
		c1 = GPIO_USB_MUX0_C1;
		c2 = GPIO_USB_MUX0_C2;
		break;

	case USB_MUX1:
		c0 = GPIO_USB_MUX1_C0;
		c1 = GPIO_USB_MUX1_C1;
		c2 = GPIO_USB_MUX1_C2;
		break;

	case USB_MUX2:
		c0 = GPIO_USB_MUX2_C0;
		c1 = GPIO_USB_MUX2_C1;
		c2 = GPIO_USB_MUX2_C2;
		break;

	default:
		break;
	}

	val &= 0x7;

	gpio_set_level(c0, val & 0x1);
	gpio_set_level(c1, !!(val & 0x2));
	gpio_set_level(c2, !!(val & 0x4));
}

/* This function assumes only 1 port works at a time. */
static int command_portctl(int argc, char **argv)
{
	int port;
	int enable;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	port = atoi(argv[1]);
	if ((port < 0) || (port > 19) || !parse_bool(argv[2], &enable))
		return EC_ERROR_INVAL;

	gpio_set_level(GPIO_EN_USB_MUX2, 0);

	/*
	 * For each port, we must configure the USB 2.0 muxes and make sure that
	 * the power enables are configured as desired.
	 */

	gpio_set_level(enabled_port, 0);
	if (enabled_port != GPIO_EN_C0 + port)
		CPRINTS("Port %d: disabled", enabled_port-GPIO_EN_C0);

	/* Allow time for an unplug. */
	usleep(1 * SECOND);

	if (enable) {
		enabled_port = GPIO_EN_C0 + port;
		gpio_set_level(enabled_port, 1);

		if (port < 8) {
			set_mux(USB_MUX0, 7-port);
			set_mux(USB_MUX2, 3);
		} else if (port < 16) {
			if (port < 14)
				set_mux(USB_MUX1, 5-(port-8));
			else
				set_mux(USB_MUX1, 7-(port-14));

			set_mux(USB_MUX2, 1);
		} else {
			set_mux(USB_MUX2, 7-(port-16));
		}

		gpio_set_level(GPIO_EN_USB_MUX2, 1);
		output_en = 1;
	} else {
		gpio_set_level(enabled_port, 0);
		output_en = 0;
	}

	print_port_status();
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(portctl, command_portctl,
			"<port# 0-19> <enable/disable>",
			"enable or disable a port");

static int command_status(int argc, char **argv)
{
	int vbus_mv = adc_read_channel(ADC_PPVAR_VBUS_DUT);

	CPRINTS("PPVAR_VBUS_DUT: %dmV (raw: %d)", vbus_mv*7692/1000,
		vbus_mv);
	print_port_status();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(status, command_status, NULL, "show current status");

/*
 * According to the USB PD Spec, the minimum voltage for a fixed source is 95%
 * of the new source voltage with an additional 500mV drop.
 *
 * vSrcNew |    min   | vSrcNew(min) + vSrcValid
 *     5V  |   4.75V  | 4.25V  | 553mV
 *     9V  |   8.55V  | 8.05V  | 1047mV
 *    12V  |  11.4V   | 10.9V  | 1417mV
 *    15V  |  14.25V  | 13.75V | 1788mV
 *    20V  |  19V     | 18.5V  | 2405mV
 *
 * With the resistor divider that fluffy has, the ADC is only seeing 0.13 of the
 * actual voltage.
 */
void show_output_voltage_on_leds(void)
{
	int vbus_mv = adc_read_channel(ADC_PPVAR_VBUS_DUT);
	static int prev_vbus_mv;
	int i;
	int act;

	if (vbus_mv != ADC_READ_ERROR) {
		if (vbus_mv >= 2405) {
			for (i = 0; i < LED_COUNT; i++)
				set_led(i, 1);
		} else if (vbus_mv >= 1788) {
			for (i = 0; i < LED_20V; i++)
				set_led(i, 1);
			set_led(LED_20V, 0);
		} else if (vbus_mv >= 1417) {
			for (i = 0; i < LED_15V; i++)
				set_led(i, 1);
			set_led(LED_15V, 0);
			set_led(LED_20V, 0);
		} else if (vbus_mv >= 1047) {
			for (i = 0; i < LED_12V; i++)
				set_led(i, 1);
			set_led(LED_12V, 0);
			set_led(LED_15V, 0);
			set_led(LED_20V, 0);
		} else if (vbus_mv >= 553) {
			set_led(LED_5V, 1);
			set_led(LED_9V, 0);
			set_led(LED_12V, 0);
			set_led(LED_15V, 0);
			set_led(LED_20V, 0);
		} else {
			for (i = 0; i < LED_COUNT; i++)
				set_led(i, 0);
		}

		act = (vbus_mv * 7692) / 1000;
		if ((vbus_mv > prev_vbus_mv+2) || (vbus_mv < prev_vbus_mv-2)) {
			CPRINTS("PPVAR_VBUS_DUT: %d mV (raw: %d)", act,
				vbus_mv);
			prev_vbus_mv = vbus_mv;
		}
	}
	hook_call_deferred(&show_output_voltage_on_leds_data,
			   500 * MSEC);
}
