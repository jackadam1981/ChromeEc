/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_MPU_CHIP_H
#define __CROS_EC_MPU_CHIP_H

/**
 * Configure lock region for NPCX
 */
int mpu_chip_config_lock_region(uint8_t region, uint32_t addr, uint32_t size);

#endif /* __CROS_EC_MPU_CHIP_H */
