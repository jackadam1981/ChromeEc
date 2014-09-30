/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Strago board-specific configuration */

#include "gpio.h"
#include "registers.h"
#include "util.h"
#include "power_button.h"
#include "lid_switch.h"
#include "power.h"
#include "extpower.h"
#include "i2c.h"
#include "adc.h"
#include "adc_chip.h"
#include "charger.h"
#include "charge_state.h"
#include "switch.h"
#include "thermal.h"
#include "driver/temp_sensor/tmp432.h"
#include "temp_sensor.h"
#include "temp_sensor_chip.h"

#define GPIO_KB_INPUT (GPIO_INPUT | GPIO_PULL_UP)
#define GPIO_KB_OUTPUT (GPIO_ODR_HIGH)
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
 #define GPIO_KB_OUTPUT_COL2 (GPIO_OUT_LOW)
#else
 #define GPIO_KB_OUTPUT_COL2 (GPIO_OUT_HIGH)
#endif

#include "gpio_list.h"

/* ADC channels */
const struct adc_t adc_channels[] = {
	[ADC_BC_PMON] = {"BC_PMON", 1, 1, 0, MEC1322_ADC_CH(1)},
	[ADC_BC_IADP] = {"BC_IADP", 1, 1, 0, MEC1322_ADC_CH(2)},
	[ADC_BC_IDHG] = {"BC_IDHG", 1, 1, 0, MEC1322_ADC_CH(3)},
};

BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);


/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_ALL_SYS_PGOOD,     1, "ALL_SYS_PWRGD"},
	{GPIO_RSMRST_L_PGOOD,    1, "RSMRST_N_PWRGD"},
	{GPIO_PCH_SLP_S3_L,      1, "SLP_S3#_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,      1, "SLP_S4#_DEASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

const struct i2c_port_t i2c_ports[]  = {
	{"batt_chg", I2C_PORT_CHARGER, 100},
	{"thermal",  I2C_PORT_THERMAL, 100},
	{ "accel",   I2C_PORT_ACCEL , 100},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/*
 * Temperature sensors data; must be in same order as enum temp_sensor_id.
 * Sensor index and name must match those present in coreboot:
 *     src/mainboard/google/${board}/acpi/dptf.asl
 */
const struct temp_sensor_t temp_sensors[] = {
	{"TMP432_Internal", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
		TMP432_IDX_LOCAL, 4},
	{"TMP432_Sensor_1", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
		TMP432_IDX_REMOTE1, 4},
	{"TMP432_Sensor_2", TEMP_SENSOR_TYPE_BOARD, tmp432_get_val,
		TMP432_IDX_REMOTE2, 4},
	{"Battery", TEMP_SENSOR_TYPE_BATTERY, charge_temp_sensor_get_val, 0, 4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/* Thermal limits for each temp sensor. All temps are in degrees K. Must be in
 * same order as enum temp_sensor_id. To always ignore any temp, use 0.
 */
struct ec_thermal_config thermal_params[] = {
	{{0, 0, 0}, 0, 0}, /* TMP432_Internal */
	{{0, 0, 0}, 0, 0}, /* TMP432_Sensor_1 */
	{{0, 0, 0}, 0, 0}, /* TMP432_Sensor_2 */
	{{0, 0, 0}, 0, 0}, /* Battery Sensor */
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);

/**
 * Discharge battery when on AC power for factory test.
 */
int board_discharge_on_ac(int enable)
{
	return charger_discharge_on_ac(enable);
}

