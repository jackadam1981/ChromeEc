/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Those are example, board specific */
GPIO_INT(ISH_GPIO_3_INT,    PIN(3), GPIO_INT_F_HIGH,    bmi160_interrupt)
GPIO(ISH_GPIO_1_IN,         PIN(1),  GPIO_INPUT)
GPIO(ISH_GPIO_2_OUT,        PIN(2),  GPIO_OUT_LOW)
GPIO(ISH_GPIO_4_IN,         PIN(4),  GPIO_INPUT)
GPIO(ISH_GPIO_5_OUT,        PIN(5),  GPIO_OUT_LOW)

UNIMPLEMENTED(ISH_GPIO_0) /*in RVP3, it's connected to RX, so not config in RVP3*/
UNIMPLEMENTED(ISH_GPIO_6)
UNIMPLEMENTED(ISH_GPIO_7)
/* end of example */

/* Those are used by common code, don't change*/
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(I2C0_SCL)
UNIMPLEMENTED(I2C0_SDA)
