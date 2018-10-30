/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* IT83xx development board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "it83xx_pd.h"
#include "fan.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "intc.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "lpc.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
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
#else
/* EC EVB */
void pd_task(void)
{
	while (1)
		task_wait_event(-1);
}
#endif

#include "gpio_list.h"

/*
 * PWM channels. Must be in the exactly same order as in enum pwm_channel.
 * There total three 16 bits clock prescaler registers for all pwm channels,
 * so use the same frequency and prescaler register setting is required if
 * number of pwm channel greater than three.
 */
const struct pwm_t pwm_channels[] = {
	{7, 0,                     30000, PWM_PRESCALER_C4},
	{0, PWM_CONFIG_DSLEEP,     100,   PWM_PRESCALER_C6},
};

BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = 0,	/* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = -1,
};

const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 1500,
	.rpm_start = 1500,
	.rpm_max = 6500,
};

struct fan_t fans[] = {
	{ .conf = &fan_conf_0, .rpm = &fan_rpm_0, },
};
BUILD_ASSERT(ARRAY_SIZE(fans) == CONFIG_FANS);

/*
 * PWM HW channelx binding tachometer channelx for fan control.
 * Four tachometer input pins but two tachometer modules only,
 * so always binding [TACH_CH_TACH0A | TACH_CH_TACH0B] and/or
 * [TACH_CH_TACH1A | TACH_CH_TACH1B]
 */
const struct fan_tach_t fan_tach[] = {
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_NULL,  -1, -1, -1},
	{TACH_CH_TACH0A, 2, 50, 30},
};
BUILD_ASSERT(ARRAY_SIZE(fan_tach) == PWM_HW_CH_TOTAL);

/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_POWER_BUTTON_L, GPIO_LID_OPEN
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* Initialize board. */
static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* Convert to mV (3000mV/1024). */
	{"ADC_VBUSSA", 3000, 1024, 0, CHIP_ADC_CH0}, /* GPI0, ADC0 */
	{"ADC_VBUSSB", 3000, 1024, 0, CHIP_ADC_CH1}, /* GPI1, ADC1 */
	{"ADC_EVB_CH_13", 3000, 1024, 0, CHIP_ADC_CH13}, /* GPL0, ADC13 */
	{"ADC_EVB_CH_14", 3000, 1024, 0, CHIP_ADC_CH14}, /* GPL1, ADC14 */
	{"ADC_EVB_CH_15", 3000, 1024, 0, CHIP_ADC_CH15}, /* GPL2, ADC15 */
	{"ADC_EVB_CH_16", 3000, 1024, 0, CHIP_ADC_CH16}, /* GPL3, ADC16 */
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 35,
	.debounce_down_us = 5 * MSEC,
	.debounce_up_us = 40 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

#define INT64_MIN (-INT64_MAX - 1LL)

int64_t strtoll(const char *nptr, char **endptr, int base)
{
	const char *s = nptr;
	uint64_t acc;
	int c;
	uint64_t cutoff;
	int neg = 0, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+') {
		c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	cutoff = neg ? -(uint64_t)INT64_MIN : INT64_MAX;
	cutlim = cutoff % (uint64_t)base;
	cutoff /= (uint64_t)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}

	if (any < 0)
		acc = neg ? INT64_MIN : INT64_MAX;
	else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return acc;
}

static int command_lldiv(int argc, char **argv)
{
	int64_t dividend;
	int64_t divisor;
	char *e;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;
	dividend = strtoll(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;
	divisor = strtoll(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	ccprintf("%ld / %ld = %ld\n", dividend, divisor, dividend/divisor);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(lldiv, command_lldiv,
			"dividend divisor",
			"signed 64 bit division");

static int command_ulldiv(int argc, char **argv)
{
	uint64_t dividend;
	uint64_t divisor;
	char *e;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;
	dividend = strtoul(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;
	divisor = strtoul(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	ccprintf("%ld / %ld = %ld\n", dividend, divisor, dividend/divisor);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ulldiv, command_ulldiv,
			"dividend divisor",
			"unsigned 64 bit division");

/*
 * I2C channels (A, B, and C) are using the same timing registers (00h~07h)
 * at default.
 * In order to set frequency independently for each channels,
 * We use timing registers 09h~0Bh, and the supported frequency will be:
 * 50KHz, 100KHz, 400KHz, or 1MHz.
 * I2C channels (D, E and F) can be set different frequency on different ports.
 * The I2C(D/E/F) frequency depend on the frequency of SMBus Module and
 * the individual prescale register.
 * The frequency of SMBus module is 24MHz on default.
 * The allowed range of I2C(D/E/F) frequency is as following setting.
 * SMBus Module Freq = PLL_CLOCK / ((IT83XX_ECPM_SCDCR2 & 0x0F) + 1)
 * (SMBus Module Freq / 510) <=  I2C Freq <= (SMBus Module Freq / 8)
 * Channel D has multi-function and can be used as UART interface.
 * Channel F is reserved for EC debug.
 */

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"battery", IT83XX_I2C_CH_C, 100, GPIO_I2C_C_SCL, GPIO_I2C_C_SDA},
	{"evb-1",   IT83XX_I2C_CH_A, 100, GPIO_I2C_A_SCL, GPIO_I2C_A_SDA},
	{"evb-2",   IT83XX_I2C_CH_B, 100, GPIO_I2C_B_SCL, GPIO_I2C_B_SDA},
	{"opt-4",   IT83XX_I2C_CH_E, 100, GPIO_I2C_E_SCL, GPIO_I2C_E_SDA},
};

const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	{ CONFIG_SPI_FLASH_PORT, 0, -1},
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);
