/* -*- mode:c -*-
 *
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Button and LED on the MAX32660 EvalBoard */
#if 0
GPIO_INT(BOARD_BUTTON,	PIN(0, 12), GPIO_INT_FALLING | GPIO_PULL_UP, button_interrupt)
GPIO(BOARD_LED,  PIN(0, 13), GPIO_OUTPUT)
#endif

/*
 * Signals which aren't implemented on Board but we'll emulate anyway, to
 * make it more convenient to debug other code.
 */
UNIMPLEMENTED(WP)          /* Write protect input */
UNIMPLEMENTED(ENTERING_RW) /* EC entering RW code */

ALTERNATE(PIN_MASK(0, 0x03), 1, MODULE_UART,     0)      /* UART0 */
ALTERNATE(PIN_MASK(0, 0x03), 1, MODULE_UART,     0)      /* UART1 */

