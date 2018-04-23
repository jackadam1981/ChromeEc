/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

GPIO_INT(EMMC_CMD, PIN(B, 5), GPIO_INT_FALLING, emmc_cmd_interrupt)

/* Unimplemented signals since we are not an EC */
GPIO(WP_L,                      PIN(A, 13), GPIO_INPUT | GPIO_PULL_UP)
GPIO(SPI_TEST,                  PIN(B, 12), GPIO_OUT_LOW)

UNIMPLEMENTED(ENTERING_RW)

ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART, GPIO_PULL_UP) /* USART1: PA09/PA10 - Servo stm32 console UART */

ALTERNATE(PIN_MASK(A, 0x0070), 0, MODULE_SPI, 0) /* SPI1 SLAVE:PA4/5/6 */
ALTERNATE(PIN_MASK(B, 0x0020), 0, MODULE_SPI, 0) /* SPI1 SLAVE:PB5 */
