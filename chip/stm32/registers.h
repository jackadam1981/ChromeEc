/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Register map for the STM32 family of chips
 *
 * This header file should only contain register definitions and
 * functionality that are common to all STM32 chips.
 * Any chip/family specific macros must be placed in their family
 * specific registers file, which is conditionally included at the
 * end of this file.
 * Include this file directly for all STM32 register definitions.
 *
 * ### History and Reasoning ###
 * In a time before chip family register file separation,
 * long long ago, there lived a single file called `registers.h`,
 * which housed register definitions for all STM32 chip family and variants.
 * This poor file was 3000 lines of register macros and C definitions,
 * swiss-cheesed by nested preprocessor conditional logic.
 * Adding a single new chip variant required splitting multiple,
 * already nested, conditional sections throughout the file.
 * Readability was on the difficult side and refactoring was dangerous.
 *
 * The number of STM32 variants had outgrown the single registers file model.
 * The minor gains of sharing a set of registers between a subset of chip
 * variants no longer outweighed the complexity of the following operations:
 * - Adding a new chip variant or variant feature
 * - Determining if a register was properly setup for a variant or if it
 *   was simply not unset
 *
 * To strike a balance between shared registers and chip specific registers,
 * the registers.h file remains a place for common definitions, but family
 * specific definitions were moved to their own files.
 * These family specific files contain a much reduced level of preprocessor
 * logic for variant specific registers.
 *
 * See https://crrev.com/c/1674679 to witness the separation steps.
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"
#include "compile_time_macros.h"

#if defined(CHIP_FAMILY_STM32F0)
#include "registers-stm32f0.h"
#elif defined(CHIP_FAMILY_STM32F3)
#include "registers-stm32f3.h"
#elif defined(CHIP_FAMILY_STM32F4)
#include "registers-stm32f4.h"
#elif defined(CHIP_FAMILY_STM32F7)
#include "registers-stm32f7.h"
#elif defined(CHIP_FAMILY_STM32G0)
#include "registers-stm32g0.h"
#elif defined(CHIP_FAMILY_STM32H7)
#include "registers-stm32h7.h"
#elif defined(CHIP_FAMILY_STM32L)
#include "registers-stm32l.h"
#elif defined(CHIP_FAMILY_STM32L4)
#include "registers-stm32l4.h"
#else
#error "Unsupported chip family"
#endif

#endif /* __CROS_EC_REGISTERS_H */
