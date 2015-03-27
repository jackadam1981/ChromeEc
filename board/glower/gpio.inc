/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inputs with interrupt handlers are first for efficiency */
GPIOMEC(POWER_BUTTON_L,   203, GPIOMEC_INT_BOTH,                power_button_interrupt)
GPIOMEC(LID_OPEN,         160, GPIOMEC_INT_BOTH,                lid_interrupt)
GPIOMEC(AC_PRESENT,       163, GPIOMEC_INT_BOTH,                extpower_interrupt)
GPIOMEC(PCH_SLP_S3_L,     204, GPIOMEC_INT_BOTH | GPIOMEC_PULL_UP, power_signal_interrupt) /* SLP_S3# signal from PCH */
GPIOMEC(PCH_SLP_S4_L,     210, GPIOMEC_INT_BOTH | GPIOMEC_PULL_UP, power_signal_interrupt) /* SLP_S4# signal from PCH */
GPIOMEC(PP1050_PGOOD,     133, GPIOMEC_INT_BOTH,                power_signal_interrupt) /* Power good on 1.05V */
GPIOMEC(PP3300_PCH_PGOOD, 44, GPIOMEC_INT_BOTH,                power_signal_interrupt) /* Power good on 3.3V (PCH supply) */
GPIOMEC(PP5000_PGOOD,     30, GPIOMEC_INT_BOTH,                power_signal_interrupt) /* Power good on 5V */
GPIOMEC(S5_PGOOD,         62, GPIOMEC_INT_BOTH,                power_signal_interrupt) /* Power good on S5 supplies */
GPIOMEC(VCORE_PGOOD,      57, GPIOMEC_INT_BOTH,                power_signal_interrupt) /* Power good on core VR */
GPIOMEC(WP_L,             12, GPIOMEC_INT_BOTH,                switch_interrupt)       /* Write protect input */

/* Other inputs */
GPIOMEC(BOARD_VERSION1,   6, GPIOMEC_INPUT, NULL)
GPIOMEC(BOARD_VERSION2,   123, GPIOMEC_INPUT, NULL)
GPIOMEC(BOARD_VERSION3,   127, GPIOMEC_INPUT, NULL)

#ifdef CONFIG_CHIPSET_DEBUG
GPIOMEC(PCH_SLP_SX_L,     211, GPIOMEC_INPUT | GPIOMEC_PULL_UP, NULL)
GPIOMEC(PCH_SUS_STAT_L,   201, GPIOMEC_INPUT | GPIOMEC_PULL_UP, NULL)
GPIOMEC(PCH_SUSPWRDNACK,  46, GPIOMEC_INPUT | GPIOMEC_PULL_UP, NULL)
#endif

GPIOMEC(PP1000_S0IX_PGOOD, 35, GPIOMEC_INPUT, NULL)
GPIOMEC(USB1_OC_L,         134, GPIOMEC_INPUT, NULL)
GPIOMEC(USB2_OC_L,         16, GPIOMEC_INPUT, NULL)

