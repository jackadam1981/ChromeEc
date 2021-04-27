/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel TGLY-MECC1.0-ITE board-specific configuration */

/*
 * MEC152X data sheets GPIO numbers are OCTAL.
 */

/* include common gpio.inc under chip/mchp/lfw/... */
#include "chip/mchp/lfw/gpio.inc"

/*
 * Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first.
 */

/* Power sequencing interrupts */
//// GPIO_INT(ALL_SYS_PWRGD_EC,	PIN(F, 0), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(ALL_SYS_PWRGD_EC,	PIN(057), GPIO_INT_BOTH, power_signal_interrupt)
//// GPIO_INT(RSMRST_PWRGD_EC,	PIN(F, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(RSMRST_PWRGD_EC,	PIN(0221), GPIO_INT_BOTH, power_signal_interrupt)
//// GPIO_INT(PCH_SLP_S0_N,		PIN(G, 6), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_S0_N,		PIN(0243), GPIO_INT_BOTH, power_signal_interrupt)

/* Change for TGL-Y RVP */
//GPIO_INT(VCCPDSW_3P3_EC,	PIN(I, 3), GPIO_INT_BOTH, power_signal_interrupt)
//// GPIO_INT(EDP_BKLT_EN,		PIN(J, 6), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(EDP_BKLT_EN,		PIN(022), GPIO_INT_BOTH, power_signal_interrupt)
//GPIO_INT(PM_SLP_SUS_EC, 	PIN(K, 2), GPIO_INT_BOTH, power_signal_interrupt)
//// GPIO_INT(SMC_SHUTDOWN,		PIN(K, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SMC_SHUTDOWN,		PIN(0255), GPIO_INT_BOTH, power_signal_interrupt)

#ifndef CONFIG_HOSTCMD_ESPI_VW_SLP_S3
//// GPIO_INT(SLP_S3_R_L,		PIN(F, 2), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S3_R_L,		PIN(0242), GPIO_INT_BOTH, power_signal_interrupt)
#endif
#ifndef CONFIG_HOSTCMD_ESPI_VW_SLP_S4
//// GPIO_INT(SLP_S4_R_L,		PIN(F, 3), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S4_R_L,		PIN(0241), GPIO_INT_BOTH, power_signal_interrupt)
#endif

/* Button interrupts */
//// GPIO_INT(VOLUME_UP,		PIN(D, 5), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(VOLUME_UP,		PIN(036), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
//// GPIO_INT(VOL_DN_EC_R,		PIN(D, 6), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(VOL_DN_EC_R,		PIN(0254), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
//// GPIO_INT(SMC_LID,		PIN(E, 2), GPIO_INT_BOTH | GPIO_PULL_UP, lid_interrupt)
GPIO_INT(SMC_LID,		PIN(0226), GPIO_INT_BOTH | GPIO_PULL_UP, lid_interrupt)
//// GPIO_INT(MECH_PWR_BTN_ODL,	PIN(E, 4), GPIO_INT_BOTH, power_button_interrupt)
GPIO_INT(MECH_PWR_BTN_ODL,	PIN(0163), GPIO_INT_BOTH, power_button_interrupt)
//// GPIO_INT(EC_WP_ODL,		PIN(I, 4), GPIO_INT_BOTH, switch_interrupt)
GPIO_INT(EC_WP_ODL,		PIN(014), GPIO_INT_BOTH, switch_interrupt)

/* DC Jack presence coming from +VADP_OUT */
//// GPIO_INT(STD_ADP_PRSNT, 	PIN(J, 2), GPIO_INT_BOTH, board_dc_jack_interrupt) /* DC_JACK_PRESENT */
GPIO_INT(STD_ADP_PRSNT, 	PIN(043), GPIO_INT_BOTH, board_dc_jack_interrupt) /* DC_JACK_PRESENT */

//// GPIO_INT(BC_ACOK_EC,		PIN(K, 3), GPIO_INT_BOTH, extpower_interrupt) /* AC Present */
GPIO_INT(BC_ACOK_EC,		PIN(0156), GPIO_INT_BOTH, extpower_interrupt) /* AC Present */

