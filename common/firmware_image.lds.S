/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#ifdef CONFIG_EXT_FLASH
#define FW_RO_OFF (CONFIG_RAM_BASE + CONFIG_RAM_CODE_RO_OFF)
#define FW_RW_OFF (CONFIG_RAM_BASE + CONFIG_RAM_CODE_RW_OFF)
#define ALIGN_SIZE 1
#define FW_END_OFF (CONFIG_RAM_BASE + CONFIG_RAM_CODE_SIZE)
#else
#define FW_RO_OFF (CONFIG_FLASH_BASE + CONFIG_FW_RO_OFF)
#define FW_RW_OFF (CONFIG_FLASH_BASE + CONFIG_FW_RW_OFF)
#define ALIGN_SIZE CONFIG_FLASH_BANK_SIZE
#define FW_END_OFF (CONFIG_FLASH_BASE + CONFIG_FLASH_SIZE)
#endif

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
MEMORY
{
#ifdef CONFIG_EXT_FLASH
    FLASH (rx) : ORIGIN = CONFIG_RAM_BASE, LENGTH = CONFIG_RAM_CODE_SIZE
#else
    FLASH (rx) : ORIGIN = CONFIG_FLASH_BASE, LENGTH = CONFIG_FLASH_SIZE
#endif
}
SECTIONS
{
    . = ALIGN(ALIGN_SIZE);
    .image.RO : AT(FW_RO_OFF) {
        *(.image.RO)
    } > FLASH =0xff
    . = ALIGN(ALIGN_SIZE);
    .image.RW : AT(FW_RW_OFF) {
        *(.image.RW)
        BYTE(0xEA)                      /* Mark end explicitly */
    } > FLASH =0xff
    .padding : AT(FW_END_OFF - 1) {
        BYTE(0xff);
    } > FLASH =0xff
}