/* Outputs; all unasserted by default except for reset signals */
GPIOMEC(CPU_PROCHOT,         145, GPIOMEC_OUT_LOW,  NULL) /* Force CPU to think it's overheated */
GPIOMEC(ENABLE_BACKLIGHT,    200, GPIOMEC_ODR_HIGH, NULL) /* Enable backlight power */
GPIOMEC(ENABLE_TOUCHPAD,     64, GPIOMEC_OUT_LOW,  NULL) /* Enable touchpad power */
GPIOMEC(ENTERING_RW,         33, GPIOMEC_OUT_LOW,  NULL) /* Indicate when EC is entering RW code */
GPIOMEC(LPC_CLKRUN_L,        14, GPIOMEC_ODR_HIGH, NULL) /* Request that PCH drive LPC clock */
GPIOMEC(PCH_CORE_PWROK,      122, GPIOMEC_OUT_LOW,  NULL) /* Indicate core well power is stable */
GPIOMEC(PCH_PWRBTN_L,        130, GPIOMEC_ODR_HIGH, NULL) /* Power button output to PCH */
GPIOMEC(PCH_RCIN_L,          140, GPIOMEC_ODR_HIGH, NULL) /* Reset line to PCH (for 8042 emulation) */
GPIOMEC(PCH_RSMRST_L,        143, GPIOMEC_OUT_LOW,  NULL) /* Reset PCH resume power plane logic */
GPIOMEC(PCH_SMI_L,           55, GPIOMEC_ODR_HIGH, NULL) /* System management interrupt to PCH */
GPIOMEC(PCH_SOC_OVERRIDE_L,  65, GPIOMEC_OUT_LOW,  NULL) /* SOC override signal to PCH; when high, ME ignores security descriptor */
GPIOMEC(PCH_SYS_PWROK,       124, GPIOMEC_OUT_LOW,  NULL) /* EC thinks everything is up and ready */
GPIOMEC(PCH_WAKE_L,          202, GPIOMEC_ODR_HIGH, NULL) /* Wake signal from EC to PCH */
GPIOMEC(PP1350_EN,           147, GPIOMEC_OUT_LOW,  NULL) /* Enable 1.35V supply */
GPIOMEC(PP3300_DX_EN,        50, GPIOMEC_OUT_LOW,  NULL) /* Enable power to lots of peripherals */
GPIOMEC(PP3300_LTE_EN,       11, GPIOMEC_OUT_LOW,  NULL) /* Enable LTE radio */
GPIOMEC(PP3300_WLAN_EN,      47, GPIOMEC_OUT_LOW,  NULL) /* Enable WiFi power */
GPIOMEC(PP5000_EN,           27, GPIOMEC_OUT_LOW,  NULL) /* Enable 5V supply */
GPIOMEC(PPSX_EN,             53, GPIOMEC_INPUT,    NULL) /* Enable PP1350_PCH_SX, PP1000_PCH_SX */
GPIOMEC(SUSP_VR_EN,          66, GPIOMEC_OUT_LOW,  NULL) /* Enable 1.05V regulator */
GPIOMEC(TOUCHSCREEN_RESET_L, 161, GPIOMEC_OUT_HIGH, NULL) /* Reset touch screen */
GPIOMEC(USB_CTL1,            157, GPIOMEC_OUT_LOW,  NULL) /* USB control signal 1 to both ports */
GPIOMEC(USB_ILIM_SEL,        36, GPIOMEC_OUT_LOW,  NULL) /* USB current limit to both ports */
GPIOMEC(USB1_ENABLE,         15, GPIOMEC_OUT_LOW,  NULL) /* USB port 1 output power enable */
GPIOMEC(USB2_ENABLE,         17, GPIOMEC_OUT_LOW,  NULL) /* USB port 2 output power enable */
GPIOMEC(VCORE_EN,            150, GPIOMEC_OUT_LOW,  NULL) /* Enable core power supplies */
GPIOMEC(WLAN_OFF_L,          52, GPIOMEC_OUT_LOW,  NULL) /* Disable WiFi radio */
GPIOMEC(KBD_IRQ_L,           67, GPIOMEC_ODR_HIGH, NULL) /* Negative edge triggered irq. */
GPIOMEC(BAT_LED0,            154, GPIOMEC_ODR_HIGH, NULL)
GPIOMEC(BAT_LED1,            155, GPIOMEC_ODR_HIGH, NULL)
GPIOMEC(KBD_KSO2,            101, GPIOMEC_OUT_LOW,  NULL)

GPIOMEC(SYS_RST_L,           121, GPIOMEC_INPUT,    NULL)
GPIOMEC(EC_HIB,              34, GPIOMEC_INPUT,    NULL)
GPIOMEC(ICMNT,               61, GPIOMEC_INPUT,    NULL)
GPIOMEC(PVT_CS0,             146, GPIOMEC_ODR_HIGH, NULL)

/* Configure VCC_PWRGD as GPIOMEC so that it's internally gated high */
GPIOMEC(TP78,                63, GPIOMEC_OUT_HIGH, NULL)


ALTERNATE(PORT(16), 0x24, 1, MODULE_UART,          0) /* UART0 */
ALTERNATE(PORT(2),  0x0f, 2, MODULE_I2C,           0) /* I2C1 and I2C2 */
ALTERNATE(PORT(0),  0xfe, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_OUTPUT)
ALTERNATE(PORT(1),  0x03, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_OUTPUT)
ALTERNATE(PORT(3),  0x04, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(4),  0x0d, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(10), 0xd8, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_OUTPUT)
ALTERNATE(PORT(12), 0x60, 2, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(14), 0x14, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(2),  0x40, 2, MODULE_LPC,           0) /* LPC SCI */
ALTERNATE(PORT(15), 0x08, 1, MODULE_SPI,           0)
ALTERNATE(PORT(16), 0x10, 1, MODULE_SPI,           0)
ALTERNATE(PORT(5),  0x10, 1, MODULE_SPI,           0)
