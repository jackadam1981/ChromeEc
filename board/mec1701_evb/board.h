/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* mec1701_evb board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#define CONFIG_BRINGUP

/*
 * Debug on EVB with CONFIG_CHIPSET_DEBUG
 * Keep WDG disabled and JTAG enabled.
 * CONFIG_BOARD_PRE_INIT enables JTAG early
 */
#define CONFIG_CHIPSET_DEBUG

#ifdef CONFIG_CHIPSET_DEBUG
#ifndef CONFIG_BOARD_PRE_INIT
#define CONFIG_BOARD_PRE_INIT
#endif
#endif

/*
 * Enable Trace FIFO Debug port
 * When this is undefined all TRACEn() and tracen()
 * macros are defined as blank.
 * Uncomment this define to enable these messages.
 * Only enable if GPIO's 0171 & 0171 are available therefore
 * define this at the board level.
 * #define CONFIG_MEC1701_TFDP
 */

/*
 * Enable various requirements of Intel Skylake/Kabylake RVP3
 * reference board.
 * Support ACPI EC0 two byte board ID command.
 */
#define CONFIG_BOARD_SKL_RVP3

/*
 * Enable call to board level function when
 * RSMRST_L_PGOOD and RSMRST# are different.
 */
#define CONFIG_BOARD_HAS_BEFORE_RSMRST

/*
 * Enable call to board level function for
 * handling Kabylake/Skylake RVP board's
 * ALL_SYS_PWRGD signal.
 */
#define CONFIG_BOARD_HAS_ALL_SYS_PWRGD

/*
 * EVB eSPI test mode (no eSPI master connected)
 */
#define EVB_NO_ESPI_TEST_MODE


/*
 * DEBUG
 * Disable ARM Cortex-M4 write buffer so
 * exceptions become synchronous.
 *
 * #define CONFIG_DEBUG_DISABLE_WRITE_BUFFER
 */

/* New eSPI slave configuration items */

/*
 * Maximum clock frequence eSPI EC slave advertises
 * Values in MHz are 20, 25, 33, 50, and 66
 */
/* KBL + EVB fly-wire hook up only supports 20MHz */
#define CONFIG_ESPI_EC_MAX_FREQ		20

/*
 * EC eSPI slave advertises IO lanes
 * 0 = Single
 * 1 = Single and Dual
 * 2 = Single and Quad
 * 3 = Single, Dual, and Quad
 */
/* KBL + EVB fly-wire hook up only support Single mode */
#define CONFIG_ESPI_EC_MODE		0

/*
 * Bit map of eSPI channels EC advertises
 * bit[0] = 1 Peripheral channel
 * bit[1] = 1 Virtual Wire channel
 * bit[2] = 1 OOB channel
 * bit[3] = 1 Flash channel
 */
#define CONFIG_ESPI_EC_CHAN_BITMAP	0x0F

/*
 * MEC1701 EVB + KBL RVP3 board uses eSPI default of
 * Platform Reset being a virtual wire.
 */
#define CONFIG_ESPI_PLTRST_IS_VWIRE

/*
 * Allow dangerous commands.
 * TODO(shawnn): Remove this config before production.
 */
#define CONFIG_SYSTEM_UNLOCKED

/* Optional features */
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_ACCEL_KX022
#define CONFIG_ALS
#define CONFIG_ALS_OPT3001
#define CONFIG_BATTERY_CUT_OFF
#define CONFIG_BATTERY_PRESENT_GPIO GPIO_BAT_PRESENT_L
#define CONFIG_BATTERY_SMART
#define CONFIG_BOARD_VERSION
#define CONFIG_BUTTON_COUNT 2
#define CONFIG_CHARGE_MANAGER
#define CONFIG_CHARGE_RAMP_HW


#define CONFIG_CHARGER
#define CONFIG_CHARGER_V2

#define CONFIG_CHARGER_DISCHARGE_ON_AC
#define CONFIG_CHARGER_ISL9237
#define CONFIG_CHARGER_ILIM_PIN_DISABLED
#define CONFIG_CHARGER_INPUT_CURRENT 512

/*
 * TODO MCHP disable this for Kabylake eSPI bring up
 * #define CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON 1
*/

#define CONFIG_CHARGER_NARROW_VDC
#define CONFIG_CHARGER_PROFILE_OVERRIDE
#define CONFIG_CHARGER_SENSE_RESISTOR 10
#define CONFIG_CHARGER_SENSE_RESISTOR_AC 20
#define CONFIG_CMD_CHARGER_ADC_AMON_BMON

