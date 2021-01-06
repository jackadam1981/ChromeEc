/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Configuration for Kukui */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#define VARIANT_KUKUI_JACUZZI
#define VARIANT_KUKUI_BATTERY_SMART
#define VARIANT_KUKUI_CHARGER_ISL9238

#ifndef SECTION_IS_RW
#define VARIANT_KUKUI_NO_SENSORS
#endif /* SECTION_IS_RW */

#include "baseboard.h"

/* exclude unnecessary configs */
#undef CONFIG_COMMON_GPIO_SHORTNAMES
#undef CONFIG_IO_EXPANDER
#undef CONFIG_IO_EXPANDER_IT8801
#undef CONFIG_KEYBOARD_BOARD_CONFIG
#undef CONFIG_KEYBOARD_NOT_RAW
/* exclude chip-specific configs */
#undef CONFIG_UART_RX_DMA
#undef CONFIG_STM_HWTIMER32
#undef CONFIG_WATCHDOG_HELP
/* include chip-specific configs */
#define CONFIG_IT83XX_HARD_RESET_BY_GPG1
#define CONFIG_IT83XX_VCC_1P8V
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096
/* option */
#undef CONFIG_LTO

#define CONFIG_CMD_MD
#define CONFIG_HIBERNATE
#define CONFIG_CMD_APTHROTTLE
#define CONFIG_CMD_CRASH
#define CONFIG_CMD_HCDEBUG
#define CONFIG_CMD_IDLE_STATS
#define CONFIG_CMD_MMAPINFO
#define CONFIG_CMD_PWR_AVG
#define CONFIG_CMD_REGULATOR
#define CONFIG_CMD_RW
#define CONFIG_CMD_SHMEM
#define CONFIG_CMD_SLEEPMASK
#define CONFIG_CMD_SLEEPMASK_SET
#define CONFIG_CMD_STACKOVERFLOW
#define CONFIG_CMD_SYSLOCK
#define CONFIG_HOSTCMD_FLASHPD
#define CONFIG_HOSTCMD_RWHASHPD
#define CONFIG_CONSOLE_CMDHELP

#undef CONFIG_CHIPSET_POWER_SEQ_VERSION
#define CONFIG_CHIPSET_POWER_SEQ_VERSION 1
#undef CONFIG_SYSTEM_UNLOCKED

#define CONFIG_BATTERY_HW_PRESENT_CUSTOM

#define CONFIG_CHARGER_PSYS

#define CONFIG_CHARGER_RUNTIME_CONFIG

#define CONFIG_BC12_DETECT_PI3USB9201

#define CONFIG_EXTPOWER_GPIO
#undef CONFIG_EXTPOWER_DEBOUNCE_MS
#define CONFIG_EXTPOWER_DEBOUNCE_MS 200

#undef CONFIG_I2C_BITBANG
#undef CONFIG_I2C_NACK_RETRY_COUNT
#define CONFIG_I2C_NACK_RETRY_COUNT 10
#define CONFIG_SMBUS_PEC

#define CONFIG_USB_PD_TCPM_FUSB302
#define CONFIG_USB_PD_DISCHARGE_GPIO
#define CONFIG_USB_PD_TCPC_LOW_POWER

#define CONFIG_USB_MUX_IT5205

/* Motion Sensors */
#ifndef VARIANT_KUKUI_NO_SENSORS
#define CONFIG_ACCEL_KX022	/* Lid accel */
#define CONFIG_ACCELGYRO_BMI160 /* Base accel */
#define CONFIG_ACCEL_INTERRUPTS
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#define CONFIG_ALS
#define CONFIG_CMD_ACCEL_INFO

#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL

#define CONFIG_ACCEL_FORCE_MODE_MASK BIT(LID_ACCEL)

#endif /* VARIANT_KUKUI_NO_SENSORS */

/* I2C ports */
#define I2C_PORT_BC12               IT83XX_I2C_CH_C
#define I2C_PORT_TCPC0              IT83XX_I2C_CH_C
#define I2C_PORT_USB_MUX            IT83XX_I2C_CH_C
#define I2C_PORT_CHARGER            IT83XX_I2C_CH_A
#define I2C_PORT_SENSORS            IT83XX_I2C_CH_B
#define I2C_PORT_BATTERY            IT83XX_I2C_CH_A
#define I2C_PORT_VIRTUAL_BATTERY    I2C_PORT_BATTERY

/* Enable Accel over SPI */
#define CONFIG_SPI_ACCEL_PORT    0  /* The first SPI master port (SPI2) */

#define CONFIG_KEYBOARD_PROTOCOL_MKBP
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_GPIO
/* Define the MKBP events which are allowed to wakeup AP in S3. */
#define CONFIG_MKBP_HOST_EVENT_WAKEUP_MASK \
		(EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN) |\
		 EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON))

#define CONFIG_LED_ONOFF_STATES

#ifndef __ASSEMBLER__

enum adc_channel {
	/* Real ADC channels begin here */
	ADC_BOARD_ID = 0,
	ADC_EC_SKU_ID,
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
	BASE_ACCEL,
	BASE_GYRO,
	SENSOR_COUNT,
};

enum charge_port {
	CHARGE_PORT_USB_C,
};

enum battery_type {
	BATTERY_PANASONIC_AC15A3J,
	BATTERY_PANASONIC_AC16L5J,
	BATTERY_LGC_AC16L8J,
	BATTERY_PANASONIC_AC16L5J_KT00205009,
	BATTERY_TYPE_COUNT,
};

#include "gpio_signal.h"
#include "registers.h"

#ifdef SECTION_IS_RO
/* Interrupt handler for AP jump to BL */
void emmc_ap_jump_to_bl(enum gpio_signal signal);
#endif

void bc12_interrupt(enum gpio_signal signal);
void board_reset_pd_mcu(void);
int board_get_version(void);
int board_is_sourcing_vbus(int port);

/* returns the i2c port number of charger/battery */
int board_get_charger_i2c(void);
int board_get_battery_i2c(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