//// GPIO_INT(UART_SERVO_TX_EC_RX,	PIN(B, 0), GPIO_INT_FALLING, uart_deepsleep_interrupt)
GPIO_INT(UART_SERVO_TX_EC_RX,	PIN(0105), GPIO_INT_FALLING, uart_deepsleep_interrupt)

/* USB-C interrupts */

#ifndef CONFIG_HOSTCMD_ESPI
//// GPIO_INT(ESPI_RST_R,		PIN(D, 2), GPIO_INPUT)
GPIO_INT(ESPI_RST_R,		PIN(061), GPIO_INPUT)
#endif

#ifdef CONFIG_HOSTCMD_ESPI
/* enable 1.8v input of EC's espi_reset pin, and then this pin takes effect. */
//// GPIO_INT(ESPI_RST_R,		PIN(D, 2), GPIO_INT_FALLING | GPIO_SEL_1P8V, espi_reset_pin_asserted_interrupt) /* eSPI_reset# */
GPIO_INT(ESPI_RST_R,		PIN(061), GPIO_INT_FALLING, espi_reset_pin_asserted_interrupt) /* eSPI_reset# */
#endif

/* Sensor Interrupts */
//// GPIO_INT(SLATE_MODE_INDICATION,	PIN(K, 1), GPIO_INT_BOTH | GPIO_SEL_1P8V, gmr_tablet_switch_isr)
GPIO_INT(SLATE_MODE_INDICATION,	PIN(0222), GPIO_INT_BOTH | GPIO_SEL_1P8V, gmr_tablet_switch_isr)

/* Power sequencing GPIOs */
//// GPIO(SYS_RST_ODL_EC,		PIN(B, 6), GPIO_ODR_HIGH)
GPIO(SYS_RST_ODL_EC,		PIN(060), GPIO_ODR_HIGH)
//// GPIO(PROCHOT_EC,		PIN(C, 0), GPIO_INPUT)
GPIO(PROCHOT_EC,		PIN(002), GPIO_INPUT)
//// GPIO(PM_RSMRST_EC,		PIN(C, 6), GPIO_OUT_LOW)
GPIO(PM_RSMRST_EC,		PIN(054), GPIO_OUT_LOW)
//// GPIO(PM_PWRBTN_N_EC,		PIN(D, 0), GPIO_ODR_HIGH)
GPIO(PM_PWRBTN_N_EC,		PIN(0250), GPIO_ODR_HIGH)
//// GPIO(EC_SPI_OE_N,		PIN(I, 2), GPIO_OUT_LOW)
GPIO(EC_SPI_OE_N,		PIN(0253), GPIO_OUT_LOW)

//// GPIO(EC_PCH_MKBP_INT_ODL_EC,	PIN(B, 7), GPIO_ODR_HIGH)
GPIO(EC_PCH_MKBP_INT_ODL_EC,	PIN(0127), GPIO_ODR_HIGH)

/* Changes for TGL_Y RVP */
/* UNIMPLEMENTED(EN_PP5000) */
//GPIO(EDP_BKLT_EN,		PIN(J, 6), GPIO_OUT_HIGH)
//GPIO(EC_DS3,			PIN(L, 4), GPIO_OUT_LOW)
//GPIO(SMC_SHUTDOWN,		PIN(K, 5), GPIO_INPUT)
//// GPIO(EC_DS3,			PIN(L, 4), GPIO_OUT_LOW)
GPIO(EC_DS3,			PIN(0246), GPIO_OUT_LOW)
//// GPIO(PM_SLP_SUS_EC, 		PIN(K, 2), GPIO_OUT_LOW)
GPIO(PM_SLP_SUS_EC, 		PIN(0227), GPIO_OUT_LOW)

/*
 * PCH_SYS_PWROK is an input, driven by the Silego chip. The common x86
 * power sequencing expects that PCH_SYS_PWROK is an output and will drive
 * this signal if GPIO_PCH_SYS_PWROK is configured. Map this pin as no-connect
 * so that state can be monitored using the console.
 */
