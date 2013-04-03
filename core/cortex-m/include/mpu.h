/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MPU module for Cortex-M3 */

#ifndef __CROS_EC_MPU_H
#define __CROS_EC_MPU_H

#include "common.h"

#define MPU_TYPE		REG32(0xe000ed90)
#define MPU_CTRL		REG32(0xe000ed94)
#define MPU_NUMBER		REG32(0xe000ed98)
#define MPU_BASE		REG32(0xe000ed9c)
#define MPU_SIZE		REG16(0xe000eda0)
#define MPU_ATTR		REG16(0xe000eda2)

#define MPU_CTRL_PRIVDEFEN	(1 << 2)
#define MPU_CTRL_HFNMIENA	(1 << 1)
#define MPU_CTRL_ENABLE		(1 << 0)
#define MPU_ATTR_NX		(1 << 12)
#define MPU_ATTR_NOACCESS	(0 << 8)
#define MPU_ATTR_FULLACCESS	(3 << 8)
/* Suggested in table 3-6 of Stellaris LM4F232H5QC Datasheet and tabel 38 of
 * STM32F10xxx Cortex-M3 programming manual for internal sram. */
#define MPU_ATTR_INTERNALSRAM	6

/* Updates a memory region.
 *
 * region: Number of the resion to update
 * addr: Base address of the region
 * size_bit: Size of the region in power of two.
 * attr: Attribute of the region. Current value will be overwritten if enable
 * is set.
 * enable: Enables the region if non zero. Otherwise, disables the region.
 *
 * Based on 3.1.4.1 'Updating an MPU Region' of Stellaris LM4F232H5QC Datasheet
 */
void mpu_update_region(uint8_t region, uint32_t addr, uint8_t size_bit,
		       uint16_t attr, uint8_t enable);


int mpu_config_region(uint8_t region, uint32_t addr, uint32_t size,
		      uint16_t attr, uint8_t enable);

int mpu_nx_region(uint8_t region, uint32_t addr, uint32_t size);

/* Enables MPU */
void mpu_enable(void);

/* Gets MPU Type */
uint32_t mpu_get_type(void);

/* Location of iram.text */
extern char __iram_text_start;
extern char __iram_text_end;

/* Locks down RAM */
int mpu_protect_ram(void);

/* Initializes MPU.
 * It disables all regionsi if MPU is implemented. Otherwise, returns
 * EC_ERROR_UNIMPLEMENTED.
 */
int mpu_init(void);

#endif /* __CROS_EC_MPU_H */
