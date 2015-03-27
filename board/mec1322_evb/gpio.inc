/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIOMEC(LED1,       154, GPIOMEC_ODR_LOW,  NULL)
GPIOMEC(LED2,       155, GPIOMEC_ODR_HIGH, NULL)
GPIOMEC(LED3,       156, GPIOMEC_ODR_LOW,  NULL)
GPIOMEC(PCH_SMI_L,  44, GPIOMEC_ODR_HIGH, NULL) /* SMI output */
GPIOMEC(PCH_WAKE_L, 200, GPIOMEC_ODR_HIGH, NULL) /* PCH wake pin */

/* Switch S1 */
GPIOMEC(S1,         63, GPIOMEC_INT_FALLING | GPIOMEC_PULL_UP, NULL)

/* Shared SPI CS */
GPIOMEC(SHD_CS0,    150, GPIOMEC_ODR_HIGH, NULL)

/*
 * Signals which aren't implemented on MEC1322 eval board but we'll
 * emulate anyway, to make it more convenient to debug other code.
 */
UNIMPLEMENTED(RECOVERY_L)  /* Recovery signal from DOWN button */
UNIMPLEMENTED(WP_L)        /* Write protect input */
UNIMPLEMENTED(ENTERING_RW) /* EC entering RW code */

ALTERNATE(PORT(16), 0x24,     1, MODULE_UART,    0) /* UART0 */
ALTERNATE(PORT(3),  (1 << 4), 3, MODULE_PWM_FAN, 0)
ALTERNATE(PORT(14), (1 << 0), 3, MODULE_PWM_FAN, 0)
/*
 * I2C0: External pull-up resistors on EVB are too weak. Let's
 * also enable internal pull-up here.
 */
ALTERNATE(PORT(1),  0x60, 2, MODULE_I2C,           GPIOMEC_PULL_UP)
ALTERNATE(PORT(0),  0xfe, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_OUTPUT)
ALTERNATE(PORT(1),  0x03, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_OUTPUT)
ALTERNATE(PORT(3),  0x04, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(4),  0x0d, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(10), 0xd8, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_OUTPUT)
ALTERNATE(PORT(12), 0x60, 2, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(14), 0x14, 3, MODULE_KEYBOARD_SCAN, GPIOMEC_KB_INPUT)
ALTERNATE(PORT(2),  0x20, 2, MODULE_LPC,           0)
ALTERNATE(PORT(12), 0x14, 1, MODULE_SPI,           0)
ALTERNATE(PORT(6),  0x10, 1, MODULE_SPI,           0)
