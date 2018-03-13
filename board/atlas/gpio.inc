/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* USB PD interrupt handler section */

GPIO_INT(USB_C0_PD_INT_ODL,	PIN(6, 1), GPIO_INT_FALLING,			tcpc_alert_event)
GPIO_INT(USB_C1_PD_INT_ODL,	PIN(F, 5), GPIO_INT_FALLING,			tcpc_alert_event)

/* power seq interrupt handler section */
GPIO_INT(ROP_DSW_PWROK_EC,	PIN(F, 4), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(ROP_EC_RSMRST_L,	PIN(E, 2), GPIO_INT_BOTH,			power_signal_interrupt)
GPIO_INT(MECH_PWR_BTL_ODL,	PIN(0, 1), GPIO_INT_BOTH | GPIO_PULL_UP,	power_button_interrupt)
GPIO_INT(SLP_S0_L,		PIN(A, 4), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S4_L,	    	PIN(A, 3), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_S3_L,    		PIN(A, 6), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(SLP_SUS_L_PCH,		PIN(D, 4), GPIO_INT_BOTH, power_signal_interrupt)
GPIO_INT(ROP_EC_ACOK,		PIN(0, 0), GPIO_INT_BOTH, extpower_interrupt)
GPIO_INT(ROP_INT_L,		PIN(D, 5), GPIO_INT_BOTH, extpower_interrupt)

/* misc interrupt handler section */

GPIO_INT(EC_WP_L,			PIN(A, 1), GPIO_INT_BOTH, switch_interrupt)
GPIO_INT(LID_OPEN,		PIN(D, 2), GPIO_INT_BOTH,			lid_interrupt)
GPIO_INT(ACCELGYRO3_INT_L,	PIN(4, 1), GPIO_INT_FALLING,			bmi160_interrupt)

/* SoC section */

/* GPIO37 N/C */
GPIO(RSMRST_L,			PIN(C, 2), GPIO_OUT_LOW)
GPIO(EC_PCH_PWR_BTN_ODL,		PIN(C, 1), GPIO_ODR_HIGH)
GPIO(EC_PCH_RTCRST,		PIN(7, 6), GPIO_OUT_LOW)
GPIO(EC_PCH_WAKE_L,		PIN(7, 4), GPIO_ODR_HIGH)
GPIO(EC_PROCHOT_ODL,		PIN(3, 4), GPIO_ODR_HIGH)
GPIO(SYS_RESET_L,		PIN(0, 2), GPIO_ODR_HIGH)
GPIO(USB_C0_DP_HPD,		PIN(C, 5), GPIO_OUT_LOW)
GPIO(USB_C1_DP_HPD,		PIN(C, 6), GPIO_OUT_LOW)
/* GPIOC7 N/C */

/* power seq section */

// GPIO_INT(ROP_DSW_PWROK_EC,	PIN(F, 4), GPIO_INT_BOTH,			power_signal_interrupt)
// GPIO_INT(ROP_EC_RSMRST_L,	PIN(E, 2), GPIO_INT_BOTH,			power_signal_interrupt)
// GPIO_INT(MECH_PWR_BTL_ODL,	PIN(0, 1), GPIO_INT_BOTH | GPIO_PULL_UP,	power_button_interrupt)
GPIO(EC_PSL_OUT,		PIN(8, 5), GPIO_OUTPUT)
/* GPIO60 N/C */
GPIO(EC_PCH_ACPRESENT,		PIN(7, 3), GPIO_ODR_LOW)			/* ACOK to SOC */
GPIO(SLP_SUS_L_PMIC,		PIN(B, 7), GPIO_INPUT)
/* GPIOC0 CHARGE_LED3 */
/* GPIOD7 N/C */
// GPIO_INT(SLP_S0_L,		PIN(A, 4), GPIO_INT_BOTH, power_signal_interrupt)
// GPIO_INT(SLP_S4_L,	    	PIN(A, 3), GPIO_INT_BOTH, power_signal_interrupt)
// GPIO_INT(SLP_S3_L,    		PIN(A, 6), GPIO_INT_BOTH, power_signal_interrupt)
// GPIO_INT(SLP_SUS_L_PCH,		PIN(D, 4), GPIO_INT_BOTH, power_signal_interrupt)
// GPIO_INT(ROP_EC_ACOK,		PIN(0, 0), GPIO_INT_BOTH, extpower_interrupt)
// GPIO_INT(ROP_INT_L,		PIN(D, 5), GPIO_INT_BOTH, extpower_interrupt)

/* USB PD section */

GPIO(EN_USB_C0_5V_OUT,		PIN(6, 7), GPIO_OUT_LOW | GPIO_PULL_UP)		/* C0 5V Enable */
GPIO(EN_USB_C0_CHARGE_L,	PIN(0, 3), GPIO_OUT_LOW)			/* %alt% */
/* %int% GPIO61 USB_C0_PD_INT_ODL  */
GPIO(EN_USB_C0_3A,		PIN(6, 2), GPIO_OUT_LOW)			/* 1.5/3.0 C0 current limit selection */
/* GPIO82 N/C */
GPIO(EN_USB_C1_5V_OUT,		PIN(7, 0), GPIO_OUT_LOW | GPIO_PULL_UP)		/* C1 5V Enable */
GPIO(EN_USB_C1_CHARGE_L,	PIN(0, 4), GPIO_OUT_LOW)			/* %alt% */
/* %int% GPIOF5 USB_C1_PD_INT_ODL */
GPIO(EN_USB_C1_3A,		PIN(8, 3), GPIO_OUT_LOW)			/* %alt% 1.5/3.0 C1 current limit selection */
/* GPIOB1 N/C */
/* GPIO63 N/C */
/* GPIO40 N/C */
/* GPIO95 %alt% */
/* GPIOE4 N/C */
/* GPIOE0 N/C */
/* GPIOA2 N/C */
/* GPIOA0 N/C */
GPIO(USB_PD_RST_L,		PIN(F, 1), GPIO_OUT_LOW)			/* C0,C1 PD Reset */
GPIO(CHARGER_IADP,		PIN(4, 3), GPIO_INPUT)
GPIO(CHARGER_PMON,		PIN(4, 2), GPIO_INPUT)

/* misc section */

/* GPIO56 N/C */
/* GPIO50 N/C */
GPIO(CCD_MODE_ODL,		PIN(E, 3), GPIO_INPUT)				/* Case Closed Debug Mode */
GPIO(EC_BATT_PRES_L,		PIN(E, 5), GPIO_INPUT)				/* Battery Present */
GPIO(EC_ENTERING_RW,		PIN(E, 1), GPIO_OUTPUT)				/* EC Entering RW */
// GPIO_INT(EC_WP_L,		PIN(A, 1), GPIO_INT_BOTH, switch_interrupt)
/* %alt% GPIO80/PWM3 KBD_BL_PWM */
/* %alt% GPIOC3/PWM0 CHARGE_LED1 */
/* %alt% GPIOC4/PWM2 CHARGE_LED2 */
GPIO(EC_BL_DISABLE_L,		PIN(D, 3), GPIO_OUT_LOW)			/* Enable Backlight */
// GPIO_INT(LID_OPEN,		PIN(D, 2), GPIO_INT_BOTH,			lid_interrupt)
/* GPIO45 N/C */
/* GPIO44 N/C */
GPIO(EC_BRD_ID1,		PIN(9, 6), GPIO_INPUT)				/* Board ID bit0 */
GPIO(EC_BRD_ID2,		PIN(9, 3), GPIO_INPUT)				/* Board ID bit1 */
GPIO(EC_BRD_ID3,		PIN(F, 0), GPIO_INPUT)				/* Board ID bit2 */
// GPIO_INT(ACCELGYRO3_INT_L,	PIN(4, 1), GPIO_INT_FALLING,			bmi160_interrupt)

GPIO(KBD_BL_EN,			PIN(7, 5), GPIO_OUTPUT)				/* KB backlight enable */
GPIO(EC_PLATFORM_RST,		PIN(8, 6), GPIO_OUT_LOW)			/* EC Reset to LDO_EN */
GPIO(GPIO57,			PIN(5, 7), GPIO_INPUT)				/* N/C */

/* I2C */

/* I2C pins - these will be reconfigured for alternate function below */
GPIO(I2C0_0_SCL,		PIN(9, 0), GPIO_INPUT)				/* EC_I2C00_USB_C0_SCL */
GPIO(I2C0_0_SDA,		PIN(8, 7), GPIO_INPUT)				/* EC_I2C00_USB_C0_SDA */
GPIO(I2C0_1_SCL,		PIN(9, 2), GPIO_INPUT)				/* EC_I2C00_USB_C1_SCL */
GPIO(I2C0_1_SDA,		PIN(9, 1), GPIO_INPUT)				/* EC_I2C00_USB_C1_SDA */
GPIO(I2C1_SCL,			PIN(3, 3), GPIO_INPUT)				/* EC_I2C1_GYRO_SCL */
GPIO(I2C1_SDA,			PIN(3, 6), GPIO_INPUT)				/* EC_I2C1_GYRO_SDA */
GPIO(I2C2_SCL,			PIN(D, 1), GPIO_INPUT)				/* EC_I2C2_SENSOR_3V3_SCL */
GPIO(I2C2_SDA,			PIN(D, 0), GPIO_INPUT)				/* EC_I2C2_SENSOR_3V3_SDA */
GPIO(I2C3_SCL,			PIN(B, 5), GPIO_INPUT)				/* EC_I2C3_POWER_SCL */
GPIO(I2C3_SDA,			PIN(B, 4), GPIO_INPUT)				/* EC_I2C3_POWER_SDA */
/* GPIOB3 N/C */
/* GPIOB2 N/C */
GPIO(I2C4_1_SCL,		PIN(F, 3), GPIO_INPUT)				/* EC_I2C4_POWER_SCL */
GPIO(I2C4_1_SDA,		PIN(F, 2), GPIO_INPUT)				/* EC_I2C4_POWER_SDA */

/* uart */

/* %alt% GPIO65/CR_SOUT */
/* %alt% GPIO64/CR_SIN */

/* keyboard */

#define GPIO_KB_INPUT		(GPIO_INPUT | GPIO_PULL_UP)
#define GPIO_KB_OUTPUT		(GPIO_ODR_HIGH)
#define GPIO_KB_OUTPUT_COL2	(GPIO_OUT_LOW)

/* actually called EC_KB_ROW02_INV */
/* KBD_KSO2 */
GPIO(EC_KB_ROW02_INV,			PIN(1, 7), GPIO_KB_OUTPUT_COL2)

/* VCC1_RST EC_VCC1_RST_ODL */

/* CLK */

/* 32KXIN N/C */
/* 32KXOUT */

/* power seq alternate functions */

ALTERNATE(PIN_MASK(A, 0x08), 1, MODULE_GPIO, 0)		/* GPIOA3 SLP_S4_L */

/* USB PD alternate functions */

ALTERNATE(PIN_MASK(0, 0x08), 1, MODULE_USB, 0)		/* GPIO03 EN_USB_C0_CHARGE_L */
ALTERNATE(PIN_MASK(0, 0x10), 1, MODULE_USB, 0)		/* GPIO04 EN_USB_C1_CHARGE_L */
ALTERNATE(PIN_MASK(8, 0x08), 1, MODULE_USB, 0)		/* GPIO83 EN_USB_C1_3A */
ALTERNATE(PIN_MASK(9, 0x20), 1, MODULE_SWITCH, 0)	/* GPIO95 EC_GPIO95 */

/* misc alternate functions */

ALTERNATE(PIN_MASK(A, 0x02), 1, MODULE_SWITCH, 0)	/* GPIOA1 EC_WP_L */
ALTERNATE(PIN_MASK(5, 0x80), 1, MODULE_SWITCH, 0)	/* GPIO57 N/C */

/* keyboard backlight PWM channel */
ALTERNATE(PIN_MASK(8, 0x01), 1, MODULE_PWM, 0)		/* GPIO80/PWM3 KBD_BL_PWM */

/* LED PWM channels */
ALTERNATE(PIN_MASK(C, 0x08), 1, MODULE_PWM, 0)		/* GPIOC3 PWM0 CHARGE_LED1 */
ALTERNATE(PIN_MASK(C, 0x10), 1, MODULE_PWM, 0)		/* GPIOC4 PWM2 CHARGE_LED2 */
ALTERNATE(PIN_MASK(C, 0x01), 1, MODULE_PWM, 0)		/* GPIOC0 PWM6 CHARGE_LED3 */

/* I2C alternate functions */

ALTERNATE(PIN_MASK(9, 0x01), 1, MODULE_I2C, 0) /* I2C1_SCL0 */
ALTERNATE(PIN_MASK(8, 0x80), 1, MODULE_I2C, 0) /* I2C1_SDA0 */
ALTERNATE(PIN_MASK(9, 0x06), 1, MODULE_I2C, 0) /* I2C2_SCL0|I2C2_SDA0 */
ALTERNATE(PIN_MASK(3, 0x48), 1, MODULE_I2C, 0) /* I2C5_SDA0|I2C5_SCL0 */
ALTERNATE(PIN_MASK(D, 0x03), 1, MODULE_I2C, 0) /* I2C3_SCL0|I2C3_SDA0 */
ALTERNATE(PIN_MASK(B, 0x30), 1, MODULE_I2C, 0) /* I2C0_SCL0|I2C0_SDA0 */
ALTERNATE(PIN_MASK(B, 0x0c), 1, MODULE_I2C, 0) /* I2C7_SCL0|I2C7_SDA0 */
ALTERNATE(PIN_MASK(F, 0x0c), 1, MODULE_I2C, 0) /* I2C4_SCL1|I2C4_SDA1 */

/* UART alternate functions */

ALTERNATE(PIN_MASK(6, 0x30), 1, MODULE_UART, 0)		/* GPIO65-64 */		/* UART from EC to Servo */

/* keyboard alternate functions */

ALTERNATE(PIN_MASK(3, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PIN_MASK(2, 0xfc), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_INPUT)
ALTERNATE(PIN_MASK(2, 0x03), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PIN_MASK(1, 0x7f), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)
ALTERNATE(PIN_MASK(0, 0xe0), 0, MODULE_KEYBOARD_SCAN, GPIO_KB_OUTPUT)

/* WoV alternate functions */

ALTERNATE(PIN_MASK(9, 0x90), 1,  MODULE_WOV, 0) /* DMIC_DATA/DMIC_CLK GPIO97/94 */
ALTERNATE(PIN_MASK(A, 0xA0), 1,  MODULE_WOV, 0) /* I2S_CLK/I2S_SYNC   GPIOA7/A5 */
ALTERNATE(PIN_MASK(B, 0x01), 1,  MODULE_WOV, 0) /* I2S_DATA           GPIOB0    */