//// GPIO(PCH_PWROK_EC_R,	PIN(K, 4), GPIO_INPUT)
GPIO(PCH_PWROK_EC_R,	PIN(0106), GPIO_INPUT)
//// GPIO(SYS_PWROK_EC,	PIN(D, 1), GPIO_INPUT)
GPIO(SYS_PWROK_EC,	PIN(0202), GPIO_INPUT)
//// GPIO(DSW_PWROK_EC,	PIN(L, 6), GPIO_OUT_LOW)
GPIO(DSW_PWROK_EC,	PIN(034), GPIO_OUT_LOW)

/* Host communication GPIOs */
#ifndef CONFIG_HOSTCMD_ESPI
//// GPIO(PLT_RST_L, 	PIN(H, 6), GPIO_INPUT | GPIO_PULL_UP) /* PCH_PLTRST_L */
GPIO(PLT_RST_L, 	PIN(052), GPIO_INPUT | GPIO_PULL_UP) /* PCH_PLTRST_L */
#endif
//// GPIO(PCH_WAKE_N,	PIN(J, 0), GPIO_ODR_HIGH)
GPIO(PCH_WAKE_N,	PIN(051), GPIO_ODR_HIGH)

/* Battery present */
//// GPIO(BAT_DET_EC,	PIN(K, 0), GPIO_INPUT)
GPIO(BAT_DET_EC,	PIN(0206), GPIO_INPUT)

/* LED */
//// GPIO(LED_1_L_EC,	PIN(A, 6), GPIO_OUT_HIGH)	/* BAT_LED_GREEN_L LED_2_L */
GPIO(LED_1_L_EC,	PIN(0157), GPIO_OUT_HIGH)	/* BAT_LED_GREEN_L LED_2_L */
//// GPIO(LED_2_L_EC,	PIN(A, 7), GPIO_OUT_HIGH)	/* AC_LED_GREEN_L LED_1_L */
GPIO(LED_2_L_EC,	PIN(0153), GPIO_OUT_HIGH)	/* AC_LED_GREEN_L LED_1_L */

/* H1 pins */
//// GPIO(EC_H1_PACKET_MODE_EC,	PIN(B, 2), GPIO_OUT_LOW)
GPIO(EC_H1_PACKET_MODE_EC,	PIN(035), GPIO_OUT_LOW)
//// GPIO(EC_ENTERING_RW_EC,		PIN(K, 7), GPIO_OUT_LOW)
GPIO(EC_ENTERING_RW_EC,		PIN(0102), GPIO_OUT_LOW)

/* Case Closed Debug Mode */
UNIMPLEMENTED(CCD_MODE_ODL)
//// GPIO(NC_CCD_MODE_ODL,	PIN(B, 5), GPIO_INPUT)
GPIO(NC_CCD_MODE_ODL,	PIN(0175), GPIO_INPUT)

/* FAN control pins */
//// GPIO(EC_THRM_SEN_PWRGATE_N,	PIN(K, 6), GPIO_OUT_LOW)
GPIO(EC_THRM_SEN_PWRGATE_N,	PIN(0141), GPIO_OUT_LOW)

/*
 * I2C pins should be configure as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
 /* MEC1521 doesn't support flash update via I2C */
//// GPIO(EC_I2C_PROG_SCL,		PIN(B, 3), GPIO_INPUT)	/* I2C_A_SCL */
//// GPIO(EC_I2C_PROG_SDA,		PIN(B, 4), GPIO_INPUT)	/* I2C_A_SDA */
//// GPIO(SMB_BS_CLK,		PIN(C, 1), GPIO_INPUT)	/* I2C_B_SCL */
GPIO(SMB_BS_CLK,		PIN(004), GPIO_INPUT)	/* I2C_B_SCL */
//// GPIO(SMB_BS_DATA,		PIN(C, 2), GPIO_INPUT)	/* I2C_B_SDA */
GPIO(SMB_BS_DATA,		PIN(003), GPIO_INPUT)	/* I2C_B_SDA */

