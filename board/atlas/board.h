/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Eve board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*
 * By default, enable all console messages excepted HC, ACPI and event:
 * The sensor stack is generating a lot of activity.
 */
#define CC_DEFAULT     (CC_ALL & ~(CC_MASK(CC_EVENTS) | CC_MASK(CC_LPC)))
#undef CONFIG_HOSTCMD_DEBUG_MODE
#define CONFIG_HOSTCMD_DEBUG_MODE HCDEBUG_OFF

/*
 * Allow dangerous commands.
 * TODO: Remove this config before production.
 */
#define CONFIG_SYSTEM_UNLOCKED

/* EC */
#define CONFIG_ADC
#define CONFIG_BACKLIGHT_LID
#define CONFIG_BOARD_VERSION
#define CONFIG_BOARD_SPECIFIC_VERSION
#define CONFIG_DEVICE_EVENT
#define CONFIG_DPTF
#define CONFIG_DPTF_DEVICE_ORIENTATION
/* #define CONFIG_FLASH_SIZE 0x80000 */
#define CONFIG_FPU
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_LED_COMMON
#define CONFIG_LID_SWITCH
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_LTO
#define CONFIG_CHIP_PANIC_BACKUP
#define CONFIG_SOFTWARE_PANIC
#define CONFIG_PWM
#define CONFIG_PWM_KBLIGHT

/* Internal SPI flash on NPCX7 */
#define CONFIG_FLASH_SIZE (512 * 1024) /* It's really 1MB. */
#define CONFIG_SPI_FLASH_REGS
#define CONFIG_SPI_FLASH_W25Q80 /* Internal SPI flash type. */

#define CONFIG_VBOOT_HASH
#undef CONFIG_VOLUME_BUTTONS
#define CONFIG_VSTORE
#define CONFIG_VSTORE_SLOT_COUNT 1
#define CONFIG_WATCHDOG_HELP
#define CONFIG_WIRELESS
#define xxxCONFIG_WIRELESS_SUSPEND \
	(EC_WIRELESS_SWITCH_WLAN | EC_WIRELESS_SWITCH_WLAN_POWER)
#define xxxWIRELESS_GPIO_WLAN GPIO_WLAN_OFF_L
#define xxxWIRELESS_GPIO_WLAN_POWER GPIO_PP3300_DX_WLAN

/* EC console commands */
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO
#define CONFIG_CMD_BATT_MFG_ACCESS
#undef CONFIG_CMD_CHARGER_ADC_AMON_BMON
#define CONFIG_CMD_PD_CONTROL

/* SOC */
#define CONFIG_CHIPSET_SKYLAKE
#define CONFIG_CHIPSET_HAS_PLATFORM_PMIC_RESET
#define CONFIG_CHIPSET_RESET_HOOK
#define CONFIG_ESPI
#define CONFIG_ESPI_VW_SIGNALS
#define CONFIG_LPC
#define CONFIG_KEYBOARD_BOARD_CONFIG
#define CONFIG_KEYBOARD_COL2_INVERTED
#define CONFIG_KEYBOARD_PROTOCOL_8042
#define CONFIG_KEYBOARD_SCANCODE_MUTABLE
#define CONFIG_TABLET_MODE

/* Battery */
#define CONFIG_BATTERY_CUT_OFF
#define CONFIG_BATTERY_DEVICE_CHEMISTRY "LION"
#define CONFIG_BATTERY_PRESENT_CUSTOM
#define CONFIG_BATTERY_REVIVE_DISCONNECT
#define CONFIG_BATTERY_SMART

/* Charger */
#define CONFIG_CHARGE_MANAGER
#define xxxCONFIG_CHARGE_MANAGER_EXTERNAL_POWER_LIMIT
/* #define CONFIG_CHARGE_RAMP_SW */
/* #define CONFIG_CHARGE_RAMP_HW */

