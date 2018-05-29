/* -*- mode:c -*-
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO(I2C0_SCL,            PIN(15),  GPIO_INPUT)
GPIO(I2C0_SDA,            PIN(16),  GPIO_INPUT)

/* Unimplemented signals since we are not an EC */
UNIMPLEMENTED(ENTERING_RW)

