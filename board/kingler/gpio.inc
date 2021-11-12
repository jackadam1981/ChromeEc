/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Wake Source interrupts */
GPIO_INT(POWER_BUTTON_L,     PIN(0, 1), GPIO_INT_BOTH | GPIO_PULL_UP |
	 GPIO_HIB_WAKE_HIGH, power_button_interrupt) /* GSC_EC_PWR_BTN_ODL */
GPIO_INT(LID_OPEN,           PIN(D, 2), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH,
         lid_interrupt)
GPIO_INT(TABLET_MODE_L,      PIN(B, 2), GPIO_INT_BOTH,
         gmr_tablet_switch_isr)

/* Chipset interrupts */
GPIO_INT(AP_EC_WARM_RST_REQ, PIN(5, 1), GPIO_INT_RISING | GPIO_SEL_1P8V,
         chipset_reset_request_interrupt)

/* Power sequencing interrupts */
GPIO_INT(AP_IN_SLEEP_L, PIN(5, 4),
         GPIO_INT_BOTH | GPIO_PULL_DOWN | GPIO_SEL_1P8V, power_signal_interrupt)

/* Sensor Interrupts */
GPIO_INT(BASE_IMU_INT_L,     PIN(5, 6), GPIO_INT_FALLING | GPIO_SEL_1P8V,
         motion_interrupt)
GPIO_INT(LID_ACCEL_INT_L,    PIN(B, 3), GPIO_INT_FALLING | GPIO_SEL_1P8V,
         lis2dw12_interrupt)

/* Volume button interrupts */
GPIO_INT(VOLUME_DOWN_L,      PIN(E, 2), GPIO_INT_BOTH | GPIO_PULL_UP,
         button_interrupt) /* EC_VOLDN_BTN_ODL */
GPIO_INT(VOLUME_UP_L,        PIN(D, 5), GPIO_INT_BOTH | GPIO_PULL_UP,
         button_interrupt) /* EC_VOLUP_BTN_ODL */

/* Other interrupts */
GPIO_INT(AP_XHCI_INIT_DONE,  PIN(A, 3), GPIO_INT_BOTH | GPIO_PULL_DOWN | GPIO_SEL_1P8V,
         usb_a0_interrupt)
GPIO_INT(AC_PRESENT,         PIN(0, 0), GPIO_INT_BOTH | GPIO_HIB_WAKE_HIGH,
         extpower_interrupt) /* GSC_ACOK_OD */
GPIO_INT(WP,                 PIN(E, 3), GPIO_INT_BOTH | GPIO_SEL_1P8V,
         switch_interrupt) /* EC_FLASH_WP_OD */
GPIO_INT(X_EC_GPIO2,         PIN(D, 4), GPIO_ODR_HIGH, x_ec_interrupt)

/* TODO: check if we need this */
GPIO_INT(AP_EC_SPI_CS_L,    PIN(5, 3), GPIO_INT_FALLING | GPIO_PULL_DOWN, shi_cs_event)  /* EC SPI Chip Select */
/*
 * SPI host interface - enable PDs by default. These will be made functional
 * by the SHI driver when the AP powers up, and restored back to GPIO when
 * the AP powers down.
 * GPIO(AP_EC_SPI_MOSI,        PIN(4, 6), GPIO_INPUT | GPIO_PULL_DOWN)
 * GPIO(AP_EC_SPI_MISO,        PIN(4, 7), GPIO_INPUT | GPIO_PULL_DOWN)
 * GPIO(AP_EC_SPI_CLK,         PIN(5, 5), GPIO_INPUT | GPIO_PULL_DOWN)
 */

/* USB-C interrupts */
/* TODO: interrupt function not ready */
GPIO(USB_C0_BC12_INT_ODL, PIN(8, 0), GPIO_INPUT)
GPIO(USB_C0_PPC_INT_ODL, PIN(6, 7), GPIO_INPUT)

/* Power Sequencing Signals */
GPIO(EC_PMIC_EN_ODL,         PIN(7, 4), GPIO_ODR_HIGH | GPIO_SEL_1P8V)
GPIO(EN_PP5000_Z2,           PIN(3, 4), GPIO_OUT_HIGH)
GPIO(EN_ULP,                 PIN(C, 6), GPIO_OUT_LOW)
GPIO(SYS_RST_ODL,            PIN(C, 5), GPIO_ODR_LOW)
GPIO(EC_BL_EN_OD,            PIN(4, 0), GPIO_ODR_LOW | GPIO_SEL_1P8V)
GPIO(AP_EC_WDTRST_L,         PIN(5, 2), GPIO_INPUT | GPIO_SEL_1P8V)

/* MKBP event synchronization */
GPIO(EC_INT_L,               PIN(C, 1), GPIO_ODR_HIGH | GPIO_SEL_1P8V) /* EC_AP_INT_ODL */

/* USB and USBC Signals */
GPIO(DP_AUX_PATH_SEL,        PIN(6, 3), GPIO_OUT_HIGH)
GPIO(EC_AP_DP_HPD_ODL,       PIN(6, 1), GPIO_ODR_HIGH)
GPIO(EN_PP5000_USB_A0_VBUS,  PIN(6, 0), GPIO_OUT_LOW)

/* Misc Signals */
GPIO(EC_BATT_PRES_ODL,       PIN(E, 0), GPIO_INPUT)
GPIO(EN_EC_ID_ODL,           PIN(7, 6), GPIO_ODR_LOW)
GPIO(ENTERING_RW,            PIN(0, 3), GPIO_OUT_LOW) /* EC_ENTERING_RW */
GPIO(EN_5V_USM,              PIN(0, 2), GPIO_OUT_LOW)
GPIO(USB_A0_FAULT_ODL,       PIN(C, 7), GPIO_INPUT)