#define CONFIG_CHARGER
#define CONFIG_CHARGER_V2
/* #define CONFIG_CHARGER_BD9995X */
/* #define CONFIG_CHARGER_BD9995X_CHGEN */
#define CONFIG_CHARGER_ISL9238
#define CONFIG_CHARGER_DISCHARGE_ON_AC
#define CONFIG_CHARGER_INPUT_CURRENT 512
#define CONFIG_CHARGER_LIMIT_POWER_THRESH_BAT_PCT 1
#define CONFIG_CHARGER_LIMIT_POWER_THRESH_CHG_MW 15000
#define CONFIG_CHARGER_MAINTAIN_VBAT
#define CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON 1
#define CONFIG_CHARGER_PROFILE_OVERRIDE
#define CONFIG_CHARGER_PSYS_READ
#define CONFIG_CHARGER_SENSE_RESISTOR 10
#define CONFIG_CHARGER_SENSE_RESISTOR_AC 10
#define xxxBD9995X_IOUT_GAIN_SELECT \
		BD9995X_CMD_PMON_IOUT_CTRL_SET_IOUT_GAIN_SET_20V
#define xxxBD9995X_PSYS_GAIN_SELECT \
		BD9995X_CMD_PMON_IOUT_CTRL_SET_PMON_GAIN_SET_02UAW
#define CONFIG_EXTPOWER_GPIO
#undef   CONFIG_EXTPOWER_DEBOUNCE_MS
#define  CONFIG_EXTPOWER_DEBOUNCE_MS 1000
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_X86
#define CONFIG_POWER_COMMON
#define CONFIG_POWER_SIGNAL_INTERRUPT_STORM_DETECT_THRESHOLD 30

/* Sensor */
#define CONFIG_MKBP_EVENT
#define CONFIG_MKBP_USE_HOST_EVENT
#define CONFIG_ACCEL_KXCJ9
#define CONFIG_ALS_SI114X 0x40
#define CONFIG_ALS_SI114X_INT_EVENT TASK_EVENT_CUSTOM(8)
#define CONFIG_ALS_SI114X_POLLING
#define CONFIG_TEMP_SENSOR
#define CONFIG_TEMP_SENSOR_BD99992GW
#define CONFIG_THERMISTOR_NCP15WB
#define CONFIG_ACCELGYRO_BMI160
/* #define CONFIG_MAG_BMI160_BMM150 */
#define CONFIG_ACCEL_INTERRUPTS
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT TASK_EVENT_CUSTOM(4)
/* #define BMM150_I2C_ADDRESS BMM150_ADDR0	/%* 8-bit address */
#define CONFIG_MAG_CALIBRATE
/* #define CONFIG_LID_ANGLE */
#define CONFIG_LID_ANGLE_UPDATE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
/* #define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL */

/* FIFO size is in power of 2. */
#define CONFIG_ACCEL_FIFO 1024

/* Depends on how fast the AP boots and typical ODRs */
#define CONFIG_ACCEL_FIFO_THRES (CONFIG_ACCEL_FIFO / 3)

/* Enable double tap detection */
#define CONFIG_GESTURE_DETECTION
#define CONFIG_GESTURE_HOST_DETECTION
#define CONFIG_GESTURE_SENSOR_BATTERY_TAP 1
#define CONFIG_GESTURE_SAMPLING_INTERVAL_MS 5
#define CONFIG_GESTURE_TAP_THRES_MG 100
#define CONFIG_GESTURE_TAP_MAX_INTERSTICE_T 500
#define CONFIG_GESTURE_DETECTION_MASK \
	 (1 << CONFIG_GESTURE_SENSOR_BATTERY_TAP)
#define CONFIG_GESTURE_TAP_EVENT          TASK_EVENT_CUSTOM(1024)

/* USB */
#define xxxCONFIG_USB_CHARGER
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DISCHARGE_TCPC
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT TYPEC_RP_3A0
#define CONFIG_USB_PD_PORT_COUNT 2
#define xxxCONFIG_USB_PD_VBUS_DETECT_CHARGER
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USB_PD_TCPC_LOW_POWER
#define CONFIG_USB_PD_TCPM_MUX
#define CONFIG_USB_PD_TCPM_PS8751
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PD_VBUS_MEASURE_NOT_PRESENT
#undef CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC
#define CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC 2
#define CONFIG_USB_POWER_DELIVERY

