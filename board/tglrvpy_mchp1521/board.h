/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel TGLY-MECC1.0-ITE board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* TGLY_MEC1521_MECC_H1 */
/* ITE EC Variant */
//// #define VARIANT_INTELRVP_EC_IT8320
/* MCHP EC variant */
#define VARIANT_INTELRVP_EC_MCHP

/* TGLY_MEC1521_MECC_H1 */
/* mchp mec1521 specific !!! */
/* UART for EC console */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 0

/*
 * External parallel crystal between XTAL1 and XTAL2 pins.
 *   #define CONFIG_CLOCK_SRC_EXTERNAL
 *   #define CONFIG_CLOCK_CRYSTAL
 * External single ended 32KHz 50% duty cycle input clock.
 *   #define CONFIG_CLOCK_SRC_EXTERNAL
 *   #undef CONFIG_CLOCK_CRYSTAL
 * Use internal silicon 32KHz oscillator
 *   #undef CONFIG_CLOCK_SRC_EXTERNAL
 *   CONFIG_CLOCK_CRYSTAL is a don't care
 */
#undef	CONFIG_CLOCK_SRC_EXTERNAL
//// #define	CONFIG_CLOCK_CRYSTAL

/*
 * MEC1521H loads firmware using QMSPI controller
 * CONFIG_SPI_FLASH_PORT is the index into
 * spi_devices[] in board.c
 */
#define CONFIG_SPI_FLASH_PORT 0
#define CONFIG_SPI_FLASH

/*
 * Google uses smaller flashes on chromebook boards
 * MCHP SPI test dongle for EVB uses 16MB W25Q128F
 * Configure for smaller flash is OK for testing except
 * for SPI flash lock bit.
 */
 #define CONFIG_FLASH_SIZE_BYTES 524288
 #define CONFIG_SPI_FLASH_W25X40

/*
 * Enable extra SPI flash and generic SPI
 * commands via EC UART
 */
#define CONFIG_CMD_SPI_FLASH
#define CONFIG_CMD_SPI_XFER


/* MEC152x does not have GP-SPI controllers */
#undef CONFIG_MCHP_GPSPI
/* End of mchp mec1521 specific !!! */

/* FAN configs */
#define CONFIG_FANS 1
#define BOARD_FAN_MIN_RPM 3000
#define BOARD_FAN_MAX_RPM 10000

#include "baseboard.h"

/* MECC config */
#define CONFIG_INTEL_RVP_MECC_VERSION_1_0

#define CONFIG_CHIPSET_TIGERLAKE
#define CONFIG_POWER_PP5000_CONTROL

/* USB ports */
#define CONFIG_USB_PD_PORT_MAX_COUNT 2
#define PD_MAX_POWER_MW              60000
/* TGLY_MEC1521_MECC_H1 */
#define IT83XX_I2C_CH_C		     MCHP_I2C_PORT6
#define I2C_PORT_TYPEC		     IT83XX_I2C_CH_C

/* USB MUX */
#define CONFIG_USB_MUX_VIRTUAL

/* Defined to suppress compilation errors */
/* TCPC & PPC */
#define CONFIG_USB_PD_TCPM_PPC_CCGXXF

/* Config BB retimer */
#define CONFIG_USBC_RETIMER_INTEL_BB
#define I2C_PORT0_BB_RETIMER_ADDR	0x42
#define I2C_PORT1_BB_RETIMER_ADDR	0x41

/* DC Jack charge ports */
#undef  CONFIG_DEDICATED_CHARGE_PORT_COUNT
#define CONFIG_DEDICATED_CHARGE_PORT_COUNT CONFIG_USB_PD_PORT_MAX_COUNT
#define DEDICATED_CHARGE_PORT CONFIG_USB_PD_PORT_MAX_COUNT

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_AC_PRESENT			GPIO_BC_ACOK_EC
#define GPIO_EC_INT_L			GPIO_EC_PCH_MKBP_INT_ODL_EC
#define GPIO_EN_PP3300			GPIO_EC_DS3

/* Change for TGL-Y RVP */
#define GPIO_EN_PP5000			GPIO_EC_DS3