/* I2C pins - Alternate function below configures I2C module on these pins */
GPIO(EC_I2C_SENSOR_SCL,              PIN(B, 5), GPIO_INPUT |GPIO_SEL_1P8V)
GPIO(EC_I2C_SENSOR_SDA,              PIN(B, 4), GPIO_INPUT |GPIO_SEL_1P8V)
GPIO(EC_I2C_USB_C0_SCL,              PIN(9, 0), GPIO_INPUT)
GPIO(EC_I2C_USB_C0_SDA,              PIN(8, 7), GPIO_INPUT)
GPIO(EC_I2C_USB_C1_SCL,              PIN(9, 2), GPIO_INPUT)
GPIO(EC_I2C_USB_C1_SDA,              PIN(9, 1), GPIO_INPUT)
GPIO(EC_I2C_PWR_CBI_SCL,             PIN(D, 1), GPIO_INPUT)
GPIO(EC_I2C_PWR_CBI_SDA,             PIN(D, 0), GPIO_INPUT)
GPIO(EC_I2C_BATT_SCL,                PIN(3, 3), GPIO_INPUT)
GPIO(EC_I2C_BATT_SDA,                PIN(3, 6), GPIO_INPUT)


/*
 * SPI host interface - enable PDs by default. These will be made functional
 * by the SHI driver when the AP powers up, and restored back to GPIO when
 * the AP powers down.
 */
GPIO(AP_EC_SPI_MOSI,        PIN(4, 6), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(AP_EC_SPI_MISO,        PIN(4, 7), GPIO_INPUT | GPIO_PULL_DOWN)
GPIO(AP_EC_SPI_CLK,         PIN(5, 5), GPIO_INPUT | GPIO_PULL_DOWN)
/* NC / TP */

/* Keyboard pins */

/* Subboards HDMI/TYPEC */
GPIO(EC_X_GPIO1,             PIN(6, 2), GPIO_OUT_LOW)
GPIO(EC_X_GPIO3,             PIN(D, 3), GPIO_INPUT)
GPIO(HDMI_PRSNT_ODL,         PIN(3, 7), GPIO_INPUT) /* low -> hdmi, other -> usb */

/* Alternate functions GPIO definitions */
ALTERNATE(PIN_MASK(B, 0x30), 1, MODULE_I2C, 0)			/* I2C 0 (GPIOB4/B5) */
ALTERNATE(PIN_MASK(9, 0x07), 1, MODULE_I2C, 0)			/* I2C1 SCL (GPIO90),I2C2 (GPIO91/92) */
ALTERNATE(PIN_MASK(8, 0x80), 1, MODULE_I2C, 0)			/* I2C1 SDA (GPIO87) */
ALTERNATE(PIN_MASK(D, 0x03), 1, MODULE_I2C, GPIO_SEL_1P8V)	/* I2C 3 (GPIOD0/D1)*/
ALTERNATE(PIN_MASK(3, 0x48), 1, MODULE_I2C, 0)			/* I2C 5 (GPIO33/36) */

/* Keyboard */
ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_INPUT | GPIO_PULL_UP) /* KSI0, KSI1 */
ALTERNATE(PIN_MASK(2, 0xFC), 0, MODULE_KEYBOARD_SCAN, GPIO_INPUT | GPIO_PULL_UP) /* KSI2-7 */
ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		 /* KSO0, KSO1 */
ALTERNATE(PIN_MASK(1, 0x7F), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		 /* KSO3-9 */
ALTERNATE(PIN_MASK(0, 0xF0), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		 /* KSO10-13 */
ALTERNATE(PIN_MASK(8, 0x04), 0, MODULE_KEYBOARD_SCAN, GPIO_ODR_HIGH)		 /* KSO14 */
GPIO(EC_KSO_02_INV, PIN(1, 7), GPIO_OUT_LOW)	/* KSO2 inverted */

/* UART */
ALTERNATE(PIN_MASK(6, 0x30), 1, MODULE_UART, 0) /* EC to Servo */

/* PWM */
ALTERNATE(PIN_MASK(C, 0x1C), 1, MODULE_PWM, 0) /* PWM 0~2 */
ALTERNATE(PIN_MASK(B, 0x80), 1, MODULE_PWM, 0) /* PWM 5 */


/* ADC */
ALTERNATE(PIN_MASK(4, 0x3C), 1, MODULE_ADC, 0)	/* ADC0-3 */

/* SPI */
ALTERNATE(PIN_MASK(5, 0x28), 1, MODULE_SPI, 0) /* SPI */
ALTERNATE(PIN_MASK(4, 0xC0), 1, MODULE_SPI, 0)

/* Unimplemented Pins  */
GPIO(PACKET_MODE_EN,         PIN(7, 5), GPIO_OUT_LOW)
GPIO(PG_PP5000_Z2_OD,        PIN(7, 0), GPIO_INPUT)
GPIO(PG_MT6315_PROC_B_ODL,   PIN(E, 1), GPIO_INPUT)////////////////////////
GPIO(EC_PEN_CHG_DIS_ODL,     PIN(E, 4), GPIO_ODR_HIGH) /* 5V output */
/* reserved for future use */
GPIO(CCD_MODE_ODL,           PIN(E, 5), GPIO_INPUT)
/*
 * ADC pins don't have internal pull-down capability,
 * so we set them as output low.
 */

UNIMPLEMENTED(USB_C0_PPC_FRSINFO)
UNIMPLEMENTED(USB_C1_BC12_CHARGER_INT_ODL)

UNIMPLEMENTED(AP_EC_WATCHDOG_L)
UNIMPLEMENTED(EC_PMIC_WATCHDOG_L)
UNIMPLEMENTED(PMIC_EC_PWRGD)
UNIMPLEMENTED(BC12_DET_EN)
