/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel TGLY-MECC1.0-ITE board-specific configuration */

/*
 * Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first.
 */

/* Power sequencing interrupts */
GPIO_INT(ALL_SYS_PWRGD_EC,	PIN(7, 0), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(RSMRST_PWRGD_EC,	PIN(3, 7), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_S0_N,		PIN(A, 1), GPIO_INT_BOTH, power_signal_interrupt)

/* Change for TGL-Y RVP */
GPIO_INT(EDP_BKLT_EN,		PIN(E, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SMC_SHUTDOWN,		PIN(B, 1), GPIO_INT_BOTH, power_signal_interrupt)

#ifndef CONFIG_HOSTCMD_ESPI_VW_SLP_S3
GPIO_INT(SLP_S3_R_L,		PIN(B, 0), GPIO_INT_BOTH, power_signal_interrupt)
#endif
#ifndef CONFIG_HOSTCMD_ESPI_VW_SLP_S4
GPIO_INT(SLP_S4_R_L,		PIN(A, 5), GPIO_INT_BOTH, power_signal_interrupt)
#endif

/* Button interrupts */
GPIO_INT(VOLUME_UP,		PIN(6, 1), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(VOL_DN_EC_R,		PIN(0, 3), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(SMC_LID,		PIN(0, 1), GPIO_INT_BOTH | GPIO_PULL_UP, lid_interrupt)
GPIO_INT(MECH_PWR_BTN_ODL,	PIN(0, 0), GPIO_INT_BOTH, power_button_interrupt)
GPIO_INT(EC_WP_ODL,		PIN(9, 4), GPIO_INT_BOTH, switch_interrupt)

/* DC Jack presence coming from +VADP_OUT */
GPIO_INT(STD_ADP_PRSNT, 	PIN(0, 2), GPIO_INT_BOTH, board_dc_jack_interrupt) /* DC_JACK_PRESENT */

GPIO_INT(BC_ACOK_EC,		PIN(C, 6), GPIO_INT_BOTH, extpower_interrupt) /* AC Present */

/* Sensor Interrupts */
GPIO_INT(SLATE_MODE_INDICATION,	PIN(E, 5), GPIO_INT_BOTH | GPIO_SEL_1P8V, gmr_tablet_switch_isr)

/* Power sequencing GPIOs */
GPIO(SYS_RST_ODL_EC,		PIN(C, 5), GPIO_ODR_HIGH)
GPIO(PROCHOT_EC,		PIN(A, 7), GPIO_INPUT)
GPIO(PM_RSMRST_EC,		PIN(A, 6), GPIO_OUT_LOW)
GPIO(PM_PWRBTN_N_EC,		PIN(9, 7), GPIO_ODR_HIGH)
GPIO(EC_SPI_OE_N,		PIN(6, 0), GPIO_OUT_LOW)

GPIO(EC_PCH_MKBP_INT_ODL_EC,	PIN(F, 5), GPIO_ODR_HIGH)

/* Changes for TGL_Y RVP */
GPIO(EC_DS3,			PIN(C, 4), GPIO_OUT_LOW)
GPIO(PM_SLP_SUS_EC, 		PIN(8, 6), GPIO_OUT_LOW)

/*
 * PCH_SYS_PWROK is an input, driven by the Silego chip. The common x86
 * power sequencing expects that PCH_SYS_PWROK is an output and will drive
 * this signal if GPIO_PCH_SYS_PWROK is configured. Map this pin as no-connect
 * so that state can be monitored using the console.
 */
GPIO(PCH_PWROK_EC_R,	PIN(A, 0), GPIO_INPUT)
GPIO(SYS_PWROK_EC,	PIN(9, 3), GPIO_INPUT)
GPIO(DSW_PWROK_EC,	PIN(9, 5), GPIO_OUT_LOW)

/* Host communication GPIOs */
#ifndef CONFIG_HOSTCMD_ESPI
GPIO(PLT_RST_L, 	PIN(A, 2), GPIO_INPUT | GPIO_PULL_UP) /* PCH_PLTRST_L */
#endif
GPIO(PCH_WAKE_N,	PIN(A, 4), GPIO_ODR_HIGH)

/* Battery present */
GPIO(BAT_DET_EC,	PIN(7, 6), GPIO_INPUT)

/* LED */
GPIO(LED_1_L_EC,	PIN(B, 6), GPIO_OUT_HIGH)	/* BAT_LED_GREEN_L LED_2_L */
GPIO(LED_2_L_EC,	PIN(B, 7), GPIO_OUT_HIGH)	/* AC_LED_GREEN_L LED_1_L */

/* H1 pins */
GPIO(EC_H1_PACKET_MODE_EC,	PIN(E, 2), GPIO_OUT_LOW)
GPIO(EC_ENTERING_RW_EC,		PIN(D, 4), GPIO_OUT_LOW)

/* Case Closed Debug Mode */
UNIMPLEMENTED(CCD_MODE_ODL)
GPIO(NC_CCD_MODE_ODL,	PIN(F, 4), GPIO_INPUT)

/* FAN control pins */
GPIO(EC_THRM_SEN_PWRGATE_N,	PIN(C, 0), GPIO_OUT_LOW)

/*
 * I2C pins should be configure as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(SMB_BS_CLK,		PIN(B, 3), GPIO_INPUT)	/* I2C7_SCL */
GPIO(SMB_BS_DATA,		PIN(B, 2), GPIO_INPUT)	/* I2C7_SDA */

/* P0  */
GPIO(USBC_TCPC_I2C_CLK_P0,	PIN(B, 5), GPIO_INPUT)	/* I2C0_SCL */
GPIO(USBC_TCPC_I2C_DATA_P0,	PIN(B, 4), GPIO_INPUT)	/* I2C0_SDA */

/* BB retimer controls */
GPIO(USBC_TCPC_PPC_ALRT_P1, 	PIN(F, 1), GPIO_OUT_LOW) /* P0 - Load switch enable */
GPIO(USBC_TCPC_ALRT_P3,		PIN(6, 7), GPIO_OUT_LOW) /* P0 - Retimer reset */
GPIO(USBC_TCPC_ALRT_P2,		PIN(6, 3), GPIO_OUT_LOW) /* P1 - Load switch enable */
GPIO(USBC_TCPC_PPC_ALRT_P0, 	PIN(F, 0), GPIO_OUT_LOW) /* P1 - Retimer reset */

/* Unused 1.8V or 3.3V compatiable pins */
GPIO(SUSWARN,			PIN(D, 5), GPIO_INPUT)
GPIO(USBC_TCPC_PPC_ALRT_P2, 	PIN(F, 2), GPIO_INPUT)
GPIO(USBC_TCPC_PPC_ALRT_P3, 	PIN(F, 3), GPIO_INPUT)
GPIO(USBC_TCPC_ALRT_P1,		PIN(6, 2), GPIO_INPUT)
GPIO(CPU_C10_GATE,		PIN(9, 6), GPIO_INPUT)
GPIO(STD_ADPT_CNTRL_EC, 	PIN(C, 3), GPIO_INPUT)
GPIO(BATT_DISABLE_EC,		PIN(6, 6), GPIO_INPUT)
GPIO(ME_G3_TO_M3_EC,		PIN(E, 0), GPIO_INPUT)
GPIO(SLP_S0_CS_EC,		PIN(7, 4), GPIO_INPUT)
//GPIO(VCCST_PWRGD,		PIN(7, 1), GPIO_INPUT)
GPIO(USBC_TCPC_ALRT_P0,		PIN(4, 0), GPIO_INPUT)
GPIO(CPU_CAT_ERR_MECC,		PIN(3, 4), GPIO_INPUT)
GPIO(SMC_ONOFF_N,		PIN(D, 2), GPIO_INPUT) /* Power button interrupt without H1 */
GPIO(SMB_PCH_ALRT,		PIN(A, 3), GPIO_INPUT)
GPIO(ESPI_ALERT0_R,		PIN(5, 7), GPIO_INPUT | GPIO_SEL_1P8V)


/* Unused I2C */
GPIO(USBC_TCPC_I2C_CLK_P2,	PIN(9, 2), GPIO_INPUT)	/* I2C2_SCL */
GPIO(USBC_TCPC_I2C_DATA_P2,	PIN(9, 1), GPIO_INPUT)	/* I2C2_SDA */

GPIO(USBC_TCPC_I2C_CLK_P1,	PIN(9, 0), GPIO_INPUT)	/* I2C1_SCL */
GPIO(USBC_TCPC_I2C_DATA_P1,	PIN(8, 7), GPIO_INPUT)	/* I2C1_SDA */

GPIO(USBC_TCPC_I2C_CLK_P3,	PIN(D, 1), GPIO_INPUT)	/* I2C3_SCL */
GPIO(USBC_TCPC_I2C_DATA_P3,	PIN(D, 0), GPIO_INPUT)	/* I2C3_SDA */


ALTERNATE(PIN_MASK(6, 0x30), 0, MODULE_UART, 0)					/* UART from EC to Servo */
ALTERNATE(PIN_MASK(B, 0x30), 0, MODULE_I2C, 0)					/* I2C0 */
ALTERNATE(PIN_MASK(9, 0x07), 0, MODULE_I2C, 0)					/* I2C1 SCL / I2C2 */
ALTERNATE(PIN_MASK(8, 0x80), 0, MODULE_I2C, 0)					/* I2C1 SDA */
ALTERNATE(PIN_MASK(D, 0x03), 0, MODULE_I2C, 0)					/* I2C3 */
ALTERNATE(PIN_MASK(B, 0x0C), 0, MODULE_I2C, 0)					/* I2C7 */
ALTERNATE(PIN_MASK(4, 0x1E), 0, MODULE_ADC, 0)					/* ADC1-4 */
ALTERNATE(PIN_MASK(8, 0x01), 0, MODULE_PWM, 0)					/* CPU_FAN_MECC */
ALTERNATE(PIN_MASK(D, 0x08), 0, MODULE_PWM, 0)					/* CPU_TACHO_FAN (TB1) */

ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_INPUT)		/* KSI_00-01 */
GPIO(EC_KSO_02_INV, PIN(1, 7), GPIO_OUT_LOW)
ALTERNATE(PIN_MASK(2, 0xFC), 0, MODULE_KEYBOARD_SCAN, GPIO_INPUT)		/* KSI_02-07 */
ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_00-01 */
ALTERNATE(PIN_MASK(1, 0xFF), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_02-09 */
ALTERNATE(PIN_MASK(0, 0xF0), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_10-13 */
ALTERNATE(PIN_MASK(8, 0x0C), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		/* KSO_14-15 */
