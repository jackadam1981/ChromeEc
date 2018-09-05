/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO_INT(EINT5_TP, PIN(5), GPIO_INT_FALLING, eint_event)
GPIO_INT(EINT6_TP, PIN(6), GPIO_INT_FALLING, eint_event)
GPIO_INT(EINT7_TP, PIN(7), GPIO_INT_FALLING, eint_event)

/* Unimplemented signals which we need to emulate for now */
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(WP_L)

/*
 * EINT ALT function:
 *   GPIO[0-3]: Alt3
 *   GPIO[4-7]: Alt6
 */
ALTERNATE(PIN_MASK(0, 0x00000070), 1, MODULE_GPIO, 0) /* GPIO 5,6,7 as SCP EINT */
