/* -*- mode:c -*-
 *
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Interrupts */
GPIO_INT(SENSOR_INT, PIN(B, 8),  GPIO_INT_RISING, sensor_event)
GPIO_INT(BUTTON_INT, PIN(B, 12), GPIO_INT_BOTH, button_event)

/* Outputs */
GPIO(SPI1_NSS,       PIN(A, 4),  GPIO_OUT_HIGH)
/* Disable host interrupt temporarily for debugging */
GPIO(INT_L,          PIN(A, 10), GPIO_OUT_HIGH)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C1_SCL,       PIN(B, 6),  GPIO_INPUT)
GPIO(I2C1_SDA,       PIN(B, 7),  GPIO_INPUT)

/* USB pins */
GPIO(USB_FS_DM,      PIN(A, 11), GPIO_INPUT)
GPIO(USB_FS_DP,      PIN(A, 12), GPIO_INPUT)

/* Unimplemented signals since we are not an EC */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)


ALTERNATE(PIN_MASK(A, 0x000c), 7,  MODULE_UART, 0) /* UART2: PA2,3  - Console */
ALTERNATE(PIN_MASK(B, 0x00c0), 4,  MODULE_I2C, 0)  /* I2C1 SLAVE:PB6,7 */
ALTERNATE(PIN_MASK(A, 0x1800), 10, MODULE_USB, 0)  /* USB OTG: PA11,12 */
ALTERNATE(PIN_MASK(A, 0x00a0), 5,  MODULE_SPI_MASTER, 0) /* SPI MASTER: PA5,7 */
