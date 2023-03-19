/* -*- mode:c -*-
 *
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

GPIO_INT(ACCEL_INT_L, PIN(3), GPIO_SEL_1P8V | GPIO_INT_FALLING, lis2dw12_interrupt)
GPIO_INT(IMU_INT_L, PIN(5), GPIO_SEL_1P8V | GPIO_INT_FALLING, lsm6dso_interrupt)
GPIO_INT(ALS_INT_L, PIN(8), GPIO_INT_FALLING, tcs3400_interrupt)
/*
 * We don't have a ENTERING_RW signal wired to the cr50 but common code needs
 * it to be defined.
 */
UNIMPLEMENTED(ENTERING_RW)