/* P0 IT83XX_I2C_CH_C */
//// GPIO(USBC_TCPC_I2C_CLK_P0,	PIN(C, 7), GPIO_INPUT)	/* I2C_C_SCL */
GPIO(USBC_TCPC_I2C_CLK_P0,	PIN(0140), GPIO_INPUT)	/* I2C_C_SCL */
//// GPIO(USBC_TCPC_I2C_DATA_P0,	PIN(F, 7), GPIO_INPUT)	/* I2C_C_SDA */
GPIO(USBC_TCPC_I2C_DATA_P0,	PIN(0132), GPIO_INPUT)	/* I2C_C_SDA */

/* BB retimer controls */
//// GPIO(USBC_TCPC_PPC_ALRT_P1, 	PIN(C, 4), GPIO_OUT_LOW) /* P0 - Load switch enable */
GPIO(USBC_TCPC_PPC_ALRT_P1, 	PIN(0101), GPIO_OUT_LOW) /* P0 - Load switch enable */
//// GPIO(USBC_TCPC_ALRT_P3,		PIN(J, 3), GPIO_OUT_LOW) /* P0 - Retimer reset */
GPIO(USBC_TCPC_ALRT_P3,		PIN(0161), GPIO_OUT_LOW) /* P0 - Retimer reset */
//// GPIO(USBC_TCPC_ALRT_P2,		PIN(J, 1), GPIO_OUT_LOW) /* P1 - Load switch enable */
GPIO(USBC_TCPC_ALRT_P2,		PIN(0172), GPIO_OUT_LOW) /* P1 - Load switch enable */
//// GPIO(USBC_TCPC_PPC_ALRT_P0, 	PIN(J, 5), GPIO_OUT_LOW) /* P1 - Retimer reset */
GPIO(USBC_TCPC_PPC_ALRT_P0, 	PIN(0240), GPIO_OUT_LOW) /* P1 - Retimer reset */

/* Unused 1.8V or 3.3V compatiable pins */
//// GPIO(ESPI_CS1_N_R,		PIN(D, 3), GPIO_INPUT)
GPIO(ESPI_CS1_N_R,		PIN(011), GPIO_INPUT)
//// GPIO(TP_GPIO_D4,		PIN(D, 4), GPIO_INPUT)
GPIO(TP_GPIO_D4,		PIN(0223), GPIO_INPUT)
//// GPIO(SUSWARN,			PIN(E, 1), GPIO_INPUT)
GPIO(SUSWARN,			PIN(024), GPIO_INPUT)
//// GPIO(USBC_TCPC_PPC_ALRT_P2, 	PIN(E, 5), GPIO_INPUT)
GPIO(USBC_TCPC_PPC_ALRT_P2, 	PIN(0144), GPIO_INPUT)
//// GPIO(USBC_TCPC_PPC_ALRT_P3, 	PIN(E, 6), GPIO_INPUT)
GPIO(USBC_TCPC_PPC_ALRT_P3, 	PIN(0142), GPIO_INPUT)
//// GPIO(USBC_TCPC_ALRT_P1,		PIN(G, 0), GPIO_INPUT)
GPIO(USBC_TCPC_ALRT_P1,		PIN(0162), GPIO_INPUT)
//// GPIO(CPU_C10_GATE,		PIN(G, 1), GPIO_INPUT)
GPIO(CPU_C10_GATE,		PIN(023), GPIO_INPUT)
//// GPIO(STD_ADPT_CNTRL_EC, 	PIN(G, 2), GPIO_INPUT)
GPIO(STD_ADPT_CNTRL_EC, 	PIN(0224), GPIO_INPUT)
//// GPIO(BATT_DISABLE_EC,		PIN(H, 0), GPIO_INPUT)
GPIO(BATT_DISABLE_EC,		PIN(067), GPIO_INPUT)
//// GPIO(ME_G3_TO_M3_EC,		PIN(H, 5), GPIO_INPUT)
GPIO(ME_G3_TO_M3_EC,		PIN(033), GPIO_INPUT)
//// GPIO(SLP_S0_CS_EC,		PIN(I, 0), GPIO_INPUT)
GPIO(SLP_S0_CS_EC,		PIN(0100), GPIO_INPUT)
//// GPIO(VCCST_PWRGD,		PIN(I, 5), GPIO_INPUT)
GPIO(VCCST_PWRGD,		PIN(0207), GPIO_INPUT)
//// GPIO(USBC_TCPC_ALRT_P0,		PIN(I, 7), GPIO_INPUT)
GPIO(USBC_TCPC_ALRT_P0,		PIN(0143), GPIO_INPUT)
//// GPIO(TP_DEVELOPER_MODE_EC,	PIN(J, 4), GPIO_INPUT)
/* GPIO(TP_DEVELOPER_MODE_EC,	PIN(0224), GPIO_INPUT) not used */
//// GPIO(CPU_CAT_ERR_MECC,		PIN(L, 1), GPIO_INPUT)
GPIO(CPU_CAT_ERR_MECC,		PIN(000), GPIO_INPUT)
//// GPIO(SMC_ONOFF_N,		PIN(L, 3), GPIO_INPUT) /* Power button interrupt without H1 */
/* GPIO(SMC_ONOFF_N,		PIN(0163), GPIO_INPUT) not used */ /* Power button interrupt without H1 */
//// GPIO(TP_GPIO_L5,		PIN(L, 5), GPIO_INPUT)
GPIO(TP_GPIO_L5,		PIN(0244), GPIO_INPUT)
//// GPIO(SMB_PCH_ALRT,		PIN(L, 7), GPIO_INPUT)
GPIO(SMB_PCH_ALRT,		PIN(025), GPIO_INPUT)
//// GPIO(ESPI_ALERT0_R,		PIN(M, 6), GPIO_INPUT | GPIO_SEL_1P8V)
GPIO(ESPI_ALERT0_R,		PIN(063), GPIO_INPUT)

