/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inputs with interrupt handlers are first for efficiency */
GPIO(POWER_BUTTON_L,   NA,203, GPIO_INT_BOTH,                power_button_interrupt)
GPIO(LID_OPEN,         NA,160, GPIO_INT_BOTH,                lid_interrupt)
GPIO(AC_PRESENT,       NA,163, GPIO_INT_BOTH,                extpower_interrupt)
GPIO(PCH_SLP_S3_L,     NA,204, GPIO_INT_BOTH | GPIO_PULL_UP, power_signal_interrupt) /* SLP_S3# signal from PCH */
GPIO(PCH_SLP_S4_L,     NA,210, GPIO_INT_BOTH | GPIO_PULL_UP, power_signal_interrupt) /* SLP_S4# signal from PCH */
GPIO(PP1050_PGOOD,     NA,133, GPIO_INT_BOTH,                power_signal_interrupt) /* Power good on 1.05V */
GPIO(PP3300_PCH_PGOOD, NA, 44, GPIO_INT_BOTH,                power_signal_interrupt) /* Power good on 3.3V (PCH supply) */
GPIO(PP5000_PGOOD,     NA, 30, GPIO_INT_BOTH,                power_signal_interrupt) /* Power good on 5V */
GPIO(S5_PGOOD,         NA, 62, GPIO_INT_BOTH,                power_signal_interrupt) /* Power good on S5 supplies */
GPIO(VCORE_PGOOD,      NA, 57, GPIO_INT_BOTH,                power_signal_interrupt) /* Power good on core VR */
GPIO(WP_L,             NA, 12, GPIO_INT_BOTH,                switch_interrupt)       /* Write protect input */

/* Other inputs */
GPIO(BOARD_VERSION1,   NA, 06, GPIO_INPUT, NULL)
GPIO(BOARD_VERSION2,   NA,123, GPIO_INPUT, NULL)
GPIO(BOARD_VERSION3,   NA,127, GPIO_INPUT, NULL)

#ifdef CONFIG_CHIPSET_DEBUG
GPIO(PCH_SLP_SX_L,     NA,211, GPIO_INPUT | GPIO_PULL_UP, NULL)
GPIO(PCH_SUS_STAT_L,   NA,201, GPIO_INPUT | GPIO_PULL_UP, NULL)
GPIO(PCH_SUSPWRDNACK,  NA, 46, GPIO_INPUT | GPIO_PULL_UP, NULL)
#endif

GPIO(PP1000_S0IX_PGOOD, NA,35, GPIO_INPUT, NULL)
GPIO(USB1_OC_L,         NA,134, GPIO_INPUT, NULL)
GPIO(USB2_OC_L,         NA,16, GPIO_INPUT, NULL)

