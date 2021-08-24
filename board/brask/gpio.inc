/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* INTERRUPT GPIOs: */
GPIO_INT(ACOK_OD,                    PIN(0, 0), GPIO_INT_BOTH, extpower_interrupt)
GPIO_INT(EC_PROCHOT_IN_L,            PIN(F, 0), GPIO_INT_BOTH, throttle_ap_prochot_input_interrupt)
GPIO_INT(EC_WP_ODL,                  PIN(A, 1), GPIO_INT_BOTH, switch_interrupt)
GPIO_INT(GSC_EC_PWR_BTN_ODL,         PIN(0, 1), GPIO_INT_BOTH, power_button_interrupt)
GPIO_INT(SEQ_EC_ALL_SYS_PG,          PIN(F, 4), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SEQ_EC_DSW_PWROK,           PIN(C, 7), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SEQ_EC_RSMRST_ODL,          PIN(E, 2), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S3_L,                   PIN(A, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_SUS_L,                  PIN(F, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SYS_SLP_S0IX_L,             PIN(D, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(USB_C0_C2_TCPC_INT_ODL,     PIN(E, 0), GPIO_INT_FALLING, tcpc_alert_event)
GPIO_INT(USB_C0_PPC_INT_ODL,         PIN(6, 2), GPIO_INT_FALLING, ppc_interrupt)
GPIO_INT(USB_C0_RT_INT_ODL,          PIN(B, 1), GPIO_INT_FALLING, retimer_interrupt)
GPIO_INT(USB_C1_PPC_INT_ODL,         PIN(F, 5), GPIO_INT_FALLING, ppc_interrupt)
GPIO_INT(USB_C1_TCPC_INT_ODL,        PIN(A, 2), GPIO_INT_FALLING, tcpc_alert_event)
GPIO_INT(USB_C2_PPC_INT_ODL,         PIN(7, 0), GPIO_INT_FALLING, ppc_interrupt)
GPIO_INT(USB_C2_RT_INT_ODL,          PIN(4, 1), GPIO_INT_FALLING, retimer_interrupt)

/* CCD */
GPIO(CCD_MODE_ODL,                   PIN(E, 5), GPIO_INPUT)

/* Security */
GPIO(EC_ENTERING_RW,                 PIN(0, 3), GPIO_OUT_LOW)
GPIO(EC_GSC_PACKET_MODE,             PIN(7, 5), GPIO_OUT_LOW)

/* Fan */
GPIO(EN_PP5000_FAN,                  PIN(6, 1), GPIO_OUT_HIGH)

/* ADC, need to check the usage */
GPIO(ANALOG_PPVAR_PWR_IN_IMON_EC,    PIN(4, 2), GPIO_INPUT)

/* Display */
GPIO(DP_CONN_OC_ODL,                 PIN(2, 5), GPIO_INPUT)
GPIO(HDMI_CONN_OC_ODL,               PIN(2, 4), GPIO_INPUT)

/* BarrelJack */
GPIO(EN_PPVAR_BJ_ADP_L,              PIN(0, 7), GPIO_INPUT)
GPIO(BJ_ADP_PRESENT_ODL,             PIN(8, 2), GPIO_INPUT)

/* Chipset PCH */
GPIO(EC_PCHHOT_ODL,                  PIN(7, 4), GPIO_INPUT)
GPIO(EC_PCH_INT_ODL,                 PIN(B, 0), GPIO_ODR_HIGH)
GPIO(EC_PCH_RSMRST_L,                PIN(A, 6), GPIO_OUT_LOW)
GPIO(EC_PCH_RTCRST,                  PIN(7, 6), GPIO_OUT_LOW)
GPIO(EC_PCH_SYS_PWROK,               PIN(3, 7), GPIO_OUT_LOW)
GPIO(EC_PCH_WAKE_ODL,                PIN(C, 0), GPIO_ODR_HIGH)
GPIO(EC_PROCHOT_ODL,                 PIN(6, 3), GPIO_ODR_HIGH)
GPIO(EN_S5_RAILS,                    PIN(B, 6), GPIO_OUT_LOW)
GPIO(PCH_PWROK,                      PIN(7, 2), GPIO_OUT_LOW)
GPIO(SYS_RST_ODL,                    PIN(C, 5), GPIO_ODR_HIGH)
GPIO(VCCST_PWRGD_OD,                 PIN(A, 4), GPIO_ODR_LOW)
GPIO(IMVP9_VRRDY_OD,                 PIN(6, 0), GPIO_INPUT)
GPIO(CPU_C10_GATE_L,                 PIN(6, 7), GPIO_INPUT)

/* Button */
GPIO(EC_PCH_PWR_BTN_ODL,             PIN(C, 1), GPIO_ODR_HIGH)
GPIO(EC_RECOVERY_BTN_OD,             PIN(2, 3), GPIO_INPUT)
GPIO(GSC_EC_RECOVERY_BTN_OD,         PIN(2, 2), GPIO_INPUT)

/* NFC */
/* TODO(b/194068530): Enable NFC */
GPIO(NFC_COIL_ACT_L,                 PIN(D, 4), GPIO_INPUT)
GPIO(NFC_LOW_POWER_MODE,             PIN(9, 5), GPIO_OUT_HIGH)
GPIO(NFC_CARD_DET_L,                 PIN(A, 3), GPIO_INPUT)

/* Wireless Charger */
/* TODO(b/191418683): Implement Qi Driver */
GPIO(EC_QI_PWR,                      PIN(D, 2), GPIO_OUT_HIGH)
GPIO(EC_I2C_QI_RESET_L,              PIN(9, 3), GPIO_OUT_HIGH)
GPIO(EC_I2C_QI_INT_ODL,              PIN(9, 6), GPIO_INPUT)

/* HDMI CEC */
/* TODO(b/197474873): Enable HDMI CEC */
GPIO(HDMI_CEC_IN,                    PIN(7, 3), GPIO_INPUT)
GPIO(HDMI_CEC_OUT,                   PIN(D, 3), GPIO_OUT_HIGH | GPIO_OPEN_DRAIN)
GPIO(HDMI_CEC_PULL_UP,               PIN(C, 2), GPIO_OUT_HIGH)

/* I2C SCL/SDA */
GPIO(EC_I2C_QI_SCL,                  PIN(3, 3), GPIO_INPUT)
GPIO(EC_I2C_QI_SDA,                  PIN(3, 6), GPIO_INPUT)
GPIO(EC_I2C_MISC_SCL_R,              PIN(B, 3), GPIO_INPUT)
GPIO(EC_I2C_MISC_SDA_R,              PIN(B, 2), GPIO_INPUT)
GPIO(EC_I2C_DP_SCL,                  PIN(B, 5), GPIO_INPUT)
GPIO(EC_I2C_DP_SDA,                  PIN(B, 4), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_C2_PPC_SCL,       PIN(9, 2), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_C2_PPC_SDA,       PIN(9, 1), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_C2_RT_SCL,        PIN(D, 1), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_C2_RT_SDA,        PIN(D, 0), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_C2_TCPC_SCL,      PIN(9, 0), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_C2_TCPC_SDA,      PIN(8, 7), GPIO_INPUT)
GPIO(EC_I2C_USB_C1_MIX_SCL,          PIN(E, 4), GPIO_INPUT)
GPIO(EC_I2C_USB_C1_MIX_SDA,          PIN(E, 3), GPIO_INPUT)
GPIO(EC_I2C_USB_C1_TCPC_SCL,         PIN(F, 3), GPIO_INPUT)
GPIO(EC_I2C_USB_C1_TCPC_SDA,         PIN(F, 2), GPIO_INPUT)

/* USBA */
GPIO(EN_PP5000_USBA,                 PIN(D, 7), GPIO_OUT_LOW)
GPIO(USB_A0_OC_ODL,                  PIN(3, 1), GPIO_INPUT)
GPIO(USB_A1_OC_ODL,                  PIN(3, 0), GPIO_INPUT)
GPIO(USB_A2_OC_ODL,                  PIN(2, 7), GPIO_INPUT)
GPIO(USB_A3_OC_ODL,                  PIN(2, 6), GPIO_INPUT)
GPIO(USB_A0_STATUS_L,                PIN(2, 1), GPIO_INPUT)
GPIO(USB_A1_STATUS_L,                PIN(2, 0), GPIO_INPUT)
GPIO(USB_A2_STATUS_L,                PIN(1, 7), GPIO_INPUT)
GPIO(USB_A3_STATUS_L,                PIN(1, 6), GPIO_INPUT)
GPIO(USB_A_LOW_PWR0_OD,              PIN(1, 5), GPIO_INPUT)
GPIO(USB_A_LOW_PWR1_OD,              PIN(1, 4), GPIO_INPUT)
GPIO(USB_A_LOW_PWR2_OD,              PIN(1, 1), GPIO_INPUT)
GPIO(USB_A_LOW_PWR3_OD,              PIN(1, 0), GPIO_INPUT)

/* LED */
/* TODO(b/197471359): LED implementation */
GPIO(LED_GREEN_L,                    PIN(C, 3), GPIO_OUT_LOW)
GPIO(LED_RED_L,                      PIN(C, 4), GPIO_OUT_LOW)

/* TODO(b/197484336): Need to check how to enable it */
GPIO(USB_C0_BC12_INT_ODL,            PIN(C, 6), GPIO_INPUT)
GPIO(USB_C1_BC12_INT_ODL,            PIN(5, 0), GPIO_INPUT)
GPIO(USB_C2_BC12_INT_ODL,            PIN(8, 3), GPIO_INPUT)

/* USBC */
GPIO(USB_C0_C2_TCPC_RST_ODL,         PIN(A, 7), GPIO_ODR_LOW)
GPIO(USB_C1_FRS_EN,                  PIN(9, 4), GPIO_OUT_LOW)
GPIO(USB_C1_RT_INT_ODL,              PIN(A, 0), GPIO_INPUT)
GPIO(USB_C1_RT_RST_R_ODL,            PIN(0, 2), GPIO_ODR_LOW)

/* GPIO02_P2 to PU */
/* GPIO03_P2 to PU */
IOEX(USB_C0_OC_ODL,          EXPIN(IOEX_C0_NCT38XX, 0, 4), GPIO_ODR_HIGH)
IOEX(USB_C0_FRS_EN,          EXPIN(IOEX_C0_NCT38XX, 0, 6), GPIO_LOW)
IOEX(USB_C0_RT_RST_ODL,      EXPIN(IOEX_C0_NCT38XX, 0, 7), GPIO_ODR_LOW)

IOEX(USB_C2_RT_RST_ODL,      EXPIN(IOEX_C2_NCT38XX, 0, 2), GPIO_ODR_LOW)
IOEX(USB_C1_OC_ODL,          EXPIN(IOEX_C2_NCT38XX, 0, 3), GPIO_ODR_HIGH)
IOEX(USB_C2_OC_ODL,          EXPIN(IOEX_C2_NCT38XX, 0, 4), GPIO_ODR_HIGH)
IOEX(USB_C2_FRS_EN,          EXPIN(IOEX_C2_NCT38XX, 0, 6), GPIO_LOW)
/* GPIO07_P2 to PU */
