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

/* As written on table 3-5 of Stellaris LM4F232H5QC Datasheet */
#define MPU_ATTR_NX		(1 << 12)
#define MPU_ATTR_NO_NO		(0 << 8)
#define MPU_ATTR_RW_RW		(3 << 8)
#define MPU_ATTR_RO_NO		(5 << 8)

/* Suggested in table 3-6 of Stellaris LM4F232H5QC Datasheet and table 38 of
 * STM32F10xxx Cortex-M3 programming manual for internal sram. */
#define MPU_ATTR_INTERNAL_SRAM	6
#define MPU_ATTR_FLASH_MEMORY   2

/**
 * Enable MPU
 */
void mpu_enable(void);

/**
 * Returns the value of MPU type register
 *
 * Bit fields:
 * [15:8] Number of the data regions implemented or 0 if MPU is not present.
 * [1]    0: unified (no distinction between instruction and data)
 *        1: separated
 */
uint32_t mpu_get_type(void);

/* Location of iram.text */
extern char __iram_text_start;
extern char __iram_text_end;

/**
 * Protect RAM from code execution
 */
int mpu_protect_ram(void);

/**
 * Protect flash memory from code execution
 */
int mpu_lock_ro_flash(void);
int mpu_lock_rw_flash(void);

/**
 * Initialize MPU.
 * It disables all regions if MPU is implemented. Otherwise, returns
 * EC_ERROR_UNIMPLEMENTED.
 */
int mpu_pre_init(void);

#endif /* __CROS_EC_MPU_H */
