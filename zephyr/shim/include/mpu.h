/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_MPU_H
#define __CROS_EC_MPU_H

/**
 * Returns the value of MPU type register
 *
 * @returns 0 for now (always)
 */
uint32_t mpu_get_type(void);

/** Protect RAM from code execution */
int mpu_protect_data_ram(void);

/** Protect code RAM from being overwritten */
int mpu_protect_code_ram(void);

#endif /* __CROS_EC_CPU_H */
