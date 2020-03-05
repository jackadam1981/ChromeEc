/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common code for VARIANT_DEDEDE_IT8320 configuration */

#include "adc_chip.h"
#include "charge_state_v2.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/bc12/pi3usb9201.h"
#include "driver/charger/sm5803.h"
#include "driver/retimer/tusb544.h"
#include "driver/tcpm/anx7447.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/usb_mux/it5205.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "power.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "tcpci.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_VSNS_PP3300_A] = {
		"PP3300_A_PGOOD", CHIP_ADC_CH0, ADC_MAX_MVOLT, ADC_READ_MAX+1,
		0},
	[ADC_TEMP_SENSOR_1] = {
		"TEMP_SENSOR1", CHIP_ADC_CH2, ADC_MAX_MVOLT, ADC_READ_MAX+1, 0},

	[ADC_TEMP_SENSOR_2] = {
		"TEMP_SENSOR2", CHIP_ADC_CH3, ADC_MAX_MVOLT, ADC_READ_MAX+1, 0},

	[ADC_SUB_ANALOG] = {
		"SUB_ANALOG", CHIP_ADC_CH13, ADC_MAX_MVOLT, ADC_READ_MAX+1, 0},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* TODO(b/149094481): Set up ADC comparator interrupts for ITE */

/* BC12 chips */
const struct pi3usb9201_config_t pi3usb9201_bc12_chips[] = {
	{
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
	},
	{
		.i2c_port = I2C_PORT_SUB_USB_C1,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
	},
};

/* Charger chips */
const struct charger_config_t chg_chips[] = {
	[CHARGER_PRIMARY] = {
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
	[CHARGER_SECONDARY] = {
		.i2c_port = I2C_PORT_SUB_USB_C1,
		.i2c_addr_flags = SM5803_ADDR_CHARGER_FLAGS,
		.drv = &sm5803_drv,
	},
};
const unsigned int chg_cnt = ARRAY_SIZE(chg_chips);

/* I2C Ports */
const struct i2c_port_t i2c_ports[] = {
	{
		"eeprom", I2C_PORT_EEPROM, 1000, GPIO_EC_I2C_EEPROM_SCL,
		GPIO_EC_I2C_EEPROM_SDA
	},

	{
		"battery", I2C_PORT_BATTERY, 100, GPIO_EC_I2C_BATTERY_SCL,
		GPIO_EC_I2C_BATTERY_SDA
	},

	{
		"sensor", I2C_PORT_SENSOR, 400, GPIO_EC_I2C_SENSOR_SCL,
		GPIO_EC_I2C_SENSOR_SDA
	},

	{
		"sub_usbc1", I2C_PORT_SUB_USB_C1, 1000,
		GPIO_EC_I2C_SUB_USB_C1_SCL, GPIO_EC_I2C_SUB_USB_C1_SDA
	},

	{
		"usbc0", I2C_PORT_USB_C0, 1000, GPIO_EC_I2C_USB_C0_SCL,
		GPIO_EC_I2C_USB_C0_SDA
	},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* TCPCs */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_EMBEDDED,
		.drv = &it83xx_tcpm_drv,
	},
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_SUB_USB_C1,
			.addr_flags = AN7447_TCPC0_I2C_ADDR_FLAGS,
		},
		.drv = &anx7447_tcpm_drv,
		.flags = TCPC_FLAGS_TCPCI_REV2_0,
	},
};

/* USB Retimer */
const struct usb_mux usbc1_retimer = {
	.usb_port = 1,
	.i2c_port = I2C_PORT_SUB_USB_C1,
	.i2c_addr_flags = TUSB544_I2C_ADDR_FLAGS0,
	.driver = &tusb544_drv,
};

/* USB Muxes */
const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.usb_port = 0,
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = IT5205_I2C_ADDR1_FLAGS,
		.driver = &it5205_usb_mux_driver,
	},
	{
		.usb_port = 1,
		.i2c_port = I2C_PORT_SUB_USB_C1,
		.i2c_addr_flags = AN7447_TCPC0_I2C_ADDR_FLAGS,
		.driver = &anx7447_usb_mux_driver,
		.next_mux = &usbc1_retimer,
	},
};

uint16_t tcpc_get_alert_status(void)
{
	/*
	 * TCPC 0 is embedded in the EC and processes interrupts in the chip
	 * code (it83xx/intc.c)
	 */

	uint16_t status = 0;
	int regval;

	/* Check whether TCPC 1 pulled the shared interrupt line */
	if (!gpio_get_level(GPIO_USB_C1_INT_ODL)) {
		if (!tcpc_read16(1, TCPC_REG_ALERT, &regval)) {
			if (regval)
				status = PD_STATUS_TCPC_ALERT_1;
		}
	}

	return status;
}

void board_set_charge_limit(int port, int supplier, int charge_ma, int max_ma,
			    int charge_mv)
{
	int icl = MAX(charge_ma, CONFIG_CHARGER_INPUT_CURRENT);

	/*
	 * TODO(write bug): Characterize the input current limit in case a
	 * scaling needs to be applied here
	 */
	charge_set_input_current_limit(icl, charge_mv);
}

int pd_check_vconn_swap(int port)
{
	/* Allow VCONN swaps if the AP is on */
	return chipset_in_state(CHIPSET_STATE_ANY_SUSPEND | CHIPSET_STATE_ON);
}

void pd_power_supply_reset(int port)
{
	/* disable vbus, give back current, notify? */
}

int pd_set_power_supply_ready(int port)
{
	/* Set things on charger and/or TCPC */
	return EC_SUCCESS;
}

int pd_snk_is_vbus_provided(int port)
{
	/* Check charger? */
	return 0;
}

void board_pd_vconn_ctrl(int port, enum usbpd_cc_pin cc_pin, int enabled)
{
	/* set Vconn for port 0 */
}
