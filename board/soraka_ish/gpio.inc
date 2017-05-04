/* -*- mode:c -*-
 *
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C pins - these will be reconfigured for alternate function below */
GPIO(I2C0_SCL,            PIN(15),  GPIO_INPUT)
GPIO(I2C0_SDA,            PIN(16),  GPIO_INPUT)
GPIO(ENTERING_RW,         PIN(41),  GPIO_OUT_LOW)
