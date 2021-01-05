/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel TGLY-MECC1.0-ITE board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* ADC channels */
#define ADC_MAX_MVOLT  ADC_MAX_VOLT
#define ADC_TEMP_SNS_AMBIENT_CHANNEL  NPCX_ADC_CH3
#define ADC_TEMP_SNS_DDR_CHANNEL      NPCX_ADC_CH4
#define ADC_TEMP_SNS_SKIN_CHANNEL     NPCX_ADC_CH2
#define ADC_TEMP_SNS_VR_CHANNEL       NPCX_ADC_CH1


/* FAN configs */
#define CONFIG_FANS 1
#define BOARD_FAN_MIN_RPM 3000
#define BOARD_FAN_MAX_RPM 10000
#define CONFIG_PWM /* fan control requires PWM to be enabled */


#include "baseboard.h"

/* MECC config */
#define CONFIG_INTEL_RVP_MECC_VERSION_1_0

#define CONFIG_CHIPSET_TIGERLAKE
#define CONFIG_POWER_PP5000_CONTROL

/* USB ports */
#define CONFIG_USB_PD_PORT_MAX_COUNT 2
#define PD_MAX_POWER_MW              60000
#define I2C_PORT_TYPEC		         NPCX_I2C_PORT0_0

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

/* Change for Nuvoton MECC */
#define GPIO_KBD_KSO2			GPIO_EC_KSO_02_INV


/* I2C ports & Configs */
/* charger */
#define CONFIG_CHARGER_ISL9241
#define I2C_PORT_CHARGER	NPCX_I2C_PORT7_0

/* Battery */
#define I2C_PORT_BATTERY	NPCX_I2C_PORT7_0

/* Board ID */
#define I2C_PORT_PCA9555_BOARD_ID_GPIO	NPCX_I2C_PORT7_0
#define I2C_ADDR_PCA9555_BOARD_ID_GPIO	0x22

/* Port 80 */
#define I2C_PORT_PORT80		NPCX_I2C_PORT7_0
#define PORT80_I2C_ADDR		MAX695X_I2C_ADDR1_FLAGS

#ifndef __ASSEMBLER__

enum adlrvp_i2c_channel {
	/* I2C_CHAN_FLASH,i*/
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

enum mft_channel {
	MFT_CH_0,
	MFT_CH_COUNT
};

void espi_reset_pin_asserted_interrupt(enum gpio_signal signal);
void extpower_interrupt(enum gpio_signal signal);
int board_get_version(void);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
