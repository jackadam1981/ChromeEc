/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Waddledee board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Select Baseboard features */
#define VARIANT_DEDEDE_EC_IT8320
#include "baseboard.h"

/* System unlocked in early development */
#define CONFIG_SYSTEM_UNLOCKED

/* EC console commands */
#define CONFIG_CMD_TCPC_DUMP
#define CONFIG_CMD_CHARGER_DUMP

/* Battery */
#define CONFIG_BATTERY_FUEL_GAUGE

/* BC 1.2 */
#define CONFIG_BC12_DETECT_PI3USB9201

/* Charger */
#define CONFIG_CHARGER_RAA489000
#define CONFIG_CHARGER_SENSE_RESISTOR_AC 10
#define CONFIG_CHARGER_SENSE_RESISTOR 10
#define CONFIG_OCPC_DEF_RBATT_MOHMS 22 /* R_DS(on) 11.6mOhm + 10mOhm sns rstr */
#define CONFIG_OCPC
#undef  CONFIG_CHARGER_SINGLE_CHIP

/*
 * GPIO for C1 interrupts, for baseboard use
 *
 * Note this line might already have its pull up disabled for HDMI DBs, but
 * it should be fine to set again before z-state.
 */
#define GPIO_USB_C1_INT_ODL GPIO_SUB_C1_INT_EN_RAILS_ODL

/* LED */
#define CONFIG_LED_PWM
#define CONFIG_LED_PWM_COUNT 1
#undef CONFIG_LED_PWM_NEAR_FULL_COLOR
#undef CONFIG_LED_PWM_SOC_ON_COLOR
#undef CONFIG_LED_PWM_SOC_SUSPEND_COLOR
#undef CONFIG_LED_PWM_LOW_BATT_COLOR
#define CONFIG_LED_PWM_NEAR_FULL_COLOR EC_LED_COLOR_WHITE
#define CONFIG_LED_PWM_SOC_ON_COLOR EC_LED_COLOR_WHITE
#define CONFIG_LED_PWM_SOC_SUSPEND_COLOR EC_LED_COLOR_WHITE
#define CONFIG_LED_PWM_LOW_BATT_COLOR EC_LED_COLOR_AMBER

/* PWM */
#define CONFIG_PWM
#define I2C_PORT_ACCEL      I2C_PORT_SENSOR

/* Sensors */
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO

#define CONFIG_ACCEL_BMA255		/* Lid accel */
#define CONFIG_ACCELGYRO_BMI160	/* Base accel */

/* Lid operates in forced mode, base in FIFO */
#define CONFIG_ACCEL_FORCE_MODE_MASK BIT(LID_ACCEL)
#define CONFIG_ACCEL_FIFO
#define CONFIG_ACCEL_FIFO_SIZE 256	/* Must be a power of 2 */
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO_SIZE / 3)

#define CONFIG_ACCEL_INTERRUPTS
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)

#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_UPDATE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL

#define CONFIG_TABLET_MODE
#define CONFIG_TABLET_MODE_SWITCH
#define CONFIG_GMR_TABLET_MODE

/* TCPC */
#define CONFIG_USB_PD_PORT_MAX_COUNT 2
#define CONFIG_USB_PD_TCPM_RAA489000

/* USB defines specific to external TCPCs */
#define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USB_PD_DISCHARGE_TCPC
#define CONFIG_USB_PD_TCPC_LOW_POWER

/* Variant references the TCPCs to determine Vbus sourcing */
#define CONFIG_USB_PD_5V_EN_CUSTOM

#undef PD_POWER_SUPPLY_TURN_ON_DELAY
#undef PD_POWER_SUPPLY_TURN_OFF_DELAY
#undef PD_VCONN_SWAP_DELAY
/* 20% margin added for these timings */
#define PD_POWER_SUPPLY_TURN_ON_DELAY	13080	/* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY	16080	/* us */
#define PD_VCONN_SWAP_DELAY		787	/* us */

#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_GPIO

/* Thermistors */
#define CONFIG_TEMP_SENSOR
#define CONFIG_THERMISTOR
#define CONFIG_STEINHART_HART_3V3_51K1_47K_4050B
#define CONFIG_TEMP_SENSOR_POWER_GPIO GPIO_EN_PP3300_A

/* USB Mux and Retimer */
#define CONFIG_USB_MUX_IT5205			/* C1: ITE Mux */
#define I2C_PORT_USB_MUX I2C_PORT_USB_C0	/* Required for ITE Mux */

#define CONFIG_USBC_RETIMER_TUSB544		/* C1 Redriver: TUSB544 */

#define I2C_ADDR_EEPROM_FLAGS 0x50 /* 7b address */

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum chg_id {
	CHARGER_PRIMARY,
	CHARGER_SECONDARY,
	CHARGER_NUM,
};

enum pwm_channel {
	PWM_CH_LED_AMBER,
	PWM_CH_LED_WHITE,
	PWM_CH_COUNT,
};

/* Motion sensors */
enum sensor_id {
	LID_ACCEL,
	BASE_ACCEL,
	BASE_GYRO,
	SENSOR_COUNT
};

/* ADC channels */
enum adc_channel {
	ADC_VSNS_PP3300_A,     /* ADC0 */
	ADC_TEMP_SENSOR_1,     /* ADC2 */
	ADC_TEMP_SENSOR_2,     /* ADC3 */
	ADC_TEMP_SENSOR_3,     /* ADC15*/
	ADC_CH_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_1,
	TEMP_SENSOR_2,
	TEMP_SENSOR_3,
	TEMP_SENSOR_COUNT
};

/* List of possible batteries */
enum battery_type {
	BATTERY_LGC15,
	BATTERY_PANASONIC_AP15O5L,
	BATTERY_SANYO,
	BATTERY_SONY,
	BATTERY_SMP_AP13J7K,
	BATTERY_PANASONIC_AC15A3J,
	BATTERY_LGC_AP18C8K,
	BATTERY_MURATA_AP18C4K,
	BATTERY_LGC_AP19A8K,
	BATTERY_LGC_G023,
	BATTERY_TYPE_COUNT,
};

int board_is_sourcing_vbus(int port);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