#define CONFIG_CHIPSET_SKYLAKE
#define CONFIG_CHIPSET_RESET_HOOK
/* MCHP MEC1701 eSPI */
#define CONFIG_ESPI
#define CONFIG_ESPI_VW_SIGNALS
/* MCHP MEC1701 eSPI end */
#define CONFIG_CLOCK_CRYSTAL
#define CONFIG_EXTPOWER_GPIO
#define CONFIG_HOSTCMD_PD
#define CONFIG_HOSTCMD_PD_PANIC
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define CONFIG_KEYBOARD_PROTOCOL_8042
#define CONFIG_LED_COMMON
#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_SENSOR_BASE 0
#define CONFIG_LID_ANGLE_SENSOR_LID 2
#define CONFIG_LID_SWITCH
/*
 * Enable MEC17xx Low Power Idle support
 */
#define CONFIG_LOW_POWER_IDLE

/*
 * MEC17XX debug EC code turn off GCC link-time-optimization
 * #define CONFIG_LTO
*/
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_X86
#define CONFIG_POWER_COMMON
#define CONFIG_POWER_SIGNAL_INTERRUPT_STORM_DETECT_THRESHOLD 30

/* All data won't fit in data RAM.  So, moving boundary slightly. */
/* TODO MCHP #undef CONFIG_RO_SIZE */
/*
 * MEC1701H has contiguous 256KB SRAM. 224KB code, 32KB data.
 * Re-apportion as
 * lower 192KB for Code
 * upper 64KB for Data
 */
/* !!! This overrides chip/mec1701/config_flash_layout.h !!!
 * #define CONFIG_RO_SIZE (192 * 1024)
 */

/*
 * MEC1701H SCI is virtual wire on eSPI
 *#define CONFIG_SCI_GPIO GPIO_PCH_SCI_L
 */
/* We're space constrained on GLaDOS, so reduce the UART TX buffer size. */
#undef CONFIG_UART_TX_BUF_SIZE
/* MEC1701H increase UART buffer size from 512 to 1024 */
#define CONFIG_UART_TX_BUF_SIZE 1024
#define CONFIG_USB_CHARGER
#define CONFIG_USB_MUX_PI3USB30532
#define CONFIG_USB_MUX_PS8740
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_LOG_SIZE 512
#define CONFIG_USB_PD_PORT_COUNT 2
#define CONFIG_USB_PD_TCPM_TCPCI
/*
 * #define CONFIG_USB_PD_TCPC
 * #define CONFIG_USB_PD_TCPM_STUB
 */
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PD_VBUS_DETECT_GPIO
#define CONFIG_USB_SWITCH_PI3USB9281
#define CONFIG_USB_SWITCH_PI3USB9281_CHIP_COUNT 2
#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_SS_MUX_DFP_ONLY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_VBOOT_HASH

/*
 * MEC1701H loads firmware using QMSPI controller
 * which is defined as Port 2 in chip layer.
 */
#define CONFIG_SPI_FLASH_PORT (MEC17XX_QMSPI0_SHD_PORT)
#define CONFIG_SPI_FLASH
/*
 * Google uses smaller flashes on chromebook boards
 * MCHP SPI test dongle for EVB uses 16MB W25Q128F
 * Configure for smaller flash is OK for testing except
 * for SPI flash lock bit.
 */
 #define CONFIG_FLASH_SIZE 524288
 #define CONFIG_SPI_FLASH_W25X40
/*
 * #define CONFIG_FLASH_SIZE 0x1000000
 * #define CONFIG_SPI_FLASH_W25Q128
 */

/*
 * Enable extra SPI flash tests via EC UART
 */
#define CONFIG_CMD_SPI_FLASH

/*
 * Enable SHA-256 tests via EC UART
 */
#define CONFIG_CMD_SHA256_TEST


#define CONFIG_TEMP_SENSOR
#define CONFIG_TEMP_SENSOR_BD99992GW
#define CONFIG_THERMISTOR_NCP15WB
#define CONFIG_DPTF

/* SPI Accelerometer connected to GP-SPI0 */
#define CONFIG_MEC1701_GP_SPI
/* SPI Accelerometer */
#define CONFIG_SPI_ACCEL_PORT (MEC17XX_GPSPI0_PORT)

/*
 * Enable 1 slot of secure temporary storage to support
 * suspend/resume with read/write memory training.
 */
#define CONFIG_VSTORE
#define CONFIG_VSTORE_SLOT_COUNT 1

#define CONFIG_WATCHDOG_HELP

