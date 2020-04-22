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

/* USART1: C4/C5 */
ALTERNATE(PIN_MASK(C, 0x0030), GPIO_ALT_USART,  MODULE_UART, GPIO_PULL_UP)