/* Unused 3.3V compatiable pins */
//// GPIO(TP_GPIO_A0,		PIN(A, 0), GPIO_INPUT)
//// GPIO(TP_GPIO_A1,		PIN(A, 1), GPIO_INPUT)
//// GPIO(TP_GPIO_A3,		PIN(A, 3), GPIO_INPUT)
//// GPIO(TP_GPC3,			PIN(C, 3), GPIO_INPUT)
//// GPIO(TP_GPC5,			PIN(C, 5), GPIO_INPUT)
//// GPIO(TP_GPIO_E3,		PIN(E, 3), GPIO_INPUT)
//// GPIO(TP_GPIO_H3,		PIN(H, 3), GPIO_INPUT)
//// GPIO(TP_GPIO_H4,		PIN(H, 4), GPIO_INPUT)

/* Unused I2C */
//// GPIO(USBC_TCPC_I2C_CLK_P2,	PIN(A, 4), GPIO_INPUT)	/* I2C_F_SCL */
GPIO(USBC_TCPC_I2C_CLK_P2,	PIN(007), GPIO_INPUT)	/* I2C03 */
//// GPIO(USBC_TCPC_I2C_DATA_P2,	PIN(A, 5), GPIO_INPUT)	/* I2C_F_SDA */
GPIO(USBC_TCPC_I2C_DATA_P2,	PIN(010), GPIO_INPUT)	/* I2C03 */
//// GPIO(USBC_TCPC_I2C_CLK_P1,	PIN(E, 0), GPIO_INPUT)	/* I2C_E_SCL */
GPIO(USBC_TCPC_I2C_CLK_P1,	PIN(013), GPIO_INPUT)	/* I2C07 */
//// GPIO(USBC_TCPC_I2C_DATA_P1,	PIN(E, 7), GPIO_INPUT)	/* I2C_E_SDA */
GPIO(USBC_TCPC_I2C_DATA_P1,	PIN(012), GPIO_INPUT)	/* I2C07 */
//// GPIO(USBC_TCPC_I2C_CLK_P3,	PIN(H, 1), GPIO_INPUT)	/* I2C_D_SCL */
GPIO(USBC_TCPC_I2C_CLK_P3,	PIN(0154), GPIO_INPUT)	/* I2C02 */
//// GPIO(USBC_TCPC_I2C_DATA_P3,	PIN(H, 2), GPIO_INPUT)	/* I2C_D_SDA */
GPIO(USBC_TCPC_I2C_DATA_P3,	PIN(0155), GPIO_INPUT)	/* I2C02 */

