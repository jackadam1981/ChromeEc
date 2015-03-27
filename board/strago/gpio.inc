/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIOMEC(POWER_BUTTON_L,    35, GPIO_INT_BOTH_DSLEEP,                      power_button_interrupt) /* Power button */
GPIOMEC(LID_OPEN,          27, GPIO_INT_BOTH_DSLEEP,                      lid_interrupt)          /* Lid switch */
GPIOMEC(PCH_PWRBTN_L,      160, GPIO_OUT_HIGH,                             NULL)                   /* Power button output to PCH */
GPIOMEC(STARTUP_LATCH_SET, 46, GPIO_OUT_HIGH,                             NULL)                   /* To enable power button detection */

GPIOMEC(AC_PRESENT,        30, GPIO_INT_BOTH_DSLEEP,                      extpower_interrupt)     /* BC_ACOK / EC_ACIN - to know if battery or AC connected */
GPIOMEC(RSMRST_L_PGOOD,    63, GPIO_INT_BOTH,                             power_signal_interrupt) /* RSMRST_N_PWRGD from power logic */
GPIOMEC(PCH_RSMRST_L,      143, GPIO_OUT_LOW,                              NULL)                   /* RSMRST_N to PCH */
GPIOMEC(PCH_SLP_S4_L,      200, GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S4# signal from PCH */
GPIOMEC(PCH_SLP_S3_L,      206, GPIO_INT_BOTH_DSLEEP | GPIO_PULL_UP,       power_signal_interrupt) /* SLP_S3# signal from PCH */
GPIOMEC(ALL_SYS_PGOOD,     130, GPIO_INT_BOTH_DSLEEP,                      power_signal_interrupt) /* ALL_SYS_PWRGD from power logic */
GPIOMEC(PCH_SYS_PWROK,     65, GPIO_OUT_LOW,                              NULL)                   /* EC thinks everything is up and ready (DELAY_ALL_SYS_PWRGD) */

GPIOMEC(USB3_PWR_EN,       57, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for USB3 Port */
GPIOMEC(USB2_PWR_EN,       36, GPIO_OUT_HIGH,                             NULL)                   /* Enable power for USB2 Port */
GPIOMEC(USB_CTL1,          105, GPIO_OUT_HIGH,                             NULL)                   /* USB charging mode control */
GPIOMEC(USB_ILIM_SEL,      13, GPIO_OUT_HIGH,                             NULL)                   /* USB current control */

GPIOMEC(KBD_IRQ_L,         152, GPIO_ODR_HIGH,                             NULL)                   /* Negative edge triggered irq. */

UNIMPLEMENTED(CPU_PROCHOT)
UNIMPLEMENTED(PCH_RCIN_L)

GPIOMEC(PCH_SMI_L,         44, GPIO_ODR_HIGH,                             NULL)                   /* SMI output */
GPIOMEC(PCH_WAKE_L,        66, GPIO_ODR_HIGH,                             NULL)                   /* PCH wake pin */
GPIOMEC(KBD_KSO2,          101, GPIO_KB_OUTPUT_COL2,                       NULL)                   /* Negative edge triggered irq. */
GPIOMEC(PVT_CS0,        	146, GPIO_ODR_HIGH,                             NULL)                   /* SPI PVT Chip select */
/*
 * Signals which aren't implemented on MEC1322 eval board but we'll
 * emulate anyway, to make it more convenient to debug other code.
 */
UNIMPLEMENTED(ENTERING_RW) /* EC entering RW code */
UNIMPLEMENTED(WP_L) /* SPI WP Input */

/* Alternate functions GPIO definition */
ALTERNATE(PORT(16), 0x24,     1, MODULE_UART,          0) 			/* UART0 */
ALTERNATE(PORT(0),  0x3f,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(10), 0xdd,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(3),  0x04,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(4),  0x0d,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(12), 0x60,     2, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(14), 0x14,     3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(1),  0x10,     1, MODULE_LPC,           0)                       /* 14: CLKRUN# */
ALTERNATE(PORT(11), 0x9e,     1, MODULE_LPC,           0)                       /* 111~114:LAD[0:3],  117:PCI_CLK */
ALTERNATE(PORT(11), 0x40,     1, MODULE_LPC,           GPIO_INT_BOTH)           /* 116: LRESET# */
ALTERNATE(PORT(12), 0x01,     1, MODULE_LPC,           0)                       /* 120: LFRAME# */
ALTERNATE(PORT(5),  0x10,     1, MODULE_SPI,           0)
ALTERNATE(PORT(16), 0x10,     1, MODULE_SPI,           0)
ALTERNATE(PORT(15), 0x8,      1, MODULE_SPI,           0)