#define GPIO_ENTERING_RW		GPIO_EC_ENTERING_RW_EC
#define GPIO_LID_OPEN			GPIO_SMC_LID
#define GPIO_PACKET_MODE_EN		GPIO_EC_H1_PACKET_MODE_EC
#define GPIO_PCH_WAKE_L			GPIO_PCH_WAKE_N
#define GPIO_PCH_PWRBTN_L		GPIO_PM_PWRBTN_N_EC
#define GPIO_PCH_RSMRST_L		GPIO_PM_RSMRST_EC
#define GPIO_PCH_SLP_S0_L		GPIO_PCH_SLP_S0_N
#define GPIO_PCH_SLP_S3_L		GPIO_SLP_S3_R_L
#define GPIO_PG_EC_DSW_PWROK		GPIO_EDP_BKLT_EN
#define GPIO_POWER_BUTTON_L		GPIO_MECH_PWR_BTN_ODL
#define GPIO_RSMRST_L_PGOOD		GPIO_RSMRST_PWRGD_EC
#define GPIO_CPU_PROCHOT		GPIO_PROCHOT_EC
#define GPIO_SYS_RESET_L		GPIO_SYS_RST_ODL_EC
#define GPIO_WP_L			GPIO_EC_WP_ODL
#define GPIO_VOLUME_UP_L		GPIO_VOLUME_UP
#define GPIO_VOLUME_DOWN_L		GPIO_VOL_DN_EC_R
#define GPIO_DC_JACK_PRESENT		GPIO_STD_ADP_PRSNT
#define GPIO_ESPI_RESET_L		GPIO_ESPI_RST_R
#define GPIO_UART1_RX			GPIO_UART_SERVO_TX_EC_RX
#define CONFIG_BATTERY_PRESENT_GPIO	GPIO_BAT_DET_EC
#define GPIO_BAT_LED_RED_L		GPIO_LED_1_L_EC
#define GPIO_PWR_LED_WHITE_L		GPIO_LED_2_L_EC
#define GPIO_SLP_SUS_L			GPIO_SMC_SHUTDOWN
#define GPIO_PG_EC_RSMRST_ODL		GPIO_RSMRST_PWRGD_EC
#define GPIO_PG_EC_ALL_SYS_PWRGD	GPIO_ALL_SYS_PWRGD_EC
#define GPIO_PCH_DSW_PWROK		GPIO_DSW_PWROK_EC
#define GPIO_ALL_SYS_PWRGD		GPIO_ALL_SYS_PWRGD_EC
#define GPIO_FAN_POWER_EN		GPIO_EC_THRM_SEN_PWRGATE_N
#define GPIO_EN_PP3300_A		GPIO_PM_SLP_SUS_EC
#define GMR_TABLET_MODE_GPIO_L		GPIO_SLATE_MODE_INDICATION

/* I2C ports & Configs */
/* TGLY_MEC1521_MECC_H1 */
//// #define CONFIG_IT83XX_SMCLK2_ON_GPC7

/* charger */
#define CONFIG_CHARGER_ISL9241
/* TGLY_MEC1521_MECC_H1 */
#define IT83XX_I2C_CH_B		MCHP_I2C_PORT0
#define I2C_PORT_CHARGER	IT83XX_I2C_CH_B

/* Battery */
#define I2C_PORT_BATTERY	IT83XX_I2C_CH_B

/* Board ID */
#define I2C_PORT_PCA9555_BOARD_ID_GPIO	IT83XX_I2C_CH_B
#define I2C_ADDR_PCA9555_BOARD_ID_GPIO	0x22

/* Port 80 */
#define I2C_PORT_PORT80		IT83XX_I2C_CH_B
#define PORT80_I2C_ADDR		MAX695X_I2C_ADDR1_FLAGS

#ifndef __ASSEMBLER__

enum adlrvp_i2c_channel {
/* TGLY_MEC1521_MECC_H1 */
	//// I2C_CHAN_FLASH,
	I2C_CHAN_BATT_CHG,
	I2C_CHAN_TYPEC,
	I2C_CHAN_COUNT,
};

enum adlrvp_charge_ports {
	TYPE_C_PORT_0,
	TYPE_C_PORT_1,
};

enum battery_type {
	BATTERY_SIMPLO_SMP_HHP_408,
	BATTERY_SIMPLO_SMP_CA_445,
	BATTERY_TYPE_COUNT,
};

void espi_reset_pin_asserted_interrupt(enum gpio_signal signal);
void extpower_interrupt(enum gpio_signal signal);
int board_get_version(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