#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_SS_MUX_DFP_ONLY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP

/* Optional feature to configure npcx chip */
#define NPCX_UART_MODULE2	1 /* 1:GPIO64/65 as UART */
#define NPCX_JTAG_MODULE2	0 /* 0:GPIO21/17/16/20 as JTAG */
#define NPCX_TACH_SEL2		0 /* 0:GPIO40/73 1:GPIO93/A6 as TACH */
#define NPCX7_PWM1_SEL		0 /* GPIO C2 is not used as PWM1. */

/* %%% fixme
 see:
 ec/chip/npcx/registers.h:812
 ec/chip/npcx/i2c-npcx7.c:17
 to figure out port numbers
*/


/* I2C ports */
#define I2C_PORT_TCPC0		NPCX_I2C_PORT1_0
#define I2C_PORT_TCPC1		NPCX_I2C_PORT2_0
#define I2C_PORT_GYRO		NPCX_I2C_PORT5_0
#define I2C_PORT_ACCEL		I2C_PORT_GYRO
/* #define I2C_PORT_LID_ACCEL	NPCX_I2C_PORT2 */
#define I2C_PORT_SENSOR		NPCX_I2C_PORT3_0	/* touch/cam/als */
#define I2C_PORT_PMIC		NPCX_I2C_PORT0_0
#define I2C_PORT_BATTERY	NPCX_I2C_PORT4_1
#define I2C_PORT_CHARGER	I2C_PORT_PMIC
#define I2C_PORT_THERMAL	I2C_PORT_PMIC
/* #define I2C_PORT_MP2949		NPCX_I2C_PMIC */

/* I2C addresses */
#define I2C_ADDR_BD99992	0x60	/* %%% fixme 0x30??? */
#define I2C_ADDR_MP2949		0x40

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum board_version_list {
	BOARD_VERSION_P0,
	BOARD_VERSION_P0B,
	BOARD_VERSION_P1,
	BOARD_VERSION_P1B,
	BOARD_VERSION_EVT,
	BOARD_VERSION_DVT,
	BOARD_VERSION_PVT,
};

enum power_signal {
	X86_SLP_S0_DEASSERTED,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
	X86_SLP_SUS_DEASSERTED,
	X86_RSMRST_L_PGOOD,
	X86_PMIC_DPWROK,
	POWER_SIGNAL_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_BATTERY,	/* BD99956GW TSENSE */
	TEMP_SENSOR_AMBIENT,	/* BD99992GW SYSTHERM0 */
	TEMP_SENSOR_CHARGER,	/* BD99992GW SYSTHERM1 */
	TEMP_SENSOR_DRAM,	/* BD99992GW SYSTHERM2 */
	TEMP_SENSOR_EMMC,	/* BD99992GW SYSTHERM3 */
	TEMP_SENSOR_GYRO,
	TEMP_SENSOR_COUNT
};

/*
 * The PWM channel enums for the LEDs need to be in Red, Green, Blue order as
 * the 'set_color()' function assumes this order. The left vs right order
 * doesn't matter as long as each side follows RGB order.
 */
enum pwm_channel {
	PWM_CH_KBLIGHT,
	PWM_CH_LED_L_RED,
	PWM_CH_LED_L_GREEN,
	PWM_CH_LED_L_BLUE,
	PWM_CH_LED_R_RED,
	PWM_CH_LED_R_GREEN,
	PWM_CH_LED_R_BLUE,
	PWM_CH_COUNT
};

/*
 * For backward compatibility, to report ALS via ACPI,
 * Define the number of ALS sensors: motion_sensor copy the data to the ALS
 * memmap region.
 */
#define CONFIG_ALS
#define ALS_COUNT 1