/* Alternate pins for I2C */
//// ALTERNATE(PIN_MASK(B, BIT(3) | BIT(4)), GPIO_ALT_FUNC_DEFAULT, MODULE_I2C, GPIO_FLAG_NONE)	/* I2C A SCL/SDA B3/B4 */
//// ALTERNATE(PIN_MASK(C, BIT(1) | BIT(2)), GPIO_ALT_FUNC_DEFAULT, MODULE_I2C, GPIO_FLAG_NONE)	/* I2C B SCL/SDA C1/C2 */
/* Smart Batt: I2C00: GPIO004 / GPIO003
 * I2C00_SDA = GPIO_0003(Bank=0, bit=3) Func1
 * I2C00_SCL = GPIO_0004(Bank=0, bit=4) Func1
 */
ALTERNATE(PIN_MASK(0,  0x00000018), 1, MODULE_I2C, GPIO_ODR_HIGH)
//// ALTERNATE(PIN_MASK(C, BIT(7)), GPIO_ALT_FUNC_DEFAULT, MODULE_I2C, GPIO_FLAG_NONE)		/* I2C C SCL C7 */
//// ALTERNATE(PIN_MASK(F, BIT(7)), GPIO_ALT_FUNC_DEFAULT, MODULE_I2C, GPIO_FLAG_NONE)		/* I2C C SDA F7 */
/* USBC TCPC 0: I2C06: GPIO140 / GPIO132 
 * I2C06_SDA = GPIO_0132(Bank=2, bit=26) Func1
 * I2C06_SCL = GPIO_0140(Bank=3, bit=0) Func1
 */
ALTERNATE(PIN_MASK(2,  0x04000000), 1, MODULE_I2C, GPIO_ODR_HIGH)
ALTERNATE(PIN_MASK(3,  0x00000001), 1, MODULE_I2C, GPIO_ODR_HIGH)

/* Alternate pins for UART */
//// ALTERNATE(PIN_MASK(B, BIT(0) | BIT(1)), GPIO_ALT_FUNC_DEFAULT, MODULE_UART, GPIO_PULL_UP) 	/* UART1 B0/B1 */
/* done in lfw/gpio.inc */

/* Alternate pins for ADC */
//// ALTERNATE(PIN_MASK(I, BIT(1) | BIT(6)), GPIO_ALT_FUNC_DEFAULT, MODULE_ADC, GPIO_FLAG_NONE)	/* ADC 1,6 -> I1,I6 */
//// ALTERNATE(PIN_MASK(L, BIT(0) | BIT(2)), GPIO_ALT_FUNC_DEFAULT, MODULE_ADC, GPIO_FLAG_NONE)	/* ADC 13,15 -> L0,L2 */
/* GPI1: GPIO200/ADC00; GPI6: GPIO204/ADC04 */
/* GPL0: GPIO203/ADC03; GPL2: GPIO205/ADC05 */
/* ADC00 = GPIO_0200(Bank=4, bit=0) Func1
 * ADC03 = GPIO_0203(Bank=4, bit=3) Func1
 * ADC04 = GPIO_0204(Bank=4, bit=4) Func1
 * ADC05 = GPIO_0205(Bank=4, bit=5) Func1
 */
ALTERNATE(PIN_MASK(4,  0x00000039), 1, MODULE_ADC, GPIO_ANALOG)

/* Alternate pins for FAN */
//// ALTERNATE(PIN_MASK(A, BIT(2)), GPIO_ALT_FUNC_DEFAULT, MODULE_PWM, GPIO_FLAG_NONE)		/* PWM2 A2 */
/* GPA2: GPIO053/PWM0 */
GPIO(MCHP_TEST_FAN0, PIN(053), GPIO_OUT_LOW)
//// ALTERNATE(PIN_MASK(D, BIT(7)), GPIO_ALT_FUNC_DEFAULT, MODULE_PWM, GPIO_FLAG_NONE)		/* TACH1A D7 */
/* GPD7: GPIO050/ICT0_TACH0 */
GPIO(MCHP_TEST_TACH0, PIN(050), GPIO_INPUT | GPIO_PULL_UP)
/* MEC1521 doesn't support hw RPM-PWM */

