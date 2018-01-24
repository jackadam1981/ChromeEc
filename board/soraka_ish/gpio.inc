/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO_INT(ACCELGYRO3_INT_L,      PIN(2), GPIO_INT_FALLING | GPIO_SEL_1P8V,    bmi160_interrupt)
GPIO(ISH_GPIO_0,            PIN(0),  GPIO_INPUT)
GPIO(ISH_GPIO_1,            PIN(1),  GPIO_INPUT)
GPIO(ISH_GPIO_3,            PIN(3),  GPIO_INPUT)
GPIO(ISH_GPIO_4,            PIN(4),  GPIO_INPUT)
GPIO(ISH_GPIO_5,            PIN(5),  GPIO_INPUT)
GPIO(ISH_GPIO_6,            PIN(6),  GPIO_INPUT)
GPIO(ISH_GPIO_7,            PIN(7),  GPIO_INPUT)

UNIMPLEMENTED(ENTERING_RW)
