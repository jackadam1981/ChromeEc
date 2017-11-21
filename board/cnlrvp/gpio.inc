/* -*- mode:c -*-
 *
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel CNL-RVP board-specific configuration */

/*
 * Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first.
 */

/* Power sequencing interrupts */
GPIO_INT(RSMRST_L_PGOOD,PIN(3, 6), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PMIC_DPWROK,	PIN(6, 0), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(ALL_SYS_PWRGD,	PIN(7, 2), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_S0_L,	PIN(8, 1), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_SUS_L,	PIN(7, 3), GPIO_INT_BOTH, power_signal_interrupt)
#ifndef CONFIG_ESPI_VW_SIGNALS
GPIO_INT(PCH_SLP_S3_L,	PIN(8, 5), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_S4_L,	PIN(8, 6), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(PCH_SLP_S5_L,	PIN(0, 4), GPIO_INT_BOTH, power_signal_interrupt)
#endif

/* Button interrupts */
GPIO_INT(POWER_BUTTON_L,	PIN(A, 6), GPIO_INT_BOTH, power_button_interrupt)
GPIO_INT(LID_OPEN,		PIN(0, 3), GPIO_INT_BOTH | GPIO_PULL_UP, lid_interrupt)
GPIO_INT(EC_VOLUP_BTN_ODL,	PIN(3, 4), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)
GPIO_INT(EC_VOLDN_BTN_ODL,	PIN(3, 7), GPIO_INT_BOTH | GPIO_PULL_UP, button_interrupt)

/* Type-C interrupts */
GPIO_INT(USB_C0_PD_INT_ODL,	PIN(6, 2), GPIO_INT_FALLING, tcpc_alert_event)
GPIO_INT(USB_C1_PD_INT_ODL,	PIN(6, 3), GPIO_INT_FALLING, tcpc_alert_event)

/* AC presense */
GPIO_INT(AC_PRESENT,	PIN(D, 2), GPIO_INT_BOTH, extpower_interrupt)

/* Write protect */
GPIO_INT(WP_L,		PIN(9, 3), GPIO_INT_BOTH, switch_interrupt)

/* GPIO_66 is non-interruptable hence re-routed to GPIO_73 */
GPIO(SLP_SUS_L,		PIN(6, 6), GPIO_INPUT)
/* Power sequencing GPIOs */
GPIO(PCH_RSMRST_L,	PIN(0, 1), GPIO_OUT_LOW)
GPIO(SYS_RESET_L,	PIN(0, 0), GPIO_ODR_HIGH)
GPIO(PCH_PWRBTN_L,	PIN(7, 5), GPIO_ODR_HIGH)
GPIO(EC_DS4,		PIN(6, 1), GPIO_INPUT)	/* DS4 */
GPIO(SMC_SHUTDOWN,	PIN(3, 5), GPIO_OUT_LOW | GPIO_PULL_DOWN)
GPIO(EC_SPI_OE_N,	PIN(A, 5), GPIO_OUT_HIGH)
UNIMPLEMENTED(PCH_DSW_PWROK)	/* handled by SLG7NT4192VTR on CNLRVP */
UNIMPLEMENTED(EN_PP5000)	/* handled by SLG7NT402V on CNLRVP */
UNIMPLEMENTED(EN_PP3300_DSW)

/*
 * PCH_PROCHOT_ODL is primarily for monitoring the PROCHOT# signal which is
 * normally driven by the PMIC. The EC can also drive this signal in the event
 * that the ambient or charger temperature sensors exceeds their thresholds.
 */
GPIO(CPU_PROCHOT,	PIN(A, 3), GPIO_INPUT)		/* PCH_PROCHOT_ODL */

/* Host communication GPIOs */
GPIO(PCH_WAKE_L,	PIN(C, 1), GPIO_ODR_HIGH)

GPIO(DC_JACK_PRESENT,	PIN(7, 0), GPIO_INPUT)		/* DC Jack presence coming from +VADP_OUT */
GPIO(USBC_LDO_ENABLE,	PIN(7, 1), GPIO_OUT_LOW)	/* USB TCPC to enable LDO in dead battery */
GPIO(ENABLE_BACKLIGHT,	PIN(9, 7), GPIO_ODR_HIGH)
GPIO(ENTERING_RW,	PIN(A, 7), GPIO_OUTPUT)		/* EC_ENTERING_RW */
GPIO(EC_BATT_PRES_L,	PIN(C, 7), GPIO_INPUT)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C0_SCL,	PIN(B, 5), GPIO_ODR_HIGH)
GPIO(I2C0_SDA,	PIN(B, 4), GPIO_ODR_HIGH)
GPIO(I2C1_SCL,	PIN(9, 0), GPIO_ODR_HIGH)
GPIO(I2C1_SDA,	PIN(8, 7), GPIO_ODR_HIGH)
GPIO(I2C2_SCL,	PIN(9, 2), GPIO_ODR_HIGH)
GPIO(I2C2_SDA,	PIN(9, 1), GPIO_ODR_HIGH)
GPIO(I2C3_SCL,	PIN(D, 1), GPIO_ODR_HIGH)
GPIO(I2C3_SDA,	PIN(D, 0), GPIO_ODR_HIGH)
GPIO(I2C7_SCL,	PIN(B, 3), GPIO_ODR_HIGH)
GPIO(I2C7_SDA,	PIN(B, 2), GPIO_ODR_HIGH)

#if 0
/* LPC / eSPI signals */
GPIO(LAD0_eSPI_IO0,	PIN(4, 6), GPIO_INPUT)	/* LAD0 / eSPI_IO0 */
GPIO(LAD1_eSPI_IO1,	PIN(4, 7), GPIO_INPUT)	/* LAD1 / eSPI_IO1 */
GPIO(LAD2_eSPI_IO2,	PIN(5, 1), GPIO_INPUT)	/* LAD2 / eSPI_IO2 */
GPIO(LAD3_eSPI_IO3,	PIN(5, 2), GPIO_INPUT)	/* LAD3 / eSPI_IO3 */
GPIO(LFRAME_eSPI_CS,	PIN(5, 3), GPIO_INPUT)	/* LFRAME / eSPI_CS */
GPIO(LRESET_eSPI_RST,	PIN(5, 4), GPIO_INPUT)	/* LRESET / eSPI_RST */
GPIO(PCI_CLK_eSPI_CLK,	PIN(5, 5), GPIO_INPUT)	/* PCI_CLK / eSPI_CLK */
GPIO(CLKRUN,		PIN(5, 6), GPIO_INPUT)	/* CLKRUN */
GPIO(SER_IRQ_eSPI_ALERT,PIN(5, 7), GPIO_INPUT)	/* SER_IRQ / eSPI_ALERT */
GPIO(PCH_SCI_L,		PIN(7, 6), GPIO_ODR_HIGH)/* EC_SCI_ODL */
GPIO(ESPI_ALERT1B,	PIN(C, 5), GPIO_INPUT)	/* H_RCIN_N / eSPI_ALERT1B */
GPIO(PCH_SMI_L,		PIN(C, 6), GPIO_ODR_HIGH)/* EC_SMI_ODL */

/* Strap settings */
GPIO(TRIS,		PIN(3, 2), GPIO_INPUT)
GPIO(TEST,		PIN(3, 5), GPIO_INPUT)

GPIO(NC_E7,	PIN(E, 7), GPIO_INPUT | GPIO_PULL_UP)	/* 32K_CLKIN */
#endif

/* Unused pins 3.3V & Interruptable */
GPIO(NC_02,	PIN(0, 2), GPIO_INPUT | GPIO_PULL_UP)

GPIO(NC_40,	PIN(4, 0), GPIO_INPUT)	/* TA1_TACH1 */
GPIO(NC_41,	PIN(4, 1), GPIO_INPUT | GPIO_PULL_UP)	/* ADC4 */
GPIO(NC_42,	PIN(4, 2), GPIO_INPUT | GPIO_PULL_UP)	/* ADC3 */
GPIO(NC_43,	PIN(4, 3), GPIO_INPUT | GPIO_PULL_UP)	/* ADC2 */
GPIO(NC_44,	PIN(4, 4), GPIO_INPUT | GPIO_PULL_UP)	/* ADC1 */
GPIO(NC_45,	PIN(4, 5), GPIO_INPUT | GPIO_PULL_UP)	/* ADC5 */

GPIO(NC_67,	PIN(6, 7), GPIO_INPUT)	/* Wake-up button */

GPIO(NC_74,	PIN(7, 4), GPIO_INPUT | GPIO_PULL_UP)

GPIO(NC_80,	PIN(8, 0), GPIO_INPUT)	/* PWM3 */
GPIO(NC_82,	PIN(8, 2), GPIO_INPUT | GPIO_PULL_UP)
GPIO(NC_83,	PIN(8, 3), GPIO_INPUT | GPIO_PULL_UP)

GPIO(NC_B1,	PIN(B, 1), GPIO_INPUT | GPIO_PULL_UP)
GPIO(NC_B7,	PIN(B, 7), GPIO_INPUT)	/* PWM5 */

GPIO(NC_C0,	PIN(C, 0), GPIO_INPUT | GPIO_PULL_UP)	/* PWM6 */
GPIO(NC_C2,	PIN(C, 2), GPIO_INPUT)	/* PWM_FAN */
GPIO(NC_C3,	PIN(C, 3), GPIO_INPUT)	/* PWM0 */
GPIO(NC_C4,	PIN(C, 4), GPIO_INPUT)	/* PWM2 */

GPIO(NC_D3,	PIN(D, 3), GPIO_INPUT)	/* TACHO_FAN */

/* Unused pins: VSPI 3.3V or 1.8V & Interruptable */
GPIO(NC_94,	PIN(9, 4), GPIO_INPUT | GPIO_PULL_UP)
GPIO(NC_95,	PIN(9, 5), GPIO_INPUT | GPIO_PULL_UP)

GPIO(NC_A1,	PIN(A, 1), GPIO_INPUT | GPIO_PULL_UP)

GPIO(NC_B0,	PIN(B, 0), GPIO_INPUT | GPIO_PULL_UP)

/* Unused pins 3.3V & Non-Interruptable */
GPIO(NC_B6,	PIN(B, 6), GPIO_INPUT | GPIO_PULL_UP)	/* PWM4 */

/* eSPI: VHIF Unused pins 1.8V & Interruptable */
GPIO(NC_50,	PIN(5, 0), GPIO_INPUT | GPIO_PULL_UP)

/* Alternate pins for UART */
ALTERNATE(PIN_MASK(6, 0x30), 1, MODULE_UART, 0)	/* GPIO64/65    */

/* Alternate pins for I2C */
ALTERNATE(PIN_MASK(8, 0x80), 1, MODULE_I2C, 0)	/* I2C1:SDA GPIO87 */
ALTERNATE(PIN_MASK(9, 0x01), 1, MODULE_I2C, 0)	/* I2C1:SCL GPIO90 */
ALTERNATE(PIN_MASK(9, 0x06), 1, MODULE_I2C, 0)	/* I2C2:SDA/SCL GPIO91/92 */
ALTERNATE(PIN_MASK(B, 0x0C), 1, MODULE_I2C, 0)	/* I2C7:SDA/SCL GPIOB2/B3 */
ALTERNATE(PIN_MASK(B, 0x30), 1, MODULE_I2C, 0)	/* I2C0:SDA/SCL GPIOB4/B5 */
ALTERNATE(PIN_MASK(D, 0x03), 1, MODULE_I2C, 0)	/* I2C3:SDA/SCL GPIOD0/D1 */

/* Keyboard pins */
#define GPIO_KB_INPUT (GPIO_INPUT)
#define GPIO_KB_OUTPUT (GPIO_ODR_HIGH)

/* Keyboard Columns */
/* GPIO05/06/07 */
ALTERNATE(PIN_MASK(0, 0xE0), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* GPIO10/11/12/13/14/15/16/17 */
ALTERNATE(PIN_MASK(1, 0xFF), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
/* GPIO20/21 */
ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)

/* Keyboard Rows */
/* GPIO22/23/24/25/26/27 */
ALTERNATE(PIN_MASK(2, 0xFC), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
/* GPIO30/31 */
ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)

#if 0
/* EC SPI chip */
GPIO(F_CS0_EC_L,	PIN(A, 0), GPIO_OUT_HIGH | GPIO_PULL_UP)
ALTERNATE(PIN_MASK(A, 0x14), 1, MODULE_SPI, 0) /* SPI:MOSI/SCLK GPIOA4/A2 */
ALTERNATE(PIN_MASK(9, 0x40), 1, MODULE_SPI, 0) /* SPI:MISO GPIO96 */
#endif
