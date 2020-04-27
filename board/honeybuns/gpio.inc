/* -*- mode:c -*-
 *
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Declare symbolic names for all the GPIOs that we care about.
 * Note: Those with interrupt handlers must be declared first. */

/* Wake Source interrupts */
/* TODO(b/148624000): Change this pin assignment once schematics are final */
GPIO_INT(EC_WP_L,              PIN(A, 1), GPIO_INT_BOTH, switch_interrupt)


/* Outputs */
GPIO(LED1,                PIN(A, 5),  GPIO_OUT_LOW) /* Green */


/* Misc Signals */
UNIMPLEMENTED(ENTERING_RW)

/* LPUART1: A2/A3 */
ALTERNATE(PIN_MASK(A, 0x000C), GPIO_ALT_LPUART,  MODULE_UART, GPIO_PULL_UP)
/* MCO A8 */
ALTERNATE(PIN_MASK(A, 0x0100), 0,  MODULE_CLOCK, 0)