#if 0 /* TODO - No wireless on EVB */
#define CONFIG_WIRELESS
#define CONFIG_WIRELESS_SUSPEND \
	(EC_WIRELESS_SWITCH_WLAN | EC_WIRELESS_SWITCH_WLAN_POWER)

/* Wireless signals */
#define WIRELESS_GPIO_WLAN GPIO_WLAN_OFF_L
#define WIRELESS_GPIO_WLAN_POWER GPIO_PP3300_WLAN_EN
#endif

/* LED signals */
#define GPIO_BAT_LED_RED GPIO_CHARGE_LED_1
#define GPIO_BAT_LED_GREEN GPIO_CHARGE_LED_2

/* I2C ports */
#define I2C_CONTROLLER_COUNT	4
#define I2C_PORT_COUNT		5

/*
 * Map I2C Ports to Controllers for this board.
 *
 * I2C Controller 0 ---- Port 0 -> PMIC, USB Charger 2
 *                   |-- Port 2 -> USB Charger 1, USB Mux
 *
 * I2C Controller 1 ---- Port 3 -> PD MCU, TCPC
 * I2C Controller 2 ---- Port 4 -> ALS, Accel
 * I2C Controller 3 ---- Port 5 -> Battery, Charger
 *
 * All other ports set to 0xff (not used)
 */
#define MEC17XX_I2C0_PORTS_BITMAP   ((1u << 0) + (1u << 2))
#define MEC17XX_I2C1_PORTS_BITMAP   (1u << 3)
#define MEC17XX_I2C2_PORTS_BITMAP   (1u << 4)
#define MEC17XX_I2C3_PORTS_BITMAP   (1u << 5)


#define I2C_PORT_PMIC           MEC17XX_I2C_PORT10
#define I2C_PORT_USB_CHARGER_1	MEC17XX_I2C_PORT2
#define I2C_PORT_USB_MUX        MEC17XX_I2C_PORT2
#define I2C_PORT_USB_CHARGER_2	MEC17XX_I2C_PORT2
#define I2C_PORT_PD_MCU         MEC17XX_I2C_PORT3
#define I2C_PORT_TCPC           MEC17XX_I2C_PORT3
#define I2C_PORT_ALS            MEC17XX_I2C_PORT4
#define I2C_PORT_ACCEL          MEC17XX_I2C_PORT4
#define I2C_PORT_BATTERY        MEC17XX_I2C_PORT5
#define I2C_PORT_CHARGER        MEC17XX_I2C_PORT5

/* Thermal sensors read through PMIC ADC interface */
#define I2C_PORT_THERMAL	I2C_PORT_PMIC

/* Ambient Light Sensor address */
#define OPT3001_I2C_ADDR	OPT3001_I2C_ADDR1

/* Modules we want to exclude */
#undef CONFIG_CMD_HASH
#undef CONFIG_CMD_TEMP_SENSOR
#undef CONFIG_CMD_TIMERINFO
/* #undef CONFIG_CONSOLE_CMDHELP */

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

/* ADC signal */
enum adc_channel {
	ADC_VBUS,
	ADC_AMON_BMON,
	ADC_PSYS,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

/* power signal definitions */
enum power_signal {
	X86_RSMRST_L_PWRGD = 0,
	X86_SLP_S3_DEASSERTED,
	X86_SLP_S4_DEASSERTED,
	X86_SLP_SUS_DEASSERTED,
	X86_PMIC_DPWROK,
	X86_ALL_SYS_PWRGD,	/* TODO MCHP Kabylake */

	/* Number of X86 signals */
	POWER_SIGNAL_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_BATTERY,

	/* These temp sensors are only readable in S0 */
	TEMP_SENSOR_AMBIENT,
	TEMP_SENSOR_CHARGER,
	TEMP_SENSOR_DRAM,
	TEMP_SENSOR_WIFI,

	TEMP_SENSOR_COUNT
};

/* Light sensors */
enum als_id {
	ALS_OPT3001 = 0,

	ALS_COUNT
};

/* TODO: determine the following board specific type-C power constants */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

/* delay to turn on/off vconn */
#define PD_VCONN_SWAP_DELAY 5000 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       45000
#define PD_MAX_CURRENT_MA     3000

/* Try to negotiate to 20V since i2c noise problems should be fixed. */
#define PD_MAX_VOLTAGE_MV     20000

/* Map I2C port to controller */
int board_i2c_p2c(int port);

/* Map SPI port to controller */
int board_spi_p2c(int port);

/* Reset PD MCU */
void board_reset_pd_mcu(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
