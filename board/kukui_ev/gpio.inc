/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Unimplemented signals since we are not an EC */
GPIO(WP_L,                      PIN(A, 1), GPIO_INPUT | GPIO_PULL_UP)

UNIMPLEMENTED(ENTERING_RW)

/*
 * I2C pins should be configured as inputs until I2C module is
 * initialized. This will avoid driving the lines unintentionally.
 */
GPIO(I2C2_SCL,             PIN(A, 11), GPIO_INPUT)
GPIO(I2C2_SDA,             PIN(A, 12), GPIO_INPUT)

ALTERNATE(PIN_MASK(A, 0x0600), 1, MODULE_UART, 0) /* USART1: PA09/PA10 - Servo stm32 console UART */

ALTERNATE(PIN_MASK(A, 0x1800), 5, MODULE_I2C,  GPIO_ODR_HIGH | GPIO_PULL_UP) /* I2C1: PA11/12 */
ALTERNATE(PIN_MASK(A, 0x0040), 1, MODULE_CLOCK, 0) /* TIM3_CH1: PA6 */
