/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* IT8xxx2 PD development board configuration */

#include "adc_chip.h"
#include "console.h"
#include "it83xx_pd.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "timer.h"
#include "usb_pd_tcpm.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define USB_PD_PORT_ITE_0   0
#define USB_PD_PORT_ITE_1   1
#define USB_PD_PORT_ITE_2   2
#define USB_PD_PORT_ITE_3   3
#define RESISTIVE_DIVIDER   11

int board_get_battery_soc(void)
{
	CPRINTS("%s", __func__);
	return 100;
}

const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USB_PD_PORT_ITE_0] = {
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it83xx_tcpm_drv,
		/* Alert is active-low, push-pull */
		.flags = 0,
	},
	[USB_PD_PORT_ITE_1] = {
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		/* TCPC is embedded within EC so no i2c config needed */
		.drv = &it83xx_tcpm_drv,
		/* Alert is active-low, push-pull */
		.flags = 0,
	},
	[USB_PD_PORT_ITE_2] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = IT83XX_I2C_CH_E,
			.addr_flags = 0x52,//needn't shift?
		},
		.drv = &it885x_tcpm_drv,
		.flags = TCPC_FLAGS_TCPCI_REV2_0,
	},
	[USB_PD_PORT_ITE_3] = {
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = IT83XX_I2C_CH_E,
			.addr_flags = 0x52,//needn't shift?
		},
		.drv = &it885x_tcpm_drv,
		.flags = TCPC_FLAGS_TCPCI_REV2_0,
	},
};

void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	int cc1_enabled = 0, cc2_enabled = 0;

	if (cc_pin != USBPD_CC_PIN_1)
		cc2_enabled = enabled;
	else
		cc1_enabled = enabled;

	if (port == USBPD_PORT_A) {
		gpio_set_level(GPIO_USBPD_PORTA_CC2_VCONN, cc2_enabled);
		gpio_set_level(GPIO_USBPD_PORTA_CC1_VCONN, cc1_enabled);
	} else if (port == USBPD_PORT_B) {
		gpio_set_level(GPIO_USBPD_PORTB_CC2_VCONN, cc2_enabled);
		gpio_set_level(GPIO_USBPD_PORTB_CC1_VCONN, cc1_enabled);
	} else if (port == USBPD_PORT_C) {
		gpio_set_level(GPIO_USBPD_PORTC_CC2_VCONN, cc2_enabled);
		gpio_set_level(GPIO_USBPD_PORTC_CC1_VCONN, cc1_enabled);
	}

	CPRINTS("p%d Vconn cc1 %d, cc2 %d (On/Off)", port, cc1_enabled,
		cc2_enabled);
}

void board_pd_vbus_ctrl(int port, int enabled)
{
	CPRINTS("p%d Vbus %d(En/Dis)", port, enabled);

	if (port == USBPD_PORT_A) {
		gpio_set_level(GPIO_USBPD_PORTA_VBUS_INPUT, !enabled);
		gpio_set_level(GPIO_USBPD_PORTA_VBUS_OUTPUT, enabled);
		if (!enabled) {
			gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 1);
			udelay(10*MSEC); /* 10ms is a try and error value */
		}
		gpio_set_level(GPIO_USBPD_PORTA_VBUS_DROP, 0);
	} else if (port == USBPD_PORT_B) {
		gpio_set_level(GPIO_USBPD_PORTB_VBUS_INPUT, !enabled);
		gpio_set_level(GPIO_USBPD_PORTB_VBUS_OUTPUT, enabled);
		if (!enabled) {
			gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 1);
			udelay(10*MSEC); /* 10ms is a try and error value */
		}
		gpio_set_level(GPIO_USBPD_PORTB_VBUS_DROP, 0);
	} else if (port == USBPD_PORT_C) {
		gpio_set_level(GPIO_USBPD_PORTC_VBUS_INPUT, !enabled);
		gpio_set_level(GPIO_USBPD_PORTC_VBUS_OUTPUT, enabled);
		if (!enabled) {
			gpio_set_level(GPIO_USBPD_PORTC_VBUS_DROP, 1);
			udelay(10*MSEC); /* 10ms is a try and error value */
		}
		gpio_set_level(GPIO_USBPD_PORTC_VBUS_DROP, 0);
	}

	if (enabled)
		udelay(10*MSEC); /* 10ms is a try and error value */
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	CPRINTS("p%d %s", port, __func__);
}

static void tcpc_alert_event(enum gpio_signal s)
{
	//int port = (s == GPIO_USB_C0_INT_ODL) ? 0 : 1;
	//read reg check which port

	schedule_deferred_pd_interrupt(port);
}

static void usb_c2_c3_interrupt(enum gpio_signal s)
{
	/*
	 * The interrupt line is shared between the TCPC NO:and BC 1.2 detection
	 * chip.  Therefore we'll need to check both ICs.
	 */
	tcpc_alert_event(s);
	//task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12, 0);
}

uint16_t tcpc_get_alert_status(void)
{
	uint16_t status = 0;
	int regval;
	int port;

	//read reg to know which port event that we should read.

	/*
	 * The interrupt line is shared between the TCPC and BC1.2 detector IC.
	 * Therefore, go out and actually read the alert registers to report the
	 * alert status.
	 */
	if (!tcpc_read16(port, TCPC_REG_ALERT, &regval)) {
		/* The TCPCI Rev 1.0 spec says to ignore bits 14:12. */
		if (!(tcpc_config[0].flags & TCPC_FLAGS_TCPCI_REV2_0))
			regval &= ~((1 << 14) | (1 << 13) | (1 << 12));

		if (regval)
			status |= (port == 2 ? PD_STATUS_TCPC_ALERT_2 : PD_STATUS_TCPC_ALERT_3);
	}

	return status;
}

/*
 * PWM channels. Must be in the exactly same order as in enum pwm_channel.
 * There total three 16 bits clock prescaler registers for all pwm channels,
 * so use the same frequency and prescaler register setting is required if
 * number of pwm channel greater than three.
 */
const struct pwm_t pwm_channels[] = {
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/*
	 * The register value of ADC reading convert to mV (= register value *
	 * reading max mV / 10 bit solution 1024).
	 * NOTE: If the ADC channel measure VBUS:
	 *       the max reading mv value is the result of resistive divider,
	 *       so VBUS = reading max mv * resistive divider
	 *       (check HW schematic).
	 */
	[ADC_VBUSSA] = {
		.name = "ADC_VBUSSA",
		.factor_mul = ADC_MAX_MVOLT * RESISTIVE_DIVIDER,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH7, /* GPI7, ADC7 */
	},
	[ADC_VBUSSB] = {
		.name = "ADC_VBUSSB",
		.factor_mul = ADC_MAX_MVOLT * RESISTIVE_DIVIDER,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH3, /* GPI3, ADC3 */
	},
	[ADC_VBUSSC] = {
		.name = "ADC_VBUSSC",
		.factor_mul = ADC_MAX_MVOLT * RESISTIVE_DIVIDER,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH16, /* GPL0, ADC16 */
	},
	[ADC_EVB_CH_13] = {
		.name = "ADC_EVB_CH_13",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH13, /* GPL1, ADC13 */
	},
	[ADC_EVB_CH_14] = {
		.name = "ADC_EVB_CH_14",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH14, /* GPL2, ADC14 */
	},
	[ADC_EVB_CH_15] = {
		.name = "ADC_EVB_CH_15",
		.factor_mul = ADC_MAX_MVOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
		.channel = CHIP_ADC_CH15, /* GPL3, ADC15 */
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);
