/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADL-P-RVP-ITE board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* ITE EC variant */
#define VARIANT_INTELRVP_EC_IT8320

#include "baseboard.h"

/* MECC config */
#define CONFIG_INTEL_RVP_MECC_VERSION_1_0

/* Support early firmware selection */
#define CONFIG_VBOOT_EFS2

/* Chipset */
#define CONFIG_CHIPSET_TIGERLAKE

/*
 * Macros for GPIO signals used in common code that don't match the
 * schematic names. Signal names in gpio.inc match the schematic and are
 * then redefined here to so it's more clear which signal is being used for
 * which purpose.
 */
#define GPIO_VOLUME_DOWN_L		GPIO_VOL_DN_EC
#define GPIO_VOLUME_UP_L		GPIO_VOLUME_UP
#define GPIO_POWER_BUTTON_L		GPIO_MECH_PWR_BTN_ODL
#define GPIO_ENTERING_RW		GPIO_EC_ENTERING_RW_EC
#define GPIO_PACKET_MODE_EN		GPIO_ME_G3_TO_ME_EC
#define GPIO_LID_OPEN			GPIO_SMC_LID
#define GMR_TABLET_MODE_GPIO_L		GPIO_SLATE_MODE_INDICATION
#define GPIO_BAT_LED_RED_L		GPIO_LED_1_L_EC
#define GPIO_PWR_LED_WHITE_L		GPIO_LED_2_L_EC
#define GPIO_AC_PRESENT			GPIO_BC_ACOK_EC
#define GPIO_DC_JACK_PRESENT		GPIO_STD_ADP_PRSNT
#define GPIO_UART1_RX			GPIO_UART_SERVO_TX_EC_RX_EC
#define GPIO_WP_L			GPIO_EC_WP_ODL
#define GPIO_PCH_WAKE_L			GPIO_PCH_WAKE_N
#define GPIO_PCH_PWRBTN_L		GPIO_PM_PWRBTN_N
#define GPIO_CPU_PROCHOT		GPIO_PROCHOT_EC
#define GPIO_SYS_RESET_L		GPIO_SYS_RST_ODL_EC
#define GPIO_PCH_PLTRST_L		GPIO_PLT_RST_L
#define GPIO_ESPI_RESET_L		GPIO_ESPI_R
#define GPIO_EC_INT_L			GPIO_EC_PCH_MKBP_INT_ODL_EC
/* To avoid rework, assume Enable 3.3A & 5A rails brings DSWOPWOK signal */
#define GPIO_PG_EC_DSW_PWROK		GPIO_EC_DS4
#define GPIO_PCH_DSW_PWROK		GPIO_DSW_PWROK_EC
#define GPIO_RSMRST_L_PGOOD		GPIO_RSMRST_PWRGD_EC
#define GPIO_PG_EC_RSMRST_ODL		GPIO_RSMRST_PWRGD_EC
#define GPIO_PCH_RSMRST_L		GPIO_PM_RSMRST_EC
#define GPIO_PG_EC_ALL_SYS_PWRGD	GPIO_ALL_SYS_PWRGD_EC
#define GPIO_SLP_SUS_L			GPIO_PM_SLP_SUS_EC
#define GPIO_EN_PP3300_A		GPIO_EC_DS4
#define GPIO_PCH_SLP_S0_L		GPIO_PCH_SLP_S0_N
#define GPIO_PCH_SLP_S3_L		GPIO_SLP_S3_R_L
#define GPIO_PCH_SLP_S4_L		GPIO_SLP_S4_R_L

/* charger */
#define CONFIG_CHARGER_ISL9241
#define I2C_PORT_CHARGER	IT83XX_I2C_CH_B

/* Battery */
#define CONFIG_BATTERY_PRESENT_GPIO	GPIO_BAT_DET_EC
#define I2C_PORT_BATTERY	IT83XX_I2C_CH_B

/* Board ID */
#define I2C_PORT_PCA9555_BOARD_ID_GPIO	IT83XX_I2C_CH_B
#define I2C_ADDR_PCA9555_BOARD_ID_GPIO	0x22

/* Port 80 */
#define I2C_PORT_PORT80		IT83XX_I2C_CH_B
#define PORT80_I2C_ADDR		MAX695X_I2C_ADDR1_FLAGS

/* USB PD config */
#define CONFIG_USB_PD_PORT_MAX_COUNT	1
#define CONFIG_USB_MUX_VIRTUAL
#define PD_MAX_POWER_MW			60000
#define I2C_PORT_TYPEC			IT83XX_I2C_CH_E

/* USB-C I2C */

/* DC Jack charge ports */
#undef  CONFIG_DEDICATED_CHARGE_PORT_COUNT
#define CONFIG_DEDICATED_CHARGE_PORT_COUNT 1
#define DEDICATED_CHARGE_PORT CONFIG_USB_PD_PORT_MAX_COUNT

/* TCPC & PPC */
#define CONFIG_USB_PD_TCPM_PPC_CCGXXF

/* Config BB retimer */
#define CONFIG_USBC_RETIMER_INTEL_BB
#define I2C_PORT0_BB_RETIMER_ADDR	0x40

/* Enable VCONN */
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
/* #define CONFIG_USBC_PPC_VCONN 			 CY_UPD */
#define PD_VCONN_SWAP_DELAY		5000 /* us */

/* Enabling Thunderbolt-compatible mode */
#define CONFIG_USB_PD_TBT_COMPAT_MODE

/* Enabling USB4 mode */
#define CONFIG_USB_PD_USB4

#ifndef __ASSEMBLER__

enum adlrvp_i2c_channel {
	I2C_CHAN_FLASH,
	I2C_CHAN_BATT_CHG,
	I2C_CHAN_TYPEC,
	I2C_CHAN_COUNT,
};

enum adlrvp_charge_ports {
	TYPE_C_PORT_0,
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
