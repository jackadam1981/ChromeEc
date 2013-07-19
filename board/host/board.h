/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Emulator board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* Assertion support */
#define CONFIG_DEBUG
#define CONFIG_ASSERT_HELP

/* Optional features */
#define CONFIG_EXTPOWER_GPIO
#define CONFIG_HOSTCMD
#define CONFIG_HOST_EMU
#define CONFIG_LID_SWITCH
#define CONFIG_POWER_BUTTON
#define CONFIG_TEMP_SENSOR
#define CONFIG_CHARGER_MOCK
#ifdef HAS_TASK_CHIPSET
#define CONFIG_CHIPSET_MOCK
#endif

/* Keyboard protocol */
#ifdef KB_8042
#define CONFIG_KEYBOARD_PROTOCOL_8042
#else
#define CONFIG_KEYBOARD_PROTOCOL_MKBP
#endif

#define CONFIG_WP_ACTIVE_HIGH

enum gpio_signal {
	GPIO_EC_INT,
	GPIO_LID_OPEN,
	GPIO_POWER_BUTTON_L,
	GPIO_WP,
	GPIO_ENTERING_RW,
	GPIO_AC_PRESENT,

	GPIO_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_CPU = 0,
	TEMP_SENSOR_BOARD,
	TEMP_SENSOR_CASE,

	TEMP_SENSOR_COUNT
};

enum adc_channel {
	/* Charger current in mA. */
	ADC_CH_CHARGER_CURRENT,

	/* AC Adapter ID voltage in mV */
	ADC_AC_ADAPTER_ID_VOLTAGE,

	ADC_CH_COUNT
};

/* Hush warnings from inline functions in smart_battery.h */
#define I2C_PORT_BATTERY 0


#endif /* __BOARD_H */
