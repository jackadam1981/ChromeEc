/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Minimal set of GPIOs needed for LFW loader
 */

/* Alternate functions GPIO definition */

/*
 * UART0
 * GPIO_0105 Func 1 = UART_RX
 * GPIO_0104 Func 1 = UART_TX
 * Bank 2 bits[5:4]
 */
ALTERNATE(PIN_MASK(2, 0x30), 1, MODULE_UART, 0)

/* include gpio_common.inc under chip/mchp/lfw/... */
#include "../chip/mchp/lfw/gpio_common.inc"