/*
 * ESPI_RESET# - GPIO_0061 Func 1, Bank 1 bit[17]
 * ESPI_ALERT# - GPIO_0063 Func 1, Bank 1 bit[19]
 * ESPI_CLK    - GPIO_0065 Func 1, Bank 1 bit[21]
 * ESPI_CS#    - GPIO_0066 Func 1, Bank 1 bit[22]
 * ESPI_IO0    - GPIO_0070 Func 1, Bank 1 bit[24]
 * ESPI_IO1    - GPIO_0071 Func 1, Bank 1 bit[25]
 * ESPI_IO2    - GPIO_0072 Func 1, Bank 1 bit[26]
 * ESPI_IO3    - GPIO_0073 Func 1, Bank 1 bit[27]
 */
ALTERNATE(PIN_MASK(1, 0x0F6A0000), 1, MODULE_LPC, 0)

/* KB pins */
/*
 * MEC1521H-SZ (144 pin package)
 * KSO00 = GPIO_0040 Func2 bank 1 bit 0
 * KSO01 = GPIO_0045 Func1 bank 1 bit 5
 * KSO02 = GPIO_0046 Func1 bank 1 bit 6
 * KSO03 = GPIO_0047 Func1 bank 1 bit 7
 * KSO04 = GPIO_0107 Func2 bank 2 bit 7
 * KSO05 = GPIO_0112 Func1 bank 2 bit 10
 * KSO06 = GPIO_0113 Func1 bank 2 bit 11
 * KSO07 = GPIO_0120 Func1 bank 2 bit 16
 * KSO08 = GPIO_0121 Func2 bank 2 bit 17
 * KSO09 = GPIO_0122 Func2 bank 2 bit 18
 * KSO10 = GPIO_0123 Func2 bank 2 bit 19
 * KSO11 = GPIO_0124 Func2 bank 2 bit 20
 * KSO12 = GPIO_0125 Func2 bank 2 bit 21
 * For 8x16 test keyboard add KSO13 - KSO15
 * KSO13 = GPIO_0126 Func2 bank 2 bit 22
 * KSO14 = GPIO_0152 Func1 bank 3 bit 10
 * KSO15 = GPIO_0151 Func2 bank 3 bit 9
 *
 * KSI0 = GPIO_0017 Func1 bank 0 bit 15
 * KSI1 = GPIO_0020 Func1 bank 0 bit 16
 * KSI2 = GPIO_0021 Func1 bank 0 bit 17
 * KSI3 = GPIO_0026 Func1 bank 0 bit 22
 * KSI4 = GPIO_0027 Func1 bank 0 bit 23
 * KSI5 = GPIO_0030 Func1 bank 0 bit 24
 * KSI6 = GPIO_0031 Func1 bank 0 bit 25
 * KSI7 = GPIO_0032 Func1 bank 0 bit 26
 */
/* KSO 0 Bank 1, Func2, bit 0 */
ALTERNATE(PIN_MASK(1, 0x01), 2, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)

#ifdef CONFIG_KEYBOARD_COL2_INVERTED
/* KSO 1-3 Bank 1, Func1, bits 5-7 */
#define GPIO_KB_OUTPUT_COL2 (GPIO_OUT_LOW)
ALTERNATE(PIN_MASK(1, 0xA0), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
GPIO(KBD_KSO2, PIN(046), GPIO_KB_OUTPUT_COL2)
#else
/* KSO 1-3 Bank 1, Func1, bits 5-7 */
ALTERNATE(PIN_MASK(1, 0xE0), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
#endif

/* KSO 4, 8-12 Bank 2, Func2, bits 7, 17-21 */
ALTERNATE(PIN_MASK(2, 0x003E0080), 2, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* KSO 5-7, Bank 2, Func1, bits 10-11, 16 */
ALTERNATE(PIN_MASK(2, 0x00010C00), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)

/* KSI 0-7, Bank 0, Func1, bit 15-17, 22-26 */
ALTERNATE(PIN_MASK(0, 0x07C38000), 1, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