/*
 * Motion sensors:
 * When reading through IO memory is set up for sensors (LPC is used),
 * the first 2 entries must be accelerometers, then gyroscope.
 * For BMI160, accel, gyro and compass sensors must be next to each other.
 */
enum sensor_id {
	/* LID_ACCEL = 0, */
	BASE_ACCEL,
	BASE_GYRO,
	/* BASE_MAG, */
	LID_LIGHT,
};

enum adc_channel {
	ADC_CH_COUNT
};

/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY	30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY	250000 /* us */

/* delay to turn on/off vconn */
#define PD_VCONN_SWAP_DELAY		5000   /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW		15000
#define PD_MAX_POWER_MW			45000
#define PD_MAX_CURRENT_MA		3000
#define PD_MAX_VOLTAGE_MV		20000

/* Board specific handlers */
int board_get_version(void);
void board_reset_pd_mcu(void);
void board_set_tcpc_power_mode(int port, int mode);
void led_register_double_tap(void);
void board_update_ac_status(void);

/* Sensors without hardware FIFO are in forced mode */
/* #define CONFIG_ACCEL_FORCE_MODE_MASK ((1 << LID_ACCEL) | (1 << LID_LIGHT)) */
#define CONFIG_ACCEL_FORCE_MODE_MASK (1 << LID_LIGHT)

#endif /* !__ASSEMBLER__ */

/*
 * these are mappings from signal names used in the atlas schematics
 * vs. names hard-coded in various parts of the EC codebase.
 */

#define GPIO_PMIC_SLP_SUS_L	GPIO_SLP_SUS_L_PMIC
#define GPIO_USB_C0_5V_EN	GPIO_EN_USB_C0_5V_OUT
#define GPIO_USB_C1_5V_EN	GPIO_EN_USB_C1_5V_OUT
#define GPIO_BATTERY_PRESENT_L	GPIO_EC_BATT_PRES_L
#define GPIO_USB_C0_PD_RST_L	GPIO_USB_PD_RST_L
#define GPIO_USB_C1_PD_RST_L	GPIO_USB_PD_RST_L
#define GPIO_PCH_SLP_S0_L	GPIO_SLP_S0_L
#define GPIO_PCH_SLP_SUS_L	GPIO_SLP_SUS_L_PCH
#define GPIO_AC_PRESENT		GPIO_EC_PCH_ACPRESENT
#define GPIO_BOARD_VERSION1	GPIO_EC_BRD_ID1
#define GPIO_BOARD_VERSION2	GPIO_EC_BRD_ID2
#define GPIO_BOARD_VERSION3	GPIO_EC_BRD_ID3
#define GPIO_ENABLE_BACKLIGHT	GPIO_KBD_BL_EN
#define GPIO_PCH_ACOK		GPIO_EC_PCH_ACPRESENT
#define GPIO_POWER_BUTTON_L	GPIO_PCH_PWRBTN_L /* GPIO_EC_PCH_PWR_BTN_ODL */
#define GPIO_PMIC_DPWROK	GPIO_ROP_DSW_PWROK_EC
#define GPIO_RSMRST_L_PGOOD	GPIO_ROP_EC_RSMRST_L
#define GPIO_PCH_RSMRST_L	GPIO_RSMRST_L
#define GPIO_PCH_PWRBTN_L	GPIO_EC_PCH_PWR_BTN_ODL
#define GPIO_PCH_WAKE_L		GPIO_EC_PCH_WAKE_L
#define GPIO_WP_L		GPIO_EC_WP_L
#define GPIO_CPU_PROCHOT	GPIO_EC_PROCHOT_ODL
#define GPIO_ENTERING_RW	GPIO_EC_ENTERING_RW
#define GPIO_KBD_KSO2		GPIO_EC_KB_ROW02_INV

/* ps8751 requires 1ms reset down assertion */
#define PS8XXX_RST_L_RST_H_DELAY_MS	1

#endif /* __CROS_EC_BOARD_H */
