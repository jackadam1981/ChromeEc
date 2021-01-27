/* -*- mode:c -*-
 *
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define MODULE_KB		MODULE_KEYBOARD_SCAN

#include "generated-gpio.inc"

IOEX(USB_C0_RT_RST_ODL, EXPIN(IOEX_C0_NCT38XX, 0, 2), GPIO_ODR_LOW)
IOEX(USB_C2_RT_RST_ODL, EXPIN(IOEX_C2_NCT38XX, 0, 2), GPIO_ODR_LOW)

UNIMPLEMENTED(EN_PP3300_A)
UNIMPLEMENTED(EN_PP5000)
