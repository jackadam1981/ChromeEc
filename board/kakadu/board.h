/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Configuration for Kakadu */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#define BQ27541_ADDR	0x55
#define VARIANT_KUKUI_BATTERY_BQ27541
#define VARIANT_KUKUI_POGO_KEYBOARD

#define VARIANT_KUKUI_CHARGER_MT6370
#define VARIANT_KUKUI_EC_IT81202
#define VARIANT_KUKUI_TABLET_PWRBTN

#include "baseboard.h"

/* TODO: remove me once we fix IT83XX_ILM_BLOCK_SIZE out of space issue */
#undef CONFIG_LTO

#define CONFIG_USB_MUX_IT5205
#define CONFIG_VOLUME_BUTTONS
#define CONFIG_USB_MUX_RUNTIME_CONFIG

/* kakadu use the TCPM_MT6370
 * do we need to keep the old PD config ?
 * if keep, there is a TCPC_LPM issue.
 */
/*
#undef CONFIG_USB_DRP_ACC_TRYSRC
#undef CONFIG_USB_PD_DECODE_SOP
#undef CONFIG_USB_PD_TCPMV2
#undef CONFIG_USB_PD_ITE_ACTIVE_PORT_COUNT
#undef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
#define CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT TYPEC_RP_3A0 //only for TCPMV1
#define CONFIG_USB_PD_TCPMV1
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
*/
#undef CONFIG_USB_PD_ITE_ACTIVE_PORT_COUNT
#undef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
#define CONFIG_USB_PD_VBUS_DETECT_TCPC


/* Battery */
#define BATTERY_DESIRED_CHARGING_CURRENT    3500  /* mA */

#define CONFIG_CHARGER_MT6370_BACKLIGHT


/* Motion Sensors */
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_ACCEL_INTERRUPTS
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(LID_ACCEL)
#define CONFIG_ACCELGYRO_ICM42607	/* Base accel second source*/
#define CONFIG_ACCELGYRO_ICM42607_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(LID_ACCEL)

/* Camera VSYNC */
#define CONFIG_SYNC
#define CONFIG_SYNC_COMMAND
#define CONFIG_SYNC_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(VSYNC)

/* I2C ports */
#define I2C_PORT_CHARGER  IT83XX_I2C_CH_C
#define I2C_PORT_TCPC0    IT83XX_I2C_CH_C
#define I2C_PORT_USB_MUX  IT83XX_I2C_CH_C
#define I2C_PORT_BATTERY  IT83XX_I2C_CH_B
#define I2C_PORT_VIRTUAL_BATTERY I2C_PORT_BATTERY
#define I2C_PORT_ACCEL    IT83XX_I2C_CH_B
#define I2C_PORT_BC12     IT83XX_I2C_CH_B

/* Route sbs host requests to virtual battery driver */
#define VIRTUAL_BATTERY_ADDR_FLAGS 0x0B

/* MKBP */
#define CONFIG_MKBP_INPUT_DEVICES
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_EVENT_WAKEUP_MASK \
	(BIT(EC_MKBP_EVENT_SENSOR_FIFO) | BIT(EC_MKBP_EVENT_HOST_EVENT))

#define PD_OPERATING_POWER_MW 15000

#ifndef __ASSEMBLER__

enum adc_channel {
	/* Real ADC channels begin here */
	ADC_BOARD_ID = 0,
	ADC_EC_SKU_ID,
	ADC_BATT_ID,
	ADC_POGO_ADC_INT_L,
	ADC_CH_COUNT
};

/* power signal definitions */
enum power_signal {
	AP_IN_S3_L,
	PMIC_PWR_GOOD,

	/* Number of signals */
	POWER_SIGNAL_COUNT,
};

/* Motion sensors */
enum sensor_id {
	LID_ACCEL = 0,
	LID_GYRO,
	VSYNC,
	SENSOR_COUNT,
};

enum charge_port {
	CHARGE_PORT_USB_C,
};

#include "gpio_signal.h"
#include "registers.h"

#ifdef SECTION_IS_RO
/* Interrupt handler for AP jump to BL */
void emmc_ap_jump_to_bl(enum gpio_signal signal);
#endif

void board_reset_pd_mcu(void);
int board_get_version(void);
void pogo_adc_interrupt(enum gpio_signal signal);
int board_discharge_on_ac(int enable);
void motion_interrupt(enum gpio_signal signal);

/* Enable double tap detection */
#define CONFIG_GESTURE_DETECTION
#define CONFIG_GESTURE_SENSOR_DOUBLE_TAP
#define CONFIG_GESTURE_TAP_SENSOR 0
#define CONFIG_GESTURE_SENSOR_DOUBLE_TAP_FOR_HOST
#define CONFIG_GESTURE_SAMPLING_INTERVAL_MS 5
#define CONFIG_GESTURE_TAP_THRES_MG 100
#define CONFIG_GESTURE_TAP_MAX_INTERSTICE_T 500
#define CONFIG_GESTURE_DETECTION_MASK BIT(CONFIG_GESTURE_TAP_SENSOR)

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
