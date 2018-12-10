/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Trackpad interrupt pin */
GPIO_INT(TOUCHPAD_INT, PIN(1), GPIO_INT_F_FALLING, tp_handler)

/* Those are used by common code, don't change*/
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(ISH_I2C0_SDA)
UNIMPLEMENTED(ISH_I2C0_SCL)