/* Outputs; all unasserted by default except for reset signals */
GPIO(CPU_PROCHOT,         NA,145, GPIO_OUT_LOW,  NULL) /* Force CPU to think it's overheated */
GPIO(ENABLE_BACKLIGHT,    NA,200, GPIO_ODR_HIGH, NULL) /* Enable backlight power */
GPIO(ENABLE_TOUCHPAD,     NA, 64, GPIO_OUT_LOW,  NULL) /* Enable touchpad power */
GPIO(ENTERING_RW,         NA, 33, GPIO_OUT_LOW,  NULL) /* Indicate when EC is entering RW code */
GPIO(LPC_CLKRUN_L,        NA, 14, GPIO_ODR_HIGH, NULL) /* Request that PCH drive LPC clock */
GPIO(PCH_CORE_PWROK,      NA,122, GPIO_OUT_LOW,  NULL) /* Indicate core well power is stable */
GPIO(PCH_PWRBTN_L,        NA,130, GPIO_ODR_HIGH, NULL) /* Power button output to PCH */
GPIO(PCH_RCIN_L,          NA,140, GPIO_ODR_HIGH, NULL) /* Reset line to PCH (for 8042 emulation) */
GPIO(PCH_RSMRST_L,        NA,143, GPIO_OUT_LOW,  NULL) /* Reset PCH resume power plane logic */
GPIO(PCH_SMI_L,           NA, 55, GPIO_ODR_HIGH, NULL) /* System management interrupt to PCH */
GPIO(PCH_SOC_OVERRIDE_L,  NA, 65, GPIO_OUT_LOW,  NULL) /* SOC override signal to PCH; when high, ME ignores security descriptor */
GPIO(PCH_SYS_PWROK,       NA,124, GPIO_OUT_LOW,  NULL) /* EC thinks everything is up and ready */
GPIO(PCH_WAKE_L,          NA,202, GPIO_ODR_HIGH, NULL) /* Wake signal from EC to PCH */
GPIO(PP1350_EN,           NA,147, GPIO_OUT_LOW,  NULL) /* Enable 1.35V supply */
GPIO(PP3300_DX_EN,        NA, 50, GPIO_OUT_LOW,  NULL) /* Enable power to lots of peripherals */
GPIO(PP3300_LTE_EN,       NA, 11, GPIO_OUT_LOW,  NULL) /* Enable LTE radio */
GPIO(PP3300_WLAN_EN,      NA, 47, GPIO_OUT_LOW,  NULL) /* Enable WiFi power */
GPIO(PP5000_EN,           NA, 27, GPIO_OUT_LOW,  NULL) /* Enable 5V supply */
GPIO(PPSX_EN,             NA, 53, GPIO_INPUT,    NULL) /* Enable PP1350_PCH_SX, PP1000_PCH_SX */
GPIO(SUSP_VR_EN,          NA, 66, GPIO_OUT_LOW,  NULL) /* Enable 1.05V regulator */
GPIO(TOUCHSCREEN_RESET_L, NA,161, GPIO_OUT_HIGH, NULL) /* Reset touch screen */
GPIO(USB_CTL1,            NA,157, GPIO_OUT_LOW,  NULL) /* USB control signal 1 to both ports */
GPIO(USB_ILIM_SEL,        NA, 36, GPIO_OUT_LOW,  NULL) /* USB current limit to both ports */
GPIO(USB1_ENABLE,         NA, 15, GPIO_OUT_LOW,  NULL) /* USB port 1 output power enable */
GPIO(USB2_ENABLE,         NA, 17, GPIO_OUT_LOW,  NULL) /* USB port 2 output power enable */
GPIO(VCORE_EN,            NA,150, GPIO_OUT_LOW,  NULL) /* Enable core power supplies */
GPIO(WLAN_OFF_L,          NA, 52, GPIO_OUT_LOW,  NULL) /* Disable WiFi radio */
GPIO(KBD_IRQ_L,           NA, 67, GPIO_ODR_HIGH, NULL) /* Negative edge triggered irq. */
GPIO(BAT_LED0,            NA,154, GPIO_ODR_HIGH, NULL)
GPIO(BAT_LED1,            NA,155, GPIO_ODR_HIGH, NULL)
GPIO(KBD_KSO2,            NA,101, GPIO_OUT_LOW,  NULL)

GPIO(SYS_RST_L,           NA,121, GPIO_INPUT,    NULL)
GPIO(EC_HIB,              NA, 34, GPIO_INPUT,    NULL)
GPIO(ICMNT,               NA, 61, GPIO_INPUT,    NULL)
GPIO(PVT_CS0,             NA,146, GPIO_ODR_HIGH, NULL)

/* Configure VCC_PWRGD as GPIO so that it's internally gated high */
GPIO(TP78,                NA, 63, GPIO_OUT_HIGH, NULL)


ALTERNATE(PORT(16), 0x24, 1, MODULE_UART,          0) /* UART0 */
ALTERNATE(PORT(2),  0x0f, 2, MODULE_I2C,           0) /* I2C1 and I2C2 */
ALTERNATE(PORT(0),  0xfe, 3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(1),  0x03, 3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(3),  0x04, 3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(4),  0x0d, 3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(10), 0xd8, 3, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PORT(12), 0x60, 2, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(14), 0x14, 3, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PORT(2),  0x40, 2, MODULE_LPC,           0) /* LPC SCI */
ALTERNATE(PORT(15), 0x08, 1, MODULE_SPI,           0)
ALTERNATE(PORT(16), 0x10, 1, MODULE_SPI,           0)
ALTERNATE(PORT(5),  0x10, 1, MODULE_SPI,           0)
